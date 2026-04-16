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
#include "a2ui_agent.hh"

#include "../../common/logging.hh"
#include "agent_core.hh"

namespace tizenclaw {

A2UIAgent::A2UIAgent(AgentCore* core, const std::string& sys_prompt)
    : core_(core), system_prompt_(sys_prompt) {}

nlohmann::json A2UIAgent::Process(const std::string& tool_name,
                                   const std::string& session_id,
                                   const nlohmann::json& tool_result) {
  if (!core_) {
    LOG(ERROR) << "A2UIAgent: core is null";
    return {{"error", "A2UIAgent not initialized"}, {"raw", tool_result}};
  }

  // Build the user message: include tool name and raw JSON result so the LLM
  // has full context when generating the a2ui descriptor.
  nlohmann::json user_payload = {
      {"tool_name", tool_name},
      {"session_id", session_id},
      {"tool_result", tool_result}
  };

  const std::string user_message = user_payload.dump();

  LOG(INFO) << "A2UIAgent: processing tool '" << tool_name
            << "' for session '" << session_id << "'";

  // Single focused LLM call — no tools, no conversation history.
  std::string llm_text = core_->DirectLlmChat(system_prompt_, user_message);

  if (llm_text.empty()) {
    LOG(WARNING) << "A2UIAgent: empty LLM response for tool '" << tool_name << "'";
    return {{"error", "empty LLM response"}, {"raw", tool_result}};
  }

  // Strip markdown code fences if the LLM wrapped the JSON.
  auto strip_fences = [](std::string s) -> std::string {
    // Remove leading ```json or ``` fence
    auto start = s.find("```");
    if (start != std::string::npos) {
      auto nl = s.find('\n', start);
      if (nl != std::string::npos) s = s.substr(nl + 1);
    }
    // Remove trailing ``` fence
    auto end = s.rfind("```");
    if (end != std::string::npos) s = s.substr(0, end);
    // Trim leading/trailing whitespace
    auto lp = s.find_first_not_of(" \t\r\n");
    auto rp = s.find_last_not_of(" \t\r\n");
    if (lp == std::string::npos) return "";
    return s.substr(lp, rp - lp + 1);
  };

  std::string clean = strip_fences(llm_text);

  try {
    nlohmann::json a2ui_json = nlohmann::json::parse(clean);
    LOG(INFO) << "A2UIAgent: generated a2ui payload for tool '" << tool_name
              << "': type=" << a2ui_json.value("type", "<unknown>");
    return a2ui_json;
  } catch (const nlohmann::json::exception& e) {
    LOG(WARNING) << "A2UIAgent: failed to parse LLM output as JSON for tool '"
                 << tool_name << "': " << e.what();
    // Return the raw text wrapped so the NUI can still display something.
    return {
        {"type", "text"},
        {"title", tool_name},
        {"data", {{"text", llm_text}}},
        {"raw", tool_result}
    };
  }
}

}  // namespace tizenclaw
