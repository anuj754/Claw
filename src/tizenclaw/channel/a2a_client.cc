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
#include "a2a_client.hh"

#include <chrono>
#include <sstream>

#include "../../common/logging.hh"
#include "../infra/http_client.hh"

namespace tizenclaw {

A2AClientResult A2AClient::SendTask(const std::string& base_url,
                                    const std::string& text,
                                    const std::string& bearer_token) {
  A2AClientResult result;

  // Build a unique JSON-RPC 2.0 request id from timestamp
  auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch())
                    .count();
  std::ostringstream rpc_id;
  rpc_id << "a2ac-" << now_ms;

  nlohmann::json request = {
      {"jsonrpc", "2.0"},
      {"id", rpc_id.str()},
      {"method", "tasks/send"},
      {"params",
       {{"message",
         {{"role", "user"},
          {"parts",
           nlohmann::json::array({{{"text", text}}})}}}}}}};

  std::map<std::string, std::string> headers = {
      {"Content-Type", "application/json"},
      {"Accept", "application/json"}};
  if (!bearer_token.empty()) {
    headers["Authorization"] = "Bearer " + bearer_token;
  }

  std::string url = base_url + "/a2a";
  LOG(INFO) << "A2AClient: sending task to " << url;

  auto resp = HttpClient::Post(url, headers, request.dump(),
                               1,   // max_retries (no retry — caller decides)
                               10,  // connect_timeout_sec
                               60); // request_timeout_sec

  if (!resp.success) {
    result.error = "HTTP error: " + resp.error;
    LOG(ERROR) << "A2AClient: " << result.error;
    return result;
  }

  try {
    auto j = nlohmann::json::parse(resp.body);

    if (j.contains("error")) {
      result.error = j["error"].value("message", "Unknown RPC error");
      LOG(ERROR) << "A2AClient: RPC error: " << result.error;
      return result;
    }

    if (!j.contains("result")) {
      result.error = "Malformed response: missing 'result'";
      return result;
    }

    auto& res = j["result"];
    result.task_id = res.value("id", "");

    // Extract text from artifacts array
    if (res.contains("artifacts") && res["artifacts"].is_array()) {
      for (auto& artifact : res["artifacts"]) {
        if (artifact.value("type", "") == "text") {
          result.response += artifact.value("text", "");
        }
      }
    }

    result.success = true;
    LOG(INFO) << "A2AClient: task " << result.task_id << " completed";
  } catch (const std::exception& e) {
    result.error = std::string("Response parse error: ") + e.what();
    LOG(ERROR) << "A2AClient: " << result.error;
  }

  return result;
}

nlohmann::json A2AClient::FetchAgentCard(const std::string& base_url,
                                         const std::string& bearer_token) {
  std::map<std::string, std::string> headers = {
      {"Accept", "application/json"}};
  if (!bearer_token.empty()) {
    headers["Authorization"] = "Bearer " + bearer_token;
  }

  std::string url = base_url + "/.well-known/agent.json";
  auto resp = HttpClient::Get(url, headers, 1, 5, 10);

  if (!resp.success) {
    LOG(WARNING) << "A2AClient: failed to fetch agent card from "
                 << url << ": " << resp.error;
    return nlohmann::json::object();
  }

  try {
    return nlohmann::json::parse(resp.body);
  } catch (...) {
    return nlohmann::json::object();
  }
}

}  // namespace tizenclaw
