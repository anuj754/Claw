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
#ifndef TIZENCLAW_CLI_SWARM_DELEGATE_HH_
#define TIZENCLAW_CLI_SWARM_DELEGATE_HH_

#include <string>
#include <vector>

namespace tizenclaw {
namespace cli {

struct SwarmPeerInfo {
  std::string ip;
  std::string device_type;
  std::vector<std::string> capabilities;
};

struct DelegateResult {
  bool success = false;
  std::string task_id;
  std::string answer;
  std::string error;
};

class SwarmDelegate {
 public:
  // Query local dashboard for active swarm peers.
  // Returns peers as parsed structs; empty on failure.
  std::vector<SwarmPeerInfo> ListPeers(
      const std::string& local_url = "http://localhost:9090") const;

  // Find the IP of the first peer matching device_type.
  // Returns empty string if not found.
  std::string FindPeerIp(
      const std::string& device_type,
      const std::string& local_url = "http://localhost:9090") const;

  // Send a prompt to a target peer's A2A endpoint and return the result.
  DelegateResult Send(
      const std::string& target_ip,
      const std::string& prompt,
      const std::string& bearer_token = "",
      int port = 9090) const;
};

}  // namespace cli
}  // namespace tizenclaw

#endif  // TIZENCLAW_CLI_SWARM_DELEGATE_HH_
