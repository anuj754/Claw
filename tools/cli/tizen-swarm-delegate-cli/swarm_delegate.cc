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

#include <sstream>

#include "http_client.hh"

namespace tizenclaw {
namespace cli {

std::vector<SwarmPeerInfo> SwarmDelegate::ListPeers(
    const std::string& local_url) const {
  HttpClient http;
  auto resp = http.Get(local_url + "/api/swarm");
  if (!resp.error.empty() || resp.status_code != 200) return {};

  try {
    auto j = nlohmann::json::parse(resp.body);
    std::vector<SwarmPeerInfo> peers;
    for (const auto& p : j.value("active_peers", nlohmann::json::array())) {
      SwarmPeerInfo info;
      info.ip = p.value("ip", "");
      info.device_type = p.value("device_type", "");
      if (p.contains("capabilities") && p["capabilities"].is_array()) {
        for (const auto& c : p["capabilities"])
          info.capabilities.push_back(c.get<std::string>());
      }
      if (!info.ip.empty()) peers.push_back(std::move(info));
    }
    return peers;
  } catch (...) {
    return {};
  }
}

std::string SwarmDelegate::FindPeerIp(
    const std::string& device_type,
    const std::string& local_url) const {
  for (const auto& peer : ListPeers(local_url)) {
    if (peer.device_type == device_type) return peer.ip;
  }
  return {};
}

DelegateResult SwarmDelegate::Send(
    const std::string& target_ip,
    const std::string& prompt,
    const std::string& bearer_token,
    int port) const {
  DelegateResult result;

  // Build JSON-RPC 2.0 tasks/send payload
  nlohmann::json req = {
      {"jsonrpc", "2.0"},
      {"method", "tasks/send"},
      {"params",
       {{"message", {{"parts", nlohmann::json::array({{{"text", prompt}}})}}}}},
      {"id", 1}};

  std::ostringstream url;
  url << "http://" << target_ip << ":" << port << "/api/a2a";

  std::string auth_header;
  if (!bearer_token.empty())
    auth_header = "Authorization: Bearer " + bearer_token;

  HttpClient http;
  auto resp = http.Post(url.str(), req.dump(), auth_header);

  if (!resp.error.empty()) {
    result.error = resp.error;
    return result;
  }

  if (resp.status_code == 401) {
    result.error = "authentication failed (check bearer token)";
    return result;
  }

  if (resp.status_code != 200) {
    result.error = "HTTP " + std::to_string(resp.status_code) + ": " + resp.body;
    return result;
  }

  try {
    auto j = nlohmann::json::parse(resp.body);

    // JSON-RPC error response
    if (j.contains("error")) {
      result.error = j["error"].value("message", j["error"].dump());
      return result;
    }

    auto& r = j["result"];
    result.task_id = r.value("id", "");

    // Extract text from artifacts array
    if (r.contains("artifacts") && r["artifacts"].is_array()) {
      for (const auto& a : r["artifacts"]) {
        if (a.value("type", "") == "text") {
          result.answer += a.value("text", "");
        }
      }
    }

    result.success = (r.value("status", "") == "completed");
    if (!result.success && result.error.empty())
      result.error = "task status: " + r.value("status", "unknown");

  } catch (const std::exception& e) {
    result.error = std::string("parse error: ") + e.what();
  }

  return result;
}

}  // namespace cli
}  // namespace tizenclaw
