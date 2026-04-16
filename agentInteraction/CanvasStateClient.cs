using System;
using System.IO;
using System.Net.Sockets;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;

namespace MyNuiApp
{
    public enum AgentState
    {
        Unknown,
        Idle,       // "idle"     — ready / response complete
        Thinking,   // "thinking" — LLM is processing
        ToolCall,   // "tool_call"— executing tools
    }

    public class CanvasEvent
    {
        public AgentState State    { get; set; }
        public string     StateStr { get; set; }
        public string     Content  { get; set; }
    }

    public class ToolResultEvent
    {
        public string ToolName  { get; set; }
        public string SessionId { get; set; }
        // Raw JSON result serialized as a string for flexible UI rendering
        public string ResultJson { get; set; }
    }

    /// <summary>
    /// Carries a structured A2UI descriptor produced by the A2UIAgent from a
    /// raw tool result. The NUI application renders this directly using the
    /// <see cref="A2UIResultEvent.A2UIJson"/> payload without any per-tool logic.
    /// </summary>
    public class A2UIResultEvent
    {
        public string ToolName  { get; set; }
        public string SessionId { get; set; }
        // Full a2ui JSON descriptor as a raw string.
        // Schema: { type, title, data, actions? }
        public string A2UIJson  { get; set; }
    }

    /// <summary>
    /// Connects to /run/tizenclaw/canvas.sock and receives
    /// agent state broadcasts (server → client, newline-delimited JSON).
    ///
    /// On connect the server immediately sends:
    ///   {"type":"state","state":"idle","content":"TizenClaw Engine Connected"}
    /// </summary>
    public class CanvasStateClient : IDisposable
    {
        private const string SocketPath = "/run/tizenclaw/canvas.sock";

        public event Action<CanvasEvent>      OnStateChanged;
        public event Action<ToolResultEvent>  OnToolResult;
        // Fired when A2UIAgent has post-processed a tool result into a typed
        // UI descriptor. Only raised for sessions running in a2ui mode.
        public event Action<A2UIResultEvent>  OnA2UIResult;
        public event Action                   OnDisconnected;

        private Socket            _socket;
        private CancellationTokenSource _cts;
        private Task              _recvTask;
        private bool              _disposed;

        public bool IsConnected => _socket?.Connected == true;

        public async Task<bool> ConnectAsync()
        {
            try
            {
                _socket = new Socket(
                    AddressFamily.Unix,
                    SocketType.Stream,
                    ProtocolType.Unspecified);

                var endpoint = new UnixDomainSocketEndPoint(SocketPath);
                await _socket.ConnectAsync(endpoint);

                _cts = new CancellationTokenSource();
                // Start background receive loop
                _recvTask = Task.Run(
                    () => ReceiveLoopAsync(_cts.Token));

                return true;
            }
            catch (Exception ex)
            {
                Tizen.Log.Error("MYAPP",
                    $"CanvasStateClient connect failed: {ex.Message}");
                return false;
            }
        }

        public void Disconnect()
        {
            _cts?.Cancel();
            try { _socket?.Shutdown(SocketShutdown.Both); } catch { }
            try { _socket?.Close(); } catch { }
        }

        // -------------------------------------------------------
        // Background receive loop — reads newline-delimited JSON
        // -------------------------------------------------------
        private async Task ReceiveLoopAsync(CancellationToken ct)
        {
            var buffer    = new byte[1024];
            var remainder = new StringBuilder();

            try
            {
                while (!ct.IsCancellationRequested)
                {
                    int n = await _socket.ReceiveAsync(
                        new ArraySegment<byte>(buffer),
                        SocketFlags.None);

                    if (n <= 0)
                    {
                        Tizen.Log.Warn("MYAPP",
                            "CanvasStateClient: server closed connection");
                        break;
                    }

                    remainder.Append(
                        Encoding.UTF8.GetString(buffer, 0, n));

                    // Process every complete \n-delimited message
                    string accum = remainder.ToString();
                    int pos;
                    while ((pos = accum.IndexOf('\n')) >= 0)
                    {
                        string line = accum.Substring(0, pos).Trim();
                        accum = accum.Substring(pos + 1);

                        if (line.Length > 0)
                            ProcessMessage(line);
                    }
                    remainder.Clear();
                    remainder.Append(accum);
                }
            }
            catch (OperationCanceledException) { }
            catch (Exception ex)
            {
                Tizen.Log.Error("MYAPP",
                    $"CanvasStateClient recv error: {ex.Message}");
            }

            OnDisconnected?.Invoke();
        }

        private void ProcessMessage(string json)
        {
            try
            {
                using var doc = JsonDocument.Parse(json);
                var root = doc.RootElement;

                if (!root.TryGetProperty("type", out var typeProp))
                    return;

                string type = typeProp.GetString();

                if (type == "state")
                {
                    string stateStr = root
                        .TryGetProperty("state", out var sp)
                        ? sp.GetString() : "";
                    string content = root
                        .TryGetProperty("content", out var cp)
                        ? cp.GetString() : "";

                    var ev = new CanvasEvent
                    {
                        StateStr = stateStr,
                        Content  = content,
                        State    = ParseState(stateStr),
                    };

                    Tizen.Log.Info("MYAPP",
                        $"Canvas state: {stateStr} — {content}");

                    OnStateChanged?.Invoke(ev);
                }
                else if (type == "tool_result")
                {
                    string toolName = root
                        .TryGetProperty("tool_name", out var tn)
                        ? tn.GetString() : "";
                    string sessionId = root
                        .TryGetProperty("session_id", out var si)
                        ? si.GetString() : "";
                    string resultJson = root
                        .TryGetProperty("result", out var rp)
                        ? rp.GetRawText() : "{}";

                    var ev = new ToolResultEvent
                    {
                        ToolName   = toolName,
                        SessionId  = sessionId,
                        ResultJson = resultJson,
                    };

                    Tizen.Log.Info("MYAPP",
                        $"Tool result: {toolName} → {resultJson.Substring(0, Math.Min(120, resultJson.Length))}");

                    OnToolResult?.Invoke(ev);
                }
                else if (type == "a2ui_result")
                {
                    string toolName = root
                        .TryGetProperty("tool_name", out var atn)
                        ? atn.GetString() : "";
                    string sessionId = root
                        .TryGetProperty("session_id", out var asi)
                        ? asi.GetString() : "";
                    string a2uiJson = root
                        .TryGetProperty("a2ui", out var ajp)
                        ? ajp.GetRawText() : "{}";

                    var ev = new A2UIResultEvent
                    {
                        ToolName  = toolName,
                        SessionId = sessionId,
                        A2UIJson  = a2uiJson,
                    };

                    Tizen.Log.Info("MYAPP",
                        $"A2UI result: {toolName} → {a2uiJson.Substring(0, Math.Min(120, a2uiJson.Length))}");

                    OnA2UIResult?.Invoke(ev);
                }
            }
            catch (JsonException ex)
            {
                Tizen.Log.Warn("MYAPP",
                    $"CanvasStateClient JSON error: {ex.Message}");
            }
        }

        private static AgentState ParseState(string s) => s switch
        {
            "idle"      => AgentState.Idle,
            "thinking"  => AgentState.Thinking,
            "tool_call" => AgentState.ToolCall,
            _           => AgentState.Unknown,
        };

        public void Dispose()
        {
            if (!_disposed)
            {
                Disconnect();
                _socket?.Dispose();
                _cts?.Dispose();
                _disposed = true;
            }
        }
    }
}
