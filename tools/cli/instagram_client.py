"""
Instagram MCP Client
Connects to the Instagram MCP HTTP server and fetches posts.

Requirements:
    pip install requests

Usage:
    python instagram_client.py --server http://<server-ip>:3000 --username <instagram_username>
    python instagram_client.py --server http://<server-ip>:3000 --username <instagram_username> --limit 5
    python instagram_client.py --server http://<server-ip>:3000 --username <instagram_username> --limit all
"""

import argparse
import json
import sys
import threading
import time

# Force unbuffered output so progress is visible in all terminals
sys.stdout.reconfigure(line_buffering=True)

try:
    import requests
except ImportError:
    print("Missing dependency. Run:  pip install requests")
    sys.exit(1)


class MCPClient:
    """Minimal MCP client over HTTP/SSE transport."""

    def __init__(self, server_url: str):
        self.server_url = server_url.rstrip("/")
        self._post_url: str | None = None
        self._msg_id = 0
        self._lock = threading.Lock()
        self._pending: dict[int, tuple[threading.Event, list]] = {}
        self._connected = threading.Event()
        self._sse_thread: threading.Thread | None = None
        self._sse_response = None
        self._sse_error: Exception | None = None

    # ------------------------------------------------------------------ #
    # Connection
    # ------------------------------------------------------------------ #

    def connect(self, timeout: float = 10.0) -> None:
        """Open the SSE stream and wait until the endpoint event arrives."""
        self._sse_thread = threading.Thread(target=self._listen_sse, daemon=True)
        self._sse_thread.start()
        if not self._connected.wait(timeout=timeout):
            raise TimeoutError(
                f"Could not connect to MCP server at {self.server_url} within {timeout}s.\n"
                f"  • Make sure the server is running on the Windows machine.\n"
                f"  • Use the Windows machine's local IP, not 'localhost'.\n"
                f"    Example: python instagram_client.py --server http://192.168.1.x:3000"
            )
        if self._sse_error is not None:
            raise ConnectionError(
                f"Failed to connect to {self.server_url}:\n  {self._sse_error}\n\n"
                f"  • Do NOT use 'localhost' from another device — use the server's local IP.\n"
                f"  • Find the IP on the Windows machine with:  ipconfig\n"
                f"    Example: python instagram_client.py --server http://192.168.1.x:3000"
            ) from self._sse_error

    def close(self) -> None:
        if self._sse_response:
            self._sse_response.close()

    # ------------------------------------------------------------------ #
    # SSE listener (background thread)
    # ------------------------------------------------------------------ #

    def _listen_sse(self) -> None:
        try:
            with requests.get(f"{self.server_url}/sse", stream=True, timeout=None) as resp:
                resp.raise_for_status()
                self._sse_response = resp

                event_type: str | None = None
                data_lines: list[str] = []

                for raw in resp.iter_lines(decode_unicode=True):
                    line: str = raw if isinstance(raw, str) else raw.decode()

                    if line.startswith("event:"):
                        event_type = line[6:].strip()
                    elif line.startswith("data:"):
                        data_lines.append(line[5:].strip())
                    elif line == "":
                        data = "\n".join(data_lines)
                        self._handle_sse_event(event_type, data)
                        event_type = None
                        data_lines = []

        except Exception as exc:
            self._sse_error = exc
            # Wake up any callers waiting on _connected so they get the error
            if not self._connected.is_set():
                self._connected.set()
            # Wake up any pending requests
            for event, holder in list(self._pending.values()):
                holder.append({"error": str(exc)})
                event.set()

    def _handle_sse_event(self, event_type: str | None, data: str) -> None:
        if event_type == "endpoint":
            # data looks like:  /message?sessionId=<uuid>
            self._post_url = (
                f"{self.server_url}{data}" if data.startswith("/") else data
            )
            self._connected.set()

        elif event_type == "message":
            try:
                msg = json.loads(data)
            except json.JSONDecodeError:
                return
            msg_id = msg.get("id")
            if msg_id in self._pending:
                event, holder = self._pending[msg_id]
                holder.append(msg)
                event.set()

    # ------------------------------------------------------------------ #
    # JSON-RPC helpers
    # ------------------------------------------------------------------ #

    def _next_id(self) -> int:
        with self._lock:
            self._msg_id += 1
            return self._msg_id

    def _post(self, message: dict) -> requests.Response:
        if not self._post_url:
            raise RuntimeError("Not connected — call connect() first")
        return requests.post(
            self._post_url,
            json=message,
            headers={"Content-Type": "application/json"},
            timeout=30,
        )

    def request(self, method: str, params: dict | None = None, timeout: float = 120.0) -> dict:
        """Send a JSON-RPC request and block until the response arrives."""
        msg_id = self._next_id()
        event = threading.Event()
        holder: list = []
        self._pending[msg_id] = (event, holder)

        message: dict = {"jsonrpc": "2.0", "id": msg_id, "method": method}
        if params:
            message["params"] = params

        resp = self._post(message)
        resp.raise_for_status()

        if not event.wait(timeout=timeout):
            self._pending.pop(msg_id, None)
            raise TimeoutError(f"No response for '{method}' within {timeout}s")

        self._pending.pop(msg_id, None)
        return holder[0]

    def notify(self, method: str, params: dict | None = None) -> None:
        """Send a JSON-RPC notification (no response expected)."""
        message: dict = {"jsonrpc": "2.0", "method": method}
        if params:
            message["params"] = params
        self._post(message)

    # ------------------------------------------------------------------ #
    # MCP protocol
    # ------------------------------------------------------------------ #

    def initialize(self) -> dict:
        result = self.request("initialize", {
            "protocolVersion": "2024-11-05",
            "capabilities": {},
            "clientInfo": {"name": "instagram-python-client", "version": "1.0.0"},
        })
        self.notify("notifications/initialized")
        return result

    def list_tools(self) -> list[dict]:
        resp = self.request("tools/list")
        return resp.get("result", {}).get("tools", [])

    def call_tool(self, name: str, arguments: dict, timeout: float = 120.0) -> dict:
        return self.request("tools/call", {"name": name, "arguments": arguments}, timeout=timeout)


# ------------------------------------------------------------------ #
# Output helpers
# ------------------------------------------------------------------ #

def print_posts(data: dict) -> None:
    posts = data.get("posts", [])
    pagination = data.get("pagination", {})

    if not posts:
        print("No posts returned.")
        return

    print(f"\n{'=' * 60}")
    print(f"  Fetched {len(posts)} post(s)")
    cb = pagination.get("currentBatch", {})
    print(f"  Batch : {cb.get('start', 0)} → {cb.get('end', len(posts))}")
    if pagination.get("hasMore"):
        print(f"  More  : yes  (next startFrom={pagination.get('nextStartFrom')})")
    print(f"{'=' * 60}\n")

    for i, post in enumerate(posts, 1):
        print(f"── Post {i} {'─' * 50}")
        print(f"  URL       : {post.get('postUrl', 'N/A')}")
        print(f"  Type      : {post.get('type', 'N/A')}")
        print(f"  Timestamp : {post.get('timestamp', 'N/A')}")
        caption = post.get("caption", "").strip()
        if caption:
            short = caption[:120] + ("…" if len(caption) > 120 else "")
            print(f"  Caption   : {short}")
        print(f"  Media URL : {post.get('mediaUrl', 'N/A')}")
        if post.get("videoUrl"):
            print(f"  Video URL : {post['videoUrl']}")
        print()


# ------------------------------------------------------------------ #
# CLI
# ------------------------------------------------------------------ #

def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Fetch Instagram posts via the MCP HTTP server.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  python instagram_client.py --server http://192.168.1.10:3000 --username nasa
  python instagram_client.py --server http://192.168.1.10:3000 --username nasa --limit 6
  python instagram_client.py --server http://192.168.1.10:3000 --username nasa --limit all
  python instagram_client.py --server http://192.168.1.10:3000 --username nasa --start-from 3
  python instagram_client.py --server http://192.168.1.10:3000 --list-tools
        """,
    )
    parser.add_argument(
        "--server",
        default="http://192.168.1.3:3000",
        help="MCP server URL (default: http://192.168.1.3:3000)",
    )
    parser.add_argument("--username", default="ssuunn268", help="Instagram username to fetch posts from (default: ssuunn268)")
    parser.add_argument(
        "--limit",
        default="3",
        help='Number of posts to fetch, or "all" (default: 3)',
    )
    parser.add_argument(
        "--start-from",
        type=int,
        default=0,
        help="Post index to start from, for pagination (default: 0)",
    )
    parser.add_argument(
        "--list-tools",
        action="store_true",
        help="List available tools on the server and exit",
    )
    parser.add_argument(
        "--raw",
        action="store_true",
        help="Print raw JSON response instead of formatted output",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()

    if not args.list_tools and not args.username:
        print("Error: --username is required (or use --list-tools)")
        sys.exit(1)


    print(f"Connecting to {args.server} …")
    client = MCPClient(args.server)

    try:
        client.connect(timeout=10)
        print("Connected. Initializing MCP session …")
        client.initialize()
        print("Session ready.\n")

        if args.list_tools:
            tools = client.list_tools()
            print(f"Available tools ({len(tools)}):")
            for tool in tools:
                print(f"  • {tool['name']} — {tool.get('description', '')}")
            return

        # Build arguments for get_instagram_posts
        limit: int | str = args.limit if args.limit == "all" else int(args.limit)
        tool_args: dict = {"username": args.username, "limit": limit}
        if args.start_from:
            tool_args["startFrom"] = args.start_from

        print(f"Fetching posts for @{args.username}  (limit={limit}) …")
        print("This may take 30–60 seconds while Chrome scrapes Instagram.\n")

        response = client.call_tool("get_instagram_posts", tool_args, timeout=180)

        if "error" in response:
            print(f"Server error: {response['error']}")
            sys.exit(1)

        result = response.get("result", {})
        content = result.get("content", [])

        if not content:
            print("Empty response from server.")
            sys.exit(1)

        first = content[0]

        # Handle error content
        if result.get("isError"):
            print(f"Tool error: {first.get('text', 'Unknown error')}")
            sys.exit(1)

        # Parse JSON content
        if first.get("type") == "json":
            data = first["json"]
        else:
            try:
                data = json.loads(first.get("text", "{}"))
            except json.JSONDecodeError:
                print(first.get("text", "No data"))
                return

        if args.raw:
            print(json.dumps(data, indent=2))
        else:
            print_posts(data)

    except TimeoutError as exc:
        print(f"\nTimeout: {exc}")
        sys.exit(1)
    except requests.ConnectionError:
        print(f"\nCould not reach the server at {args.server}")
        print("Make sure the MCP server is running and the IP/port are correct.")
        sys.exit(1)
    except KeyboardInterrupt:
        print("\nInterrupted.")
    finally:
        client.close()


if __name__ == "__main__":
    main()
