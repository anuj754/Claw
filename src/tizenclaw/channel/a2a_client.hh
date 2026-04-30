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
#ifndef A2A_CLIENT_HH
#define A2A_CLIENT_HH

#include <json.hpp>
#include <map>
#include <string>

namespace tizenclaw {

// Result of a remote A2A task invocation
struct A2AClientResult {
  bool success = false;
  std::string task_id;
  std::string response;
  std::string error;
};

// HTTP client for calling a remote A2A agent.
// The remote agent must expose:
//   POST /a2a                    (JSON-RPC 2.0 tasks/send)
//   GET  /.well-known/agent.json (agent card)
//
// Usage (primary device querying secondary):
//   A2AClient client;
//   auto r = client.SendTask("http://192.168.1.20:9090", "What is the oven temperature?");
//   if (r.success) use(r.response);
class A2AClient {
 public:
  A2AClient() = default;

  // Send a text task to the remote agent and return its reply synchronously.
  // base_url:      e.g. "http://192.168.1.20:9090"
  // text:          the natural language query
  // bearer_token:  optional auth token ("" = no auth)
  [[nodiscard]] A2AClientResult SendTask(
      const std::string& base_url,
      const std::string& text,
      const std::string& bearer_token = "");

  // Fetch the agent card from /.well-known/agent.json.
  // Returns an empty object on failure.
  [[nodiscard]] nlohmann::json FetchAgentCard(
      const std::string& base_url,
      const std::string& bearer_token = "");
};

}  // namespace tizenclaw

#endif  // A2A_CLIENT_HH
