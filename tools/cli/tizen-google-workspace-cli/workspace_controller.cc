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

#include "workspace_controller.hh"
#include "http_client.hh"

#include <fstream>
#include <iostream>
#include <sstream>

// Third-party JSON
#include <nlohmann/json.hpp>
// glib for base64 encoding
#include <glib.h>

namespace tizenclaw {
namespace cli {

namespace {

std::string Base64UrlEncode(const std::string& input) {
  gchar* b64 = g_base64_encode(reinterpret_cast<const guchar*>(input.data()), input.size());
  std::string result(b64);
  g_free(b64);
  
  // URL safe replacements
  for (char& c : result) {
    if (c == '+') c = '-';
    else if (c == '/') c = '_';
  }
  // Remove padding
  result.erase(std::remove(result.begin(), result.end(), '='), result.end());
  return result;
}

std::string UrlEncode(const std::string& value) {
  gchar* encoded = g_uri_escape_string(value.c_str(), nullptr, TRUE);
  std::string result(encoded);
  g_free(encoded);
  return result;
}

}  // namespace

WorkspaceController::WorkspaceController() {
  config_path_ = "/opt/usr/share/tizenclaw/config/google_workspace_config.json";
}

std::string WorkspaceController::GetAccessToken() const {
  std::ifstream f(config_path_);
  if (!f.is_open()) {
    // Check local fallback for testing
    f.open("google_workspace_config.json");
    if (!f.is_open()) {
      nlohmann::json err = {{"error", "google_workspace_config.json not found"}};
      return err.dump();
    }
  }

  try {
    nlohmann::json j;
    f >> j;
    if (j.contains("access_token")) {
      return j["access_token"].get<std::string>();
    }
  } catch (...) {
  }
  
  return "";
}

std::string WorkspaceController::ListEmails(const std::string& query) const {
  std::string token = GetAccessToken();
  if (token.empty() || token[0] == '{') {
    return token.empty() ? R"({"error": "No access_token found"})" : token;
  }

  HttpClient client;
  std::string url = "https://gmail.googleapis.com/gmail/v1/users/me/messages?maxResults=10";
  if (!query.empty()) {
    url += "&q=" + UrlEncode(query);
  }

  std::string headers = "Authorization: Bearer " + token;
  auto resp = client.Get(url, headers);
  
  if (resp.status_code != 200) {
    nlohmann::json err = {
      {"error", "Gmail API failed"},
      {"status", resp.status_code},
      {"body", resp.body}
    };
    return err.dump();
  }

  return resp.body;
}

std::string WorkspaceController::ReadEmail(const std::string& id) const {
  std::string token = GetAccessToken();
  if (token.empty() || token[0] == '{') {
    return token.empty() ? R"({"error": "No access_token found"})" : token;
  }

  HttpClient client;
  std::string url = "https://gmail.googleapis.com/gmail/v1/users/me/messages/" + UrlEncode(id);

  std::string headers = "Authorization: Bearer " + token;
  auto resp = client.Get(url, headers);
  
  if (resp.status_code != 200) {
    nlohmann::json err = {
      {"error", "Gmail API failed"},
      {"status", resp.status_code},
      {"body", resp.body}
    };
    return err.dump();
  }

  return resp.body;
}

std::string WorkspaceController::SendEmail(const std::string& to,
                                           const std::string& subject,
                                           const std::string& body) const {
  std::string token = GetAccessToken();
  if (token.empty() || token[0] == '{') {
    return token.empty() ? R"({"error": "No access_token found"})" : token;
  }

  std::ostringstream raw_message;
  raw_message << "To: " << to << "\r\n"
              << "Subject: " << subject << "\r\n"
              << "Content-Type: text/plain; charset=\"UTF-8\"\r\n"
              << "\r\n"
              << body;

  std::string encoded_msg = Base64UrlEncode(raw_message.str());
  nlohmann::json req_body = {
    {"raw", encoded_msg}
  };

  HttpClient client;
  std::string url = "https://gmail.googleapis.com/gmail/v1/users/me/messages/send";
  std::string headers = "Authorization: Bearer " + token;

  auto resp = client.Post(url, req_body.dump(), headers);
  
  if (resp.status_code != 200) {
    nlohmann::json err = {
      {"error", "Gmail API failed"},
      {"status", resp.status_code},
      {"body", resp.body}
    };
    return err.dump();
  }

  return resp.body;
}

}  // namespace cli
}  // namespace tizenclaw
