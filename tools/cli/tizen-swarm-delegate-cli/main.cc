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
#include "swarm_delegate.hh"

#include <json.hpp>

#include <iostream>
#include <string>

namespace {

constexpr const char kUsage[] = R"(Usage:
  tizen-swarm-delegate-cli --list-peers
  tizen-swarm-delegate-cli --device-type <TYPE> --prompt <TEXT> [--bearer-token <TOKEN>]
  tizen-swarm-delegate-cli --target-ip <IP>    --prompt <TEXT> [--bearer-token <TOKEN>]

Options:
  --list-peers              List active swarm peers discovered on the LAN
  --device-type <TYPE>      Target peer by device type (e.g. tv, refrigerator, oven)
  --target-ip <IP>          Target peer by IP address directly
  --prompt <TEXT>           Prompt to send to the target agent
  --bearer-token <TOKEN>    Optional A2A bearer token for the target
)";

}  // namespace

int main(int argc, char* argv[]) {
  std::string device_type;
  std::string target_ip;
  std::string prompt;
  std::string bearer_token;
  bool list_peers = false;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--list-peers") {
      list_peers = true;
    } else if (arg == "--device-type" && i + 1 < argc) {
      device_type = argv[++i];
    } else if (arg == "--target-ip" && i + 1 < argc) {
      target_ip = argv[++i];
    } else if (arg == "--prompt" && i + 1 < argc) {
      prompt = argv[++i];
    } else if (arg == "--bearer-token" && i + 1 < argc) {
      bearer_token = argv[++i];
    }
  }

  tizenclaw::cli::SwarmDelegate delegate;

  // ── list-peers mode ──────────────────────────────────────────────────────
  if (list_peers) {
    auto peers = delegate.ListPeers();
    nlohmann::json out = nlohmann::json::array();
    for (const auto& p : peers) {
      nlohmann::json pj;
      pj["ip"] = p.ip;
      pj["device_type"] = p.device_type;
      pj["capabilities"] = p.capabilities;
      out.push_back(std::move(pj));
    }
    std::cout << nlohmann::json{{"peers", out}}.dump(2) << std::endl;
    return 0;
  }

  // ── delegate mode ─────────────────────────────────────────────────────────
  if (prompt.empty()) {
    std::cerr << kUsage;
    return 1;
  }

  // Resolve target IP from device type if not given directly
  if (target_ip.empty()) {
    if (device_type.empty()) {
      std::cerr << kUsage;
      return 1;
    }
    target_ip = delegate.FindPeerIp(device_type);
    if (target_ip.empty()) {
      nlohmann::json err = {
          {"success", false},
          {"error", "no peer found with device_type: " + device_type}};
      std::cout << err.dump(2) << std::endl;
      return 1;
    }
  }

  auto result = delegate.Send(target_ip, prompt, bearer_token);

  nlohmann::json out;
  out["success"] = result.success;
  out["task_id"] = result.task_id;
  out["target_ip"] = target_ip;
  if (!result.answer.empty()) out["answer"] = result.answer;
  if (!result.error.empty()) out["error"] = result.error;

  std::cout << out.dump(2) << std::endl;
  return result.success ? 0 : 1;
}
