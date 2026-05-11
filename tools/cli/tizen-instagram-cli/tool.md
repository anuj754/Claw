# tizen-instagram-cli
**Description**: Fetch Instagram posts via a remote MCP HTTP/SSE server. Connects using the MCP protocol over HTTP/SSE transport and calls the `get_instagram_posts` tool.

## Subcommands
| Subcommand | Description |
|---|---|
| `fetch-posts` | Fetch Instagram posts for a given username |
| `list-tools` | List all tools available on the MCP server |

## Usage
```
tizen-instagram-cli fetch-posts --username <NAME> [--server <URL>] [--limit <N|all>] [--start-from <N>]
tizen-instagram-cli list-tools [--server <URL>]
```

## Options
| Option | Default | Description |
|--------|---------|-------------|
| `--server <url>` | `http://192.168.1.3:3000` | MCP server base URL |
| `--username <name>` | — | Instagram username to fetch posts from (required for fetch-posts) |
| `--limit <n\|all>` | `3` | Number of posts to fetch, or `"all"` |
| `--start-from <n>` | `0` | Post index to start from (pagination) |

## Example Output — fetch-posts
```json
{
  "username": "nasa",
  "count": 3,
  "posts": [
    {
      "postUrl": "https://www.instagram.com/p/...",
      "type": "image",
      "timestamp": "2026-04-10T12:00:00.000Z",
      "caption": "Exploring the cosmos...",
      "mediaUrl": "https://..."
    }
  ],
  "pagination": {
    "currentBatch": { "start": 0, "end": 3 },
    "hasMore": true,
    "nextStartFrom": 3
  }
}
```

## Example Output — list-tools
```json
{
  "tools": [
    {
      "name": "get_instagram_posts",
      "description": "Fetches recent Instagram posts for a given username"
    }
  ]
}
```

## Error Output
```json
{ "error": "MCP connect timed out after 10s — check server URL and ensure the MCP server is running" }
```
