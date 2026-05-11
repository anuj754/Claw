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
#include "mcp_client.hh"

#include <curl/curl.h>

#include <chrono>
#include <cstring>
#include <sstream>
#include <stdexcept>

namespace tizenclaw {
namespace cli {

namespace {

// Strip leading/trailing whitespace
std::string Trim(const std::string& s) {
  size_t start = s.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) return {};
  size_t end = s.find_last_not_of(" \t\r\n");
  return s.substr(start, end - start + 1);
}

// libcurl write callback for POST responses (body discarded — we only
// need the HTTP status; the real response comes via SSE)
size_t DevNull(void*, size_t size, size_t nmemb, void*) {
  return size * nmemb;
}

}  // namespace

// ── CURL callbacks ─────────────────────────────────────────────────────── //

size_t McpClient::OnSseWrite(void* ptr, size_t size,
                             size_t nmemb, void* userdata) {
  auto* self = static_cast<McpClient*>(userdata);
  std::string chunk(static_cast<char*>(ptr), size * nmemb);
  self->ProcessSseChunk(chunk);
  return size * nmemb;
}

// Return non-zero to abort the SSE transfer when stop_ is set
int McpClient::OnProgress(void* userdata,
                          curl_off_t, curl_off_t,
                          curl_off_t, curl_off_t) {
  auto* self = static_cast<McpClient*>(userdata);
  return self->stop_.load() ? 1 : 0;
}

// ── SSE stream ─────────────────────────────────────────────────────────── //

void McpClient::ListenSse() {
  CURL* curl = curl_easy_init();
  if (!curl) {
    std::lock_guard<std::mutex> lk(connect_mutex_);
    connect_error_ = "curl_easy_init failed for SSE";
    connect_cv_.notify_all();
    return;
  }

  std::string sse_url = server_url_ + "/sse";
  curl_easy_setopt(curl, CURLOPT_URL, sse_url.c_str());
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, OnSseWrite);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "TizenClaw/3.0");
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L);      // stream forever
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
  // Enable progress callback so we can abort cleanly
  curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
  curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, OnProgress);
  curl_easy_setopt(curl, CURLOPT_XFERINFODATA, this);

  CURLcode res = curl_easy_perform(curl);
  curl_easy_cleanup(curl);

  // Signal connection failure if the endpoint event never arrived
  {
    std::lock_guard<std::mutex> lk(connect_mutex_);
    if (!connected_ && connect_error_.empty()) {
      if (res == CURLE_ABORTED_BY_CALLBACK) {
        connect_error_ = "aborted";
      } else if (res != CURLE_OK) {
        connect_error_ = curl_easy_strerror(res);
      }
    }
    connect_cv_.notify_all();
  }

  // Fail any requests still waiting
  if (res != CURLE_OK && res != CURLE_ABORTED_BY_CALLBACK) {
    std::string err = curl_easy_strerror(res);
    std::lock_guard<std::mutex> lk(pending_mutex_);
    for (auto& [id, p] : pending_)
      p.set_value({{"error", err}});
    pending_.clear();
  } else {
    std::lock_guard<std::mutex> lk(pending_mutex_);
    for (auto& [id, p] : pending_)
      p.set_value({{"error", "SSE stream closed"}});
    pending_.clear();
  }
}

// ── SSE parsing (SSE thread only) ─────────────────────────────────────── //

void McpClient::ProcessSseChunk(const std::string& chunk) {
  sse_buf_ += chunk;

  size_t pos;
  while ((pos = sse_buf_.find('\n')) != std::string::npos) {
    std::string line = sse_buf_.substr(0, pos);
    sse_buf_ = sse_buf_.substr(pos + 1);

    // Strip trailing \r (CRLF streams)
    if (!line.empty() && line.back() == '\r')
      line.pop_back();

    if (line.empty()) {
      // Blank line — dispatch the accumulated event
      std::string data;
      for (size_t i = 0; i < sse_data_lines_.size(); ++i) {
        if (i > 0) data += '\n';
        data += sse_data_lines_[i];
      }
      HandleSseEvent(sse_event_type_, data);
      sse_event_type_.clear();
      sse_data_lines_.clear();
    } else if (line.substr(0, 6) == "event:") {
      sse_event_type_ = Trim(line.substr(6));
    } else if (line.substr(0, 5) == "data:") {
      sse_data_lines_.push_back(Trim(line.substr(5)));
    }
    // ignore "id:" and "retry:" fields
  }
}

void McpClient::HandleSseEvent(const std::string& event_type,
                               const std::string& data) {
  if (event_type == "endpoint") {
    // data is like:  /message?sessionId=<uuid>
    post_url_ = server_url_ +
                (data.empty() || data[0] == '/' ? data : "/" + data);

    std::lock_guard<std::mutex> lk(connect_mutex_);
    connected_ = true;
    connect_cv_.notify_all();

  } else if (event_type == "message") {
    nlohmann::json msg;
    try {
      msg = nlohmann::json::parse(data);
    } catch (...) {
      return;
    }

    if (!msg.contains("id")) return;
    int id = msg["id"].get<int>();

    std::lock_guard<std::mutex> lk(pending_mutex_);
    auto it = pending_.find(id);
    if (it != pending_.end()) {
      it->second.set_value(std::move(msg));
      pending_.erase(it);
    }
  }
}

// ── Public API ─────────────────────────────────────────────────────────── //

void McpClient::Connect(const std::string& server_url, double timeout_s) {
  server_url_ = server_url;
  // Strip trailing slash
  while (!server_url_.empty() && server_url_.back() == '/')
    server_url_.pop_back();

  sse_thread_ = std::thread([this]() { ListenSse(); });

  std::unique_lock<std::mutex> lk(connect_mutex_);
  connect_cv_.wait_for(
      lk,
      std::chrono::duration<double>(timeout_s),
      [this]() { return connected_ || !connect_error_.empty(); });

  if (!connect_error_.empty() && connect_error_ != "aborted")
    throw std::runtime_error("MCP connect failed: " + connect_error_);

  if (!connected_)
    throw std::runtime_error(
        "MCP connect timed out after " +
        std::to_string(static_cast<int>(timeout_s)) + "s — " +
        "check server URL and ensure the MCP server is running");
}

void McpClient::Initialize() {
  SendRequest("initialize", {
    {"protocolVersion", "2024-11-05"},
    {"capabilities", nlohmann::json::object()},
    {"clientInfo",
     {{"name", "tizen-instagram-cli"}, {"version", "1.0.0"}}}
  });
  SendNotify("notifications/initialized", nlohmann::json::object());
}

std::vector<McpClient::ToolInfo> McpClient::ListTools() {
  auto resp = SendRequest("tools/list", nlohmann::json::object());
  std::vector<ToolInfo> tools;
  auto arr = resp.value("result", nlohmann::json::object())
                 .value("tools", nlohmann::json::array());
  for (const auto& t : arr) {
    ToolInfo info;
    info.name = t.value("name", "");
    info.description = t.value("description", "");
    tools.push_back(std::move(info));
  }
  return tools;
}

nlohmann::json McpClient::CallTool(const std::string& name,
                                   const nlohmann::json& arguments,
                                   double timeout_s) {
  return SendRequest(
      "tools/call",
      {{"name", name}, {"arguments", arguments}},
      timeout_s);
}

void McpClient::Close() {
  stop_ = true;
  if (sse_thread_.joinable()) sse_thread_.join();
}

// ── Internal helpers ───────────────────────────────────────────────────── //

int McpClient::NextId() {
  return next_id_.fetch_add(1);
}

nlohmann::json McpClient::SendRequest(const std::string& method,
                                      const nlohmann::json& params,
                                      double timeout_s) {
  if (post_url_.empty())
    throw std::runtime_error("Not connected — call Connect() first");

  int id = NextId();

  std::future<nlohmann::json> future;
  {
    std::lock_guard<std::mutex> lk(pending_mutex_);
    pending_[id] = std::promise<nlohmann::json>();
    future = pending_[id].get_future();
  }

  nlohmann::json msg = {
      {"jsonrpc", "2.0"},
      {"id", id},
      {"method", method},
      {"params", params}};
  std::string body = msg.dump();

  // POST the request — response arrives via SSE, not the HTTP body
  CURL* curl = curl_easy_init();
  if (!curl) {
    std::lock_guard<std::mutex> lk(pending_mutex_);
    pending_.erase(id);
    throw std::runtime_error("curl_easy_init failed for POST");
  }

  struct curl_slist* headers = nullptr;
  headers = curl_slist_append(headers, "Content-Type: application/json");

  curl_easy_setopt(curl, CURLOPT_URL, post_url_.c_str());
  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, DevNull);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "TizenClaw/3.0");

  CURLcode res = curl_easy_perform(curl);
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);

  if (res != CURLE_OK) {
    std::lock_guard<std::mutex> lk(pending_mutex_);
    pending_.erase(id);
    throw std::runtime_error(
        std::string("POST failed: ") + curl_easy_strerror(res));
  }

  // Wait for the SSE response
  auto status = future.wait_for(std::chrono::duration<double>(timeout_s));
  if (status == std::future_status::timeout) {
    std::lock_guard<std::mutex> lk(pending_mutex_);
    pending_.erase(id);
    throw std::runtime_error(
        "Request '" + method + "' timed out after " +
        std::to_string(static_cast<int>(timeout_s)) + "s");
  }

  return future.get();
}

void McpClient::SendNotify(const std::string& method,
                           const nlohmann::json& params) {
  if (post_url_.empty()) return;

  nlohmann::json msg = {
      {"jsonrpc", "2.0"}, {"method", method}, {"params", params}};
  std::string body = msg.dump();

  CURL* curl = curl_easy_init();
  if (!curl) return;

  struct curl_slist* headers = nullptr;
  headers = curl_slist_append(headers, "Content-Type: application/json");

  curl_easy_setopt(curl, CURLOPT_URL, post_url_.c_str());
  curl_easy_setopt(curl, CURLOPT_POST, 1L);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, DevNull);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "TizenClaw/3.0");

  curl_easy_perform(curl);
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
}

}  // namespace cli
}  // namespace tizenclaw
