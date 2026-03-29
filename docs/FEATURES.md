# TizenClaw Feature Matrix

> **Last Updated**: 2026-03-22

This document provides a comprehensive matrix of all TizenClaw features, organized by category, with their current implementation status.

---

## Legend

| Symbol | Meaning |
|:------:|---------|
| ✅ | Fully implemented and verified |
| 🟡 | Partially implemented / stub |
| 🔴 | Not yet implemented / planned |

---

## 1. Core Agent System

| Feature | Status | Details |
|---------|:------:|---------|
| Agentic Loop (iterative tool calling) | ✅ | Configurable `max_iterations` via `tool_policy.json` |
| LLM streaming responses | ✅ | Chunked IPC delivery (`stream_chunk` / `stream_end`) |
| Context compaction | ✅ | LLM-based summarization (oldest 10 turns → 1 compressed) |
| Multi-session support | ✅ | Per-session system prompt and history isolation |
| Edge memory management | ✅ | `malloc_trim(0)` + `sqlite3_release_memory` after 5min idle |
| JSON-RPC 2.0 IPC | ✅ | Length-prefix framing over Abstract Unix Domain Sockets |
| Concurrent client handling | ✅ | Thread pool, `kMaxConcurrentClients = 4` |
| UID authentication | ✅ | `SO_PEERCRED` (root, app_fw, system, developer) |
| System prompt externalization | ✅ | 4-level fallback (config → file → default → hardcoded) |
| Dynamic tool injection | ✅ | `{{AVAILABLE_TOOLS}}`, `{{CAPABILITY_SUMMARY}}` placeholders |
| Parallel tool execution | ✅ | `std::async` for concurrent tool calls |

## 2. LLM Backends

| Backend | Status | Default Model | Streaming | Token Counting |
|---------|:------:|:---:|:---------:|:--------------:|
| Google Gemini | ✅ | `gemini-2.5-flash` | ✅ | ✅ |
| OpenAI | ✅ | `gpt-4o` | ✅ | ✅ |
| Anthropic (Claude) | ✅ | `claude-sonnet-4-20250514` | ✅ | ✅ |
| xAI (Grok) | ✅ | `grok-3` | ✅ | ✅ |
| Ollama (local) | ✅ | `llama3` | ✅ | ✅ |
| RPK Plugin backends | ✅ | Custom | ✅ | ✅ |

| Feature | Status | Details |
|---------|:------:|---------|
| Unified priority switching | ✅ | Plugin > active > fallback with configurable priority |
| Automatic fallback | ✅ | Sequential retry with rate-limit backoff (HTTP 429) |
| API key encryption | ✅ | Device-bound `ENC:` prefix + base64 (backward compatible) |
| Per-session usage tracking | ✅ | Per-session, daily, monthly Markdown reports |
| System prompt customization | ✅ | Per-agent role prompts via `agent_roles.json` |

## 3. Communication Channels

| Channel | Status | Protocol | Outbound | Library |
|---------|:------:|----------|:--------:|---------|
| Telegram | ✅ | Bot API Long-Polling | ✅ | libcurl |
| Slack | ✅ | Socket Mode (WebSocket) | ✅ | libwebsockets |
| Discord | ✅ | Gateway WebSocket | ✅ | libwebsockets |
| MCP (Claude Desktop) | ✅ | stdio JSON-RPC 2.0 | ❌ | built-in |
| Webhook | ✅ | HTTP inbound (libsoup) | ❌ | libsoup |
| Voice (STT/TTS) | ✅ | Tizen STT/TTS C-API | ✅ | conditional |
| Web Dashboard | ✅ | libsoup SPA (port 9090) | ❌ | libsoup |
| SO Plugin | ✅ | C API (`tizenclaw_channel.h`) | Optional | dlopen |

| Feature | Status | Details |
|---------|:------:|---------|
| Channel abstraction interface | ✅ | C++ `Channel` base class |
| Config-driven activation | ✅ | `channels.json` enable/disable |
| Outbound messaging | ✅ | `SendTo(channel, text)` + `Broadcast(text)` |
| Plugin channel API | ✅ | `tizenclaw_channel.h` C API for `.so` plugins |
| Channel allowlists | ✅ | Per-channel chat_id/guild allowlists |
| Wake word detection | 🔴 | Requires hardware mic support |
| WhatsApp channel | 🔴 | Not implemented |
| Email channel | 🔴 | Not implemented |

## 4. Skills & Tool Ecosystem

### 4.1 Native CLI Tool Suites (13 directories)

| Category | Tool | Status | C-API | Async |
|----------|------|:------:|-------|:-----:|
| **App Management** | `list_apps` | ✅ | `app_manager` | |
| | `send_app_control` | ✅ | `app_control` | |
| | `terminate_app` | ✅ | `app_manager` | |
| | `get_package_info` | ✅ | `package_manager` | |
| **Device Info** | `get_device_info` | ✅ | `system_info` | |
| | `get_system_info` | ✅ | `system_info` | |
| | `get_runtime_info` | ✅ | `runtime_info` | |
| | `get_storage_info` | ✅ | `storage` | |
| | `get_system_settings` | ✅ | `system_settings` | |
| | `get_sensor_data` | ✅ | `sensor` | |
| | `get_thermal_info` | ✅ | `device` (thermal) | |
| **Network** | `get_wifi_info` | ✅ | `wifi-manager` | |
| | `get_bluetooth_info` | ✅ | `bluetooth` | |
| | `get_network_info` | ✅ | `connection` | |
| | `get_data_usage` | ✅ | `connection` (statistics) | |
| | `scan_wifi_networks` | ✅ | `wifi-manager` | ⚡ |
| | `scan_bluetooth_devices` | ✅ | `bluetooth` | ⚡ |
| **Display & HW** | `get_display_info` | ✅ | `device` (display) | |
| | `control_display` | ✅ | `device` (display) | |
| | `control_haptic` | ✅ | `device` (haptic) | |
| | `control_led` | ✅ | `device` (flash) | |
| | `control_volume` | ✅ | `sound_manager` | |
| | `control_power` | ✅ | `device` (power) | |
| **Media** | `get_battery_info` | ✅ | `device` (battery) | |
| | `get_sound_devices` | ✅ | `sound_manager` | |
| | `get_media_content` | ✅ | `media-content` | |
| | `get_metadata` | ✅ | `metadata-extractor` | |
| | `get_mime_type` | ✅ | `mime-type` | |
| **System** | `play_tone` | ✅ | `tone_player` | |
| | `play_feedback` | ✅ | `feedback` | |
| | `send_notification` | ✅ | `notification` | |
| | `schedule_alarm` | ✅ | `alarm` | |
| | `download_file` | ✅ | `url-download` | ⚡ |
| | `web_search` | ✅ | Wikipedia API | |

> ⚡ = Async skill using tizen-core event loop

### 4.2 Built-in Tools (AgentCore, Native C++)

| Tool | Status | Category |
|------|:------:|----------|
| `execute_code` | ✅ | Code Execution |
| `manage_custom_skill` | ✅ | Skill Management |
| `create_task` / `list_tasks` / `cancel_task` | ✅ | Task Scheduler |
| `create_session` / `list_sessions` / `send_to_session` | ✅ | Multi-Agent |
| `run_supervisor` | ✅ | Multi-Agent |
| `ingest_document` / `search_knowledge` | ✅ | RAG |
| `execute_action` / `action_<name>` | ✅ | Tizen Action Framework |
| `execute_cli` | ✅ | CLI Tool Plugins |
| `create_workflow` / `list_workflows` / `run_workflow` / `delete_workflow` | ✅ | Workflow Engine |
| `create_pipeline` / `list_pipelines` / `run_pipeline` / `delete_pipeline` | ✅ | Pipeline Engine |
| `remember` / `recall` / `forget` | ✅ | Persistent Memory |

### 4.3 Extensibility

| Feature | Status | Details |
|---------|:------:|---------|
| RPK Skill Plugins | ✅ | Python skills via platform-signed RPK packages |
| CLI Tool Plugins (TPK) | ✅ | Native binaries with `.tool.md` descriptors |
| LLM Backend Plugins (RPK) | ✅ | Custom LLM backends via RPK with priority |
| Channel Plugins (.so) | ✅ | Shared object plugins via C API |
| Skill hot-reload (inotify) | ✅ | Auto-detect new/modified skills without restart |
| Capability Registry | ✅ | Unified tool registration with function contracts |
| SKILL.md format | ✅ | Anthropic standard skill format |
| Manifest v2 | ✅ | Extended `version`, `author`, `compatibility` fields |
| Remote skill marketplace | 🟡 | REST API stubs, no dashboard UI yet |
| Per-skill seccomp profiles | 🔴 | All skills share container security profile |
| Per-skill resource quotas | 🔴 | No CPU/memory limits per execution |

## 5. Security

| Feature | Status | Details |
|---------|:------:|---------|
| OCI container isolation | ✅ | crun with PID/Mount/User namespaces |
| Tool execution policy | ✅ | Risk levels (low/medium/high), blocked skills list |
| Loop detection | ✅ | Same tool + args 3x → blocked; idle progress check |
| API key encryption | ✅ | Device-bound GLib SHA-256 + XOR |
| Audit logging | ✅ | Daily Markdown tables, 5MB rotation |
| UID authentication | ✅ | `SO_PEERCRED` on IPC socket |
| Admin authentication | ✅ | Session-token + SHA-256 password hashing |
| Webhook HMAC | ✅ | HMAC-SHA256 signature validation |
| Platform certificate signing | ✅ | Required for RPK/TPK plugin installation |
| Network access control per skill | 🔴 | No per-skill network allow/deny |

## 6. Knowledge & Intelligence

| Feature | Status | Details |
|---------|:------:|---------|
| Hybrid RAG search | ✅ | BM25 keyword (FTS5) + vector cosine via RRF (k=60) |
| On-device embedding | ✅ | ONNX Runtime `all-MiniLM-L6-v2` (384-dim, lazy-loaded) |
| Multi-DB support | ✅ | Attach multiple knowledge databases (tizen_api, tizen_guide) |
| FTS5 auto-sync | ✅ | Triggers keep FTS5 index consistent |
| Token budget estimation | ✅ | `EstimateTokens()` approximation (words × 1.3) |
| Persistent memory | ✅ | Long-term, episodic, short-term with LLM tools |
| Memory summary | ✅ | Auto-regenerated `memory.md` during idle |
| Context fusion | ✅ | Multi-source context fusion (`ContextFusionEngine`) |
| On-device OCR | ✅ | PaddleOCR PP-OCRv3 (Korean+English lite / CJK full) |
| ANN index (HNSW) | 🔴 | Currently brute-force cosine similarity |
| Pre-request token budgeting | 🔴 | Token count checked post-response only |

## 7. Automation & Orchestration

| Feature | Status | Details |
|---------|:------:|---------|
| Task scheduler | ✅ | Cron/interval/once/weekly with retry backoff |
| Supervisor agent | ✅ | Goal decomposition → delegate → validate |
| Skill pipelines | ✅ | Sequential execution with `{{variable}}` interpolation |
| Conditional branching | ✅ | `if/then/else` in pipelines |
| Workflow engine | ✅ | CRUD + execution via built-in tools |
| Autonomous triggers | ✅ | Event-driven rules with LLM evaluation |
| A2A protocol | ✅ | Cross-device HTTP JSON-RPC 2.0 with task lifecycle |
| Event Bus | ✅ | Pub/sub for system events |
| Agent roles | ✅ | Configurable via `agent_roles.json` |
| Parallel task execution | 🔴 | Currently sequential, planned dependency graph |

## 8. Operations & Deployment

| Feature | Status | Details |
|---------|:------:|---------|
| systemd service | ✅ | `tizenclaw.service` (Type=simple) |
| Socket activation | ✅ | Tool executor + code sandbox on-demand |
| GBS RPM packaging | ✅ | x86_64, armv7l, aarch64 architectures |
| Automated deploy | ✅ | `deploy.sh` script (build + install + restart) |
| Web Dashboard | ✅ | Glassmorphism SPA on port 9090 |
| Health metrics | ✅ | Prometheus-style `/api/metrics` endpoint |
| OTA updates | ✅ | HTTP pull with version check and rollback |
| Fleet management | 🟡 | Registration and heartbeat (stubs) |
| Secure tunneling | ✅ | ngrok for remote dashboard access |
| Config editor | ✅ | In-browser editing of 7+ config files |
| Debug service | ✅ | `tizenclaw-debug.service` (no container) |
| C-API SDK library | 🟡 | `libtizenclaw` implemented, not yet distributed |
| SDK documentation | 🟡 | API guide exists, integration guide pending |

## 9. MCP (Model Context Protocol)

| Feature | Status | Details |
|---------|:------:|---------|
| MCP Server (built-in) | ✅ | C++ stdio JSON-RPC 2.0 for Claude Desktop |
| MCP Client (built-in) | ✅ | Connect to external MCP tool servers |
| MCP Sandbox | ✅ | MCP servers run inside secure container |
| Tools exposed via MCP | ✅ | All registered tools available via MCP |

## 10. Testing

| Feature | Status | Details |
|---------|:------:|---------|
| Unit tests (gtest/gmock) | ✅ | 42 test files (~7,800 LOC, 205+ cases) |
| E2E smoke tests | ✅ | 2 test scripts |
| CLI tool validation | ✅ | Per-tool test scripts in `tests/verification/cli_tools/` |
| MCP compliance tests | ✅ | `tests/verification/mcp/` |
| Build-time testing | ✅ | `ctest -V` in RPM `%check` |
| LLM integration tests | ✅ | `tests/verification/llm_integration/` |
| Regression tests | ✅ | `tests/verification/regression/` |

---

## Architecture Diagram Reference

For detailed architecture diagrams and component descriptions, see:
- [System Design](DESIGN.md) — Full architecture with Mermaid diagrams
- [Tools Reference](TOOLS.md) — Complete skill/tool catalog
- [ML/AI Assets](ASSETS.md) — ONNX Runtime, RAG databases, OCR
- [C-API Guide](API_GUIDE.md) — SDK usage with code examples
