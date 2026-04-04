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

#include "mcp_http_client.hh"
#include "../../common/logging.hh"

#include <curl/curl.h>
#include <string>

namespace tizenclaw {

namespace {

// libcurl write callback — appends received data to a std::string.
size_t CurlWriteCallback(void* contents, size_t size, size_t nmemb,
                         void* userp) {
  size_t total = size * nmemb;
  static_cast<std::string*>(userp)->append(
      static_cast<char*>(contents), total);
  return total;
}

}  // namespace

McpHttpClient::McpHttpClient(const std::string& server_name,
                             const std::string& url,
                             const std::string& access_token,
                             int timeout_ms)
    : server_name_(server_name),
      url_(url),
      access_token_(access_token),
      timeout_ms_(timeout_ms) {}

bool McpHttpClient::Connect() {
  try {
    nlohmann::json init_params = {
        {"protocolVersion", "2024-11-05"},
        {"capabilities", nlohmann::json::object()},
        {"clientInfo", {{"name", "tizenclaw-mcp-client"}, {"version", "1.0.0"}}}
    };

    nlohmann::json response = PostRequest("initialize", init_params);
    if (response.contains("error")) {
      LOG(ERROR) << "McpHttpClient: Initialize failed for " << server_name_
                 << ": " << response["error"].dump();
      return false;
    }

    is_connected_ = true;
    LOG(INFO) << "McpHttpClient: Connected to " << server_name_ << " at " << url_;
    return true;
  } catch (const std::exception& e) {
    LOG(ERROR) << "McpHttpClient: Connect exception for " << server_name_
               << ": " << e.what();
    return false;
  }
}

void McpHttpClient::Disconnect() {
  // HTTP transport is stateless; nothing to tear down.
  is_connected_ = false;
}

std::vector<McpHttpClient::ToolInfo> McpHttpClient::GetTools() {
  std::vector<ToolInfo> tools;
  try {
    nlohmann::json response =
        PostRequest("tools/list", nlohmann::json::object());

    if (response.contains("result") && response["result"].contains("tools")) {
      for (const auto& t : response["result"]["tools"]) {
        ToolInfo info;
        info.name = t.value("name", "");
        info.description = t.value("description", "");
        if (t.contains("inputSchema")) {
          info.input_schema = t["inputSchema"];
        }
        tools.push_back(info);
      }
    }
  } catch (const std::exception& e) {
    LOG(ERROR) << "McpHttpClient: GetTools error for " << server_name_
               << ": " << e.what();
  }
  return tools;
}

nlohmann::json McpHttpClient::CallTool(const std::string& tool_name,
                                       const nlohmann::json& arguments) {
  nlohmann::json params = {
      {"name", tool_name},
      {"arguments", arguments}
  };

  try {
    nlohmann::json response = PostRequest("tools/call", params);
    if (response.contains("result")) {
      return response["result"];
    } else if (response.contains("error")) {
      LOG(WARNING) << "McpHttpClient: CallTool error from " << server_name_
                   << ": " << response["error"].dump();
      return {{"isError", true}, {"error", response["error"]}};
    }
    return {{"isError", true}, {"error", "Invalid response from HTTP MCP server"}};
  } catch (const std::exception& e) {
    LOG(ERROR) << "McpHttpClient: CallTool exception for " << server_name_
               << ": " << e.what();
    return {{"isError", true}, {"error", e.what()}};
  }
}

nlohmann::json McpHttpClient::PostRequest(const std::string& method,
                                          const nlohmann::json& params) {
  std::lock_guard<std::mutex> lock(mutex_);

  int req_id = next_req_id_++;
  nlohmann::json request = {
      {"jsonrpc", "2.0"},
      {"id", req_id},
      {"method", method},
      {"params", params}
  };
  std::string body = request.dump();
  std::string response_body;

  CURL* curl = curl_easy_init();
  if (!curl) {
    throw std::runtime_error("curl_easy_init() failed");
  }

  struct curl_slist* headers = nullptr;
  headers = curl_slist_append(headers, "Content-Type: application/json");
  headers = curl_slist_append(headers, "Accept: application/json");
  if (!access_token_.empty()) {
    std::string auth_header = "Authorization: Bearer " + access_token_;
    headers = curl_slist_append(headers, auth_header.c_str());
  }

  long timeout_sec = std::max(1, timeout_ms_ / 1000);
  curl_easy_setopt(curl, CURLOPT_URL, url_.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_body);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_sec);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

  CURLcode res = curl_easy_perform(curl);

  long http_code = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);

  if (res != CURLE_OK) {
    throw std::runtime_error(std::string("HTTP request failed: ") +
                             curl_easy_strerror(res));
  }

  if (http_code < 200 || http_code >= 300) {
    throw std::runtime_error("HTTP error " + std::to_string(http_code) +
                             " from " + server_name_);
  }

  try {
    return nlohmann::json::parse(response_body);
  } catch (const std::exception& e) {
    throw std::runtime_error(std::string("JSON parse error: ") + e.what() +
                             " body: " + response_body.substr(0, 256));
  }
}

}  // namespace tizenclaw
