#!/usr/bin/env python3
"""
tizenclaw_code_executor.py — Runs inside the secure container.

Listens on an abstract namespace Unix domain socket and executes
LLM-generated Python/shell code in an isolated environment.
Supports pip/npm package installation to a persistent /packages directory.

Protocol: 4-byte big-endian length prefix + UTF-8 JSON body.
"""

import io
import json
import os
import socket
import struct
import subprocess
import sys
import traceback

SOCKET_NAME = "tizenclaw-code-sandbox.sock"
PACKAGES_DIR = "/packages"
PIP_TARGET = os.path.join(PACKAGES_DIR, "pip")
NPM_PREFIX = os.path.join(PACKAGES_DIR, "npm")


def setup_package_paths():
    """Ensure package directories exist and are on the search path."""
    os.makedirs(PIP_TARGET, exist_ok=True)
    os.makedirs(NPM_PREFIX, exist_ok=True)

    # Add pip target to Python path so imports work immediately
    if PIP_TARGET not in sys.path:
        sys.path.insert(0, PIP_TARGET)

    os.environ.setdefault("PYTHONPATH", PIP_TARGET)
    os.environ.setdefault("PIP_TARGET", PIP_TARGET)
    os.environ.setdefault("NPM_CONFIG_PREFIX", NPM_PREFIX)
    os.environ.setdefault(
        "NODE_PATH", os.path.join(NPM_PREFIX, "lib", "node_modules")
    )


def execute_python_code(code: str, timeout: int = 15) -> dict:
    """Execute Python code and capture stdout/stderr."""
    buf = io.StringIO()
    old_stdout, old_stderr = sys.stdout, sys.stderr
    sys.stdout = sys.stderr = buf

    rc = 0
    try:
        exec(code, {"__builtins__": __builtins__})
    except Exception:
        traceback.print_exc(file=buf)
        rc = 1
    finally:
        sys.stdout, sys.stderr = old_stdout, old_stderr

    output = buf.getvalue()
    return {"status": "ok" if rc == 0 else "error", "output": output}


def install_package(pkg_type: str, name: str) -> dict:
    """Install a pip or npm package to the persistent /packages dir."""
    try:
        if pkg_type == "pip":
            cmd = [
                sys.executable, "-m", "pip", "install",
                "--target", PIP_TARGET,
                "--no-warn-script-location",
                name,
            ]
        elif pkg_type == "npm":
            cmd = [
                "npm", "install",
                "--prefix", NPM_PREFIX,
                name,
            ]
        else:
            return {"status": "error", "output": f"Unknown type: {pkg_type}"}

        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=120,
        )
        output = result.stdout + result.stderr
        if result.returncode == 0:
            return {"status": "ok", "output": output}
        return {"status": "error", "output": output}
    except subprocess.TimeoutExpired:
        return {"status": "error", "output": "Install timed out (120s)"}
    except Exception as e:
        return {"status": "error", "output": str(e)}


def handle_diag() -> dict:
    """Return diagnostic info about the container environment."""
    import platform

    installed = []
    try:
        result = subprocess.run(
            [sys.executable, "-m", "pip", "list", "--path", PIP_TARGET,
             "--format", "json"],
            capture_output=True, text=True, timeout=10,
        )
        if result.returncode == 0:
            installed = json.loads(result.stdout)
    except Exception:
        pass

    diag = {
        "python_version": platform.python_version(),
        "platform": platform.platform(),
        "arch": platform.machine(),
        "packages_dir": PACKAGES_DIR,
        "pip_target": PIP_TARGET,
        "npm_prefix": NPM_PREFIX,
        "pip_packages": installed,
        "sys_path": sys.path[:5],
    }
    return {"status": "ok", "output": json.dumps(diag)}


def handle_request(data: bytes) -> bytes:
    """Parse JSON request, dispatch, return JSON response."""
    try:
        req = json.loads(data.decode("utf-8"))
    except Exception as e:
        resp = {"status": "error", "output": f"Bad JSON: {e}"}
        return json.dumps(resp).encode("utf-8")

    command = req.get("command", "")

    if command == "execute_code":
        code = req.get("code", "")
        timeout = req.get("timeout", 15)
        if not code:
            resp = {"status": "error", "output": "No code provided"}
        else:
            resp = execute_python_code(code, timeout)
    elif command == "install_package":
        pkg_type = req.get("type", "pip")
        name = req.get("name", "")
        if not name:
            resp = {"status": "error", "output": "No package name"}
        else:
            resp = install_package(pkg_type, name)
    elif command == "diag":
        resp = handle_diag()
    else:
        resp = {"status": "error", "output": f"Unknown command: {command}"}

    return json.dumps(resp).encode("utf-8")


def recv_exact(conn, n):
    """Receive exactly n bytes."""
    data = b""
    while len(data) < n:
        chunk = conn.recv(n - len(data))
        if not chunk:
            return None
        data += chunk
    return data


def serve():
    """Main server loop on abstract namespace socket."""
    setup_package_paths()

    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    # Abstract namespace: prepend null byte
    addr = "\0" + SOCKET_NAME
    sock.bind(addr)
    sock.listen(5)
    print(f"[code_executor] Listening on @{SOCKET_NAME}", flush=True)

    while True:
        conn, _ = sock.accept()
        try:
            while True:
                # Read 4-byte length header
                hdr = recv_exact(conn, 4)
                if not hdr:
                    break
                payload_len = struct.unpack("!I", hdr)[0]
                if payload_len > 10 * 1024 * 1024:
                    break

                # Read payload
                payload = recv_exact(conn, payload_len)
                if not payload:
                    break

                # Handle request
                resp_data = handle_request(payload)

                # Send length-prefixed response
                resp_hdr = struct.pack("!I", len(resp_data))
                conn.sendall(resp_hdr + resp_data)
        except Exception as e:
            print(f"[code_executor] Client error: {e}", flush=True)
        finally:
            conn.close()


if __name__ == "__main__":
    serve()
