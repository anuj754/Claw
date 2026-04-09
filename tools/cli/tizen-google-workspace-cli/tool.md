# tizen-google-workspace-cli
**Description**: Interact natively with Gmail to read, list, and send emails using the Google Workspace REST API.

## Usage
### List recent emails
```bash
tizen-google-workspace-cli --action list [--query <QUERY>]
```
Example: `--query "is:unread"`

### Read a specific email by ID
```bash
tizen-google-workspace-cli --action read --id <MSG_ID>
```

### Send an email
```bash
tizen-google-workspace-cli --action send --to <EMAIL> --subject <SUBJ> --message <BODY>
```

## Arguments
| Argument | Required | Description |
|----------|----------|-------------|
| `--action` | Yes | The operation to perform: `list`, `read`, or `send` |
| `--query` | No | Search query string (only used for `list`) |
| `--id` | Yes for `read` | Gmail Message ID to read |
| `--to` | Yes for `send` | Recipient email address |
| `--subject` | Yes for `send`| Email subject |
| `--message` | Yes for `send`| Email body |

## Configuration
Requires a valid OAuth 2.0 `access_token` stored in: `/opt/usr/share/tizenclaw/config/google_workspace_config.json`.
Example:
```json
{
  "access_token": "ya29.a0AfB_by..."
}
```

## Example Output (List)
```json
{
  "messages": [
    {"id": "18c2b53..."},
    {"id": "18c2b54..."}
  ],
  "resultSizeEstimate": 2
}
```
