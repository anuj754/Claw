/*
 * Copyright (c) 2026 Samsung Electronics Co., Ltd All Rights Reserved
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef A2UI_AGENT_HH
#define A2UI_AGENT_HH

#include <json.hpp>
#include <string>

namespace tizenclaw {

class AgentCore;

// A2UIAgent transforms raw tool execution results into structured A2UI JSON
// payloads that NUI applications can render directly.
//
// Flow:
//   Tool executes → raw JSON result
//   → A2UIAgent::Process(tool_name, session_id, tool_result)
//       [single focused LLM call with a2ui_system_prompt]
//   → structured a2ui JSON
//   → CanvasIpcServer::BroadcastA2UIResult() → NUI application
//
// The a2ui_system_prompt instructs the LLM to convert any tool result into
// a typed UI descriptor (view, list, chart, text, action) with a uniform
// schema so the NUI side needs no per-tool rendering logic.
class A2UIAgent {
 public:
  // core      — AgentCore that owns the active LLM backend.
  // sys_prompt — The dedicated a2ui system prompt loaded from agent_modes.json.
  A2UIAgent(AgentCore* core, const std::string& sys_prompt);

  // Transform a completed tool result into a structured a2ui JSON payload.
  //
  // Parameters:
  //   tool_name   — Name of the tool that produced the result.
  //   session_id  — Session context (used for logging only; no history kept).
  //   tool_result — Raw JSON output from the tool execution.
  //
  // Returns a JSON object conforming to the A2UI schema:
  //   {
  //     "type":    "view" | "list" | "chart" | "text" | "action",
  //     "title":   "<human-readable title>",
  //     "data":    { ... },          // tool-specific structured content
  //     "actions": [                 // optional interactive actions
  //       { "label": "...", "tool": "...", "args": { ... } }
  //     ]
  //   }
  //
  // On LLM failure returns: { "error": "<reason>", "raw": <tool_result> }
  nlohmann::json Process(const std::string& tool_name,
                         const std::string& session_id,
                         const nlohmann::json& tool_result);

 private:
  AgentCore*  core_;
  std::string system_prompt_;
};

}  // namespace tizenclaw

#endif  // A2UI_AGENT_HH
