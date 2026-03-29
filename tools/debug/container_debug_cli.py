#!/usr/bin/env python3
"""
TizenClaw Container Debug CLI — Directly communicates with skill_executor
via UDS socket for debugging skill execution inside the secure container.

Protocol: Length-prefixed JSON (4-byte big-endian + UTF-8 JSON)
Socket:   /tmp/tizenclaw_skill.sock

Usage (inside container via crun exec, or on host if socket is shared):
  python3 container_debug_cli.py diag
  python3 container_debug_cli.py skill <name> [args_json]
  python3 container_debug_cli.py exec "<python_code>"
  python3 container_debug_cli.py file <op> <path> [content]
  python3 container_debug_cli.py raw '<json_payload>'
"""
import json
import os
import socket
import struct
import sys

SOCKET_PATH = "/tmp/tizenclaw_skill.sock"
TIMEOUT = 30


def send_recv(req_dict, sock_path=SOCKET_PATH):
    """Send a request and receive a response via UDS."""
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    sock.settimeout(TIMEOUT)
    try:
        sock.connect(sock_path)
    except (ConnectionRefusedError, FileNotFoundError) as e:
        return {"_error": f"Cannot connect to {sock_path}: {e}"}

    payload = json.dumps(req_dict).encode("utf-8")
    header = struct.pack("!I", len(payload))
    sock.sendall(header + payload)

    # Read 4-byte response header
    resp_hdr = b""
    while len(resp_hdr) < 4:
        chunk = sock.recv(4 - len(resp_hdr))
        if not chunk:
            return {"_error": "Connection closed while reading header"}
        resp_hdr += chunk

    resp_len = struct.unpack("!I", resp_hdr)[0]
    if resp_len > 10 * 1024 * 1024:
        return {"_error": f"Response too large: {resp_len}"}

    # Read response body
    resp_buf = b""
    while len(resp_buf) < resp_len:
        chunk = sock.recv(resp_len - len(resp_buf))
        if not chunk:
            break
        resp_buf += chunk

    sock.close()
    return json.loads(resp_buf.decode("utf-8"))


def cmd_diag():
    """Run comprehensive diagnostics inside the container."""
    diag_code = r"""
import sys, os, json

info = {}
info["python_version"] = sys.version
info["sys_executable"] = sys.executable
info["sys_path"] = sys.path
info["cwd"] = os.getcwd()
info["pid"] = os.getpid()
info["env_PATH"] = os.environ.get("PATH", "")
info["env_LD_LIBRARY_PATH"] = os.environ.get("LD_LIBRARY_PATH", "")
info["env_PYTHONPATH"] = os.environ.get("PYTHONPATH", "")

# Check key paths
paths_to_check = [
    "/usr/bin/python3",
    "/usr/lib/python3.12",
    "/usr/lib/libpython3.12.so.1.0",
    "/skills/skill_executor.py",
    "/skills/common/tizen_capi_utils.py",
    "/skills/list_apps/list_apps.py",
    "/skills/list_apps/manifest.json",
    "/host_lib/libc.so.6",
    "/usr/lib/libffi.so.8",
    "/lib/ld-musl-x86_64.so.1",
    "/lib/ld-musl-aarch64.so.1",
    "/lib/ld-musl-armhf.so.1",
]
info["path_exists"] = {p: os.path.exists(p) for p in paths_to_check}

# Check /usr/bin contents
try:
    bins = [f for f in os.listdir("/usr/bin") if "python" in f.lower()]
    info["usr_bin_python"] = bins
except Exception as e:
    info["usr_bin_python_error"] = str(e)

# Check /skills listing
try:
    info["skills_listing"] = os.listdir("/skills")
except Exception as e:
    info["skills_listing_error"] = str(e)

# Try importing ctypes
try:
    import ctypes
    info["ctypes_import"] = "ok"
except ImportError as e:
    info["ctypes_import_error"] = str(e)

# Try loading a CAPI library
try:
    import ctypes
    lib = ctypes.CDLL("libcapi-appfw-app-manager.so.0")
    info["capi_appfw_load"] = "ok"
except Exception as e:
    info["capi_appfw_load_error"] = str(e)

# Read a manifest
try:
    with open("/skills/list_apps/manifest.json") as f:
        info["list_apps_manifest"] = json.load(f)
except Exception as e:
    info["list_apps_manifest_error"] = str(e)

print(json.dumps(info, indent=2))
"""
    return send_recv({"command": "execute_code", "code": diag_code, "timeout": 15})


def cmd_skill(name, args_json="{}"):
    """Execute a skill by name."""
    return send_recv({"skill": name, "args": args_json})


def cmd_exec(code):
    """Execute arbitrary Python code."""
    return send_recv({"command": "execute_code", "code": code, "timeout": 15})


def cmd_file(operation, path, content=""):
    """Execute a file manager operation."""
    req = {"command": "file_manager", "operation": operation, "path": path}
    if content:
        req["content"] = content
    return send_recv(req)


def cmd_raw(payload_json):
    """Send raw JSON payload."""
    return send_recv(json.loads(payload_json))


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)

    cmd = sys.argv[1]

    if cmd == "diag":
        result = cmd_diag()
    elif cmd == "skill":
        if len(sys.argv) < 3:
            print("Usage: ... skill <name> [args_json]")
            sys.exit(1)
        name = sys.argv[2]
        args = sys.argv[3] if len(sys.argv) > 3 else "{}"
        result = cmd_skill(name, args)
    elif cmd == "exec":
        if len(sys.argv) < 3:
            print("Usage: ... exec '<python_code>'")
            sys.exit(1)
        code = sys.argv[2]
        result = cmd_exec(code)
    elif cmd == "file":
        if len(sys.argv) < 4:
            print("Usage: ... file <op> <path> [content]")
            sys.exit(1)
        op = sys.argv[2]
        path = sys.argv[3]
        content = sys.argv[4] if len(sys.argv) > 4 else ""
        result = cmd_file(op, path, content)
    elif cmd == "raw":
        if len(sys.argv) < 3:
            print("Usage: ... raw '<json_payload>'")
            sys.exit(1)
        result = cmd_raw(sys.argv[2])
    else:
        print(f"Unknown command: {cmd}")
        print(__doc__)
        sys.exit(1)

    print(json.dumps(result, indent=2, ensure_ascii=False))


if __name__ == "__main__":
    main()
