// TizenClawRawClient.cs
// Speaks directly to \0tizenclaw.sock (abstract UDS)
// Protocol (from tizenclaw.cc):
//   Send:    4-byte big-endian length  +  JSON-RPC 2.0 body
//   Receive: 4-byte big-endian length  +  JSON body (repeating for stream)
//
// Streaming frames:  {"jsonrpc":"2.0","method":"stream_chunk","params":{"text":"..."}}
// Final frame:       {"jsonrpc":"2.0","id":1,"result":{"text":"..."}}
// Error frame:       {"jsonrpc":"2.0","id":1,"error":{"code":-32602,"message":"..."}}

using System;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;

namespace MyNuiApp
{
    public class TizenClawRawClient : IDisposable
    {
        // Abstract namespace socket — daemon binds \0tizenclaw.sock
        // .NET uses "@" prefix to mean abstract namespace
        private const string SocketAddress = "@tizenclaw.sock";

        private int _nextId = 1;
        private bool _disposed;

        // -------------------------------------------------------
        // Single shot: get full response at once
        // -------------------------------------------------------
        public async Task<string> SendRequestAsync(
            string sessionId,
            string prompt,
            CancellationToken ct = default)
        {
            using var sock = await ConnectAsync(ct);

            await SendFrameAsync(sock, BuildRequest(sessionId, prompt, stream: false), ct);

            string frame = await ReadFrameAsync(sock, ct);
            return ParseResult(frame);
        }

        // -------------------------------------------------------
        // Streaming: onChunk called for each token,
        // isDone=true on the final frame
        // -------------------------------------------------------
        public async Task SendRequestStreamAsync(
            string sessionId,
            string prompt,
            Action<string, bool> onChunk,
            CancellationToken ct = default)
        {
            using var sock = await ConnectAsync(ct);

            await SendFrameAsync(sock, BuildRequest(sessionId, prompt, stream: true), ct);

            while (!ct.IsCancellationRequested)
            {
                string frame = await ReadFrameAsync(sock, ct);

                if (string.IsNullOrEmpty(frame))
                    break;

                using var doc = JsonDocument.Parse(frame);
                var root = doc.RootElement;

                // Streaming chunk: {"method":"stream_chunk","params":{"text":"..."}}
                if (root.TryGetProperty("method", out var method) &&
                    method.GetString() == "stream_chunk")
                {
                    string chunk = root
                        .GetProperty("params")
                        .GetProperty("text")
                        .GetString() ?? "";
                    onChunk(chunk, false);
                    continue;
                }

                // Final frame: {"result":{"text":"..."}} or {"error":{...}}
                if (root.TryGetProperty("result", out _) ||
                    root.TryGetProperty("error", out _))
                {
                    string finalText = ParseResult(frame);
                    onChunk(finalText, true);   // isDone = true
                    break;
                }
            }
        }

        // -------------------------------------------------------
        // Connect to abstract UDS \0tizenclaw.sock
        // -------------------------------------------------------
        private static async Task<Socket> ConnectAsync(CancellationToken ct)
        {
            var sock = new Socket(
                AddressFamily.Unix,
                SocketType.Stream,
                ProtocolType.Unspecified);
            try
            {
                // "@" prefix = abstract namespace in .NET's UnixDomainSocketEndPoint
                var ep = new UnixDomainSocketEndPoint(SocketAddress);
                await sock.ConnectAsync(ep, ct);
                return sock;
            }
            catch
            {
                sock.Dispose();
                throw;
            }
        }

        // -------------------------------------------------------
        // Wire protocol: send 4-byte big-endian length then body
        // Matches tizenclaw_client.cc:208-217
        // -------------------------------------------------------
        private static async Task SendFrameAsync(
            Socket sock, string json, CancellationToken ct)
        {
            byte[] body   = Encoding.UTF8.GetBytes(json);
            byte[] lenBuf = BitConverter.GetBytes(
                IPAddress.HostToNetworkOrder(body.Length));   // big-endian

            await sock.SendAsync(lenBuf, SocketFlags.None, ct);
            await sock.SendAsync(body,   SocketFlags.None, ct);
        }

        // -------------------------------------------------------
        // Wire protocol: read 4-byte big-endian length then body
        // Matches tizenclaw.cc:454-473
        // -------------------------------------------------------
        private static async Task<string> ReadFrameAsync(
            Socket sock, CancellationToken ct)
        {
            // 1. Read 4-byte length prefix
            byte[] lenBuf = new byte[4];
            await RecvExactAsync(sock, lenBuf, 4, ct);
            int len = IPAddress.NetworkToHostOrder(
                BitConverter.ToInt32(lenBuf, 0));

            if (len <= 0 || len > 10 * 1024 * 1024)
                throw new InvalidOperationException(
                    $"Invalid frame length: {len}");

            // 2. Read body
            byte[] body = new byte[len];
            await RecvExactAsync(sock, body, len, ct);
            return Encoding.UTF8.GetString(body);
        }

        // -------------------------------------------------------
        // Guarantee we read exactly `count` bytes
        // -------------------------------------------------------
        private static async Task RecvExactAsync(
            Socket sock, byte[] buf, int count, CancellationToken ct)
        {
            int received = 0;
            while (received < count)
            {
                int n = await sock.ReceiveAsync(
                    new ArraySegment<byte>(buf, received, count - received),
                    SocketFlags.None, ct);

                if (n == 0)
                    throw new InvalidOperationException(
                        "Connection closed by daemon");
                received += n;
            }
        }

        // -------------------------------------------------------
        // Build JSON-RPC 2.0 request
        // Matches tizenclaw_client.cc:197-206
        // -------------------------------------------------------
        private string BuildRequest(string sessionId, string prompt, bool stream)
        {
            int id = Interlocked.Increment(ref _nextId);
            return JsonSerializer.Serialize(new
            {
                jsonrpc = "2.0",
                method  = "prompt",
                id      = id,
                @params = new
                {
                    session_id = sessionId,
                    text       = prompt,
                    stream     = stream,
                }
            });
        }

        // -------------------------------------------------------
        // Extract text from final frame
        // Final frame from tizenclaw.cc:591-593:
        //   {"jsonrpc":"2.0","id":1,"result":{"text":"<response>"}}
        // -------------------------------------------------------
        private static string ParseResult(string frame)
        {
            try
            {
                using var doc  = JsonDocument.Parse(frame);
                var root = doc.RootElement;

                // Success: result.text
                if (root.TryGetProperty("result", out var result) &&
                    result.TryGetProperty("text", out var text))
                    return text.GetString() ?? "";

                // Error: error.message
                if (root.TryGetProperty("error", out var error) &&
                    error.TryGetProperty("message", out var msg))
                    throw new InvalidOperationException(
                        $"Daemon error: {msg.GetString()}");
            }
            catch (JsonException ex)
            {
                throw new InvalidOperationException(
                    $"Bad response JSON: {ex.Message}");
            }

            return frame;   // fallback: return raw
        }

        public void Dispose()
        {
            _disposed = true;
        }
    }
}
