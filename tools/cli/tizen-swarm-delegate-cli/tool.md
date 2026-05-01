# tizen-swarm-delegate-cli
**Description**: Delegate a prompt to another TizenClaw agent discovered on the local swarm network (LAN). Supports targeting by device type (tv, refrigerator, oven) or direct IP address.

## Usage
```
tizen-swarm-delegate-cli --list-peers
tizen-swarm-delegate-cli --device-type <TYPE> --prompt <TEXT> [--bearer-token <TOKEN>]
tizen-swarm-delegate-cli --target-ip <IP>    --prompt <TEXT> [--bearer-token <TOKEN>]
```

## Arguments
| Argument | Required | Description |
|----------|----------|-------------|
| `--list-peers` | — | List all active swarm peers (no prompt needed) |
| `--device-type` | One of these | Target peer by device type (e.g. `tv`, `refrigerator`, `oven`) |
| `--target-ip` | One of these | Target peer by IP address directly |
| `--prompt` | Yes (delegate mode) | Natural-language instruction to send to the remote agent |
| `--bearer-token` | No | Bearer token if the target requires A2A authentication |

## How It Works
1. `--list-peers` queries `http://localhost:9090/api/swarm` to get the active peer list discovered via UDP broadcast.
2. `--device-type` resolves to the first peer matching that type, then sends an A2A `tasks/send` JSON-RPC 2.0 request to `http://<peer-ip>:9090/api/a2a`.
3. `--target-ip` skips peer resolution and calls the target directly.

## Example Output — list-peers
```json
{
  "peers": [
    {
      "ip": "192.168.1.42",
      "device_type": "tv",
      "capabilities": ["device_control", "code_execution"]
    }
  ]
}
```

## Example Output — delegate
```json
{
  "success": true,
  "task_id": "a2a-18f3c2a1-0",
  "target_ip": "192.168.1.42",
  "answer": "Brightness has been set to 70%."
}
```

## Example Output — error
```json
{
  "success": false,
  "task_id": "",
  "target_ip": "",
  "error": "no peer found with device_type: tv"
}
```
