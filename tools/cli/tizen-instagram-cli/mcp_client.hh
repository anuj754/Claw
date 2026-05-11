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
#ifndef TIZENCLAW_CLI_MCP_CLIENT_HH_
#define TIZENCLAW_CLI_MCP_CLIENT_HH_

#include <json.hpp>

#include <atomic>
#include <condition_variable>
#include <future>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace tizenclaw {
namespace cli {

// Minimal MCP client over HTTP/SSE transport.
// Mirrors the Python MCPClient in instagram_client.py.
//
// Thread model:
//   - Caller thread: Connect(), Initialize(), CallTool(), Close()
//   - SSE thread   : streams GET /sse, parses events, resolves futures
class McpClient {
 public:
  struct ToolInfo {
    std::string name;
    std::string description;
  };

  McpClient() = default;
  ~McpClient() { Close(); }

  // Open SSE stream and wait for the endpoint event.
  // Throws std::runtime_error on failure or timeout.
  void Connect(const std::string& server_url, double timeout_s = 10.0);

  // Send MCP initialize + notifications/initialized.
  void Initialize();

  // Call tools/list and return the tool list.
  std::vector<ToolInfo> ListTools();

  // Call tools/call and return the full result object.
  nlohmann::json CallTool(const std::string& name,
                          const nlohmann::json& arguments,
                          double timeout_s = 180.0);

  // Abort the SSE stream and join the SSE thread.
  void Close();

 private:
  // SSE listener (runs on sse_thread_)
  void ListenSse();

  // Called from CURLOPT_WRITEFUNCTION on the SSE thread
  void ProcessSseChunk(const std::string& chunk);
  void HandleSseEvent(const std::string& event_type,
                      const std::string& data);

  // Send a JSON-RPC request, block until SSE delivers the response
  nlohmann::json SendRequest(const std::string& method,
                             const nlohmann::json& params,
                             double timeout_s = 30.0);

  // Fire-and-forget JSON-RPC notification (no id, no response expected)
  void SendNotify(const std::string& method,
                  const nlohmann::json& params);

  int NextId();

  // CURL callbacks
  static size_t OnSseWrite(void* ptr, size_t size,
                           size_t nmemb, void* userdata);
  static int OnProgress(void* userdata, curl_off_t, curl_off_t,
                        curl_off_t, curl_off_t);

  std::string server_url_;
  std::string post_url_;  // set by endpoint SSE event

  // SSE thread
  std::thread sse_thread_;
  std::atomic<bool> stop_{false};

  // Connection handshake
  std::mutex connect_mutex_;
  std::condition_variable connect_cv_;
  bool connected_ = false;
  std::string connect_error_;

  // Pending JSON-RPC requests: id → promise
  std::mutex pending_mutex_;
  std::map<int, std::promise<nlohmann::json>> pending_;
  std::atomic<int> next_id_{1};

  // SSE parse state (SSE thread only — no locking needed)
  std::string sse_buf_;
  std::string sse_event_type_;
  std::vector<std::string> sse_data_lines_;
};

}  // namespace cli
}  // namespace tizenclaw

#endif  // TIZENCLAW_CLI_MCP_CLIENT_HH_
