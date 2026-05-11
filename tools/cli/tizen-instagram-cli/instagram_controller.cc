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
#include "instagram_controller.hh"

#include <json.hpp>

#include "mcp_client.hh"

namespace tizenclaw {
namespace cli {

namespace {

// Build a JSON error response
std::string ErrorJson(const std::string& msg) {
  return nlohmann::json{{"error", msg}}.dump();
}

// Extract the usable data payload from a tools/call response.
// MCP returns: result.result.content[0] as {type:"json",json:{...}}
//              or                          {type:"text",text:"<json-string>"}
nlohmann::json ExtractContent(const nlohmann::json& resp) {
  if (resp.contains("error"))
    throw std::runtime_error(resp["error"].dump());

  const auto& result = resp.value("result", nlohmann::json::object());

  if (result.value("isError", false)) {
    std::string text;
    if (result.contains("content") && result["content"].is_array() &&
        !result["content"].empty())
      text = result["content"][0].value("text", "tool error");
    throw std::runtime_error(text);
  }

  if (!result.contains("content") || !result["content"].is_array() ||
      result["content"].empty())
    throw std::runtime_error("empty content in tool response");

  const auto& first = result["content"][0];

  if (first.value("type", "") == "json")
    return first["json"];

  // type == "text" — the text field is a JSON-encoded string
  try {
    return nlohmann::json::parse(first.value("text", "{}"));
  } catch (...) {
    throw std::runtime_error(
        "could not parse text content: " + first.value("text", ""));
  }
}

}  // namespace

std::string InstagramController::FetchPosts(const std::string& server,
                                            const std::string& username,
                                            const std::string& limit_str,
                                            int start_from) const {
  McpClient client;
  try {
    client.Connect(server);
    client.Initialize();

    // Build tool arguments matching the Python client exactly
    nlohmann::json args;
    args["username"] = username;
    if (limit_str == "all") {
      args["limit"] = "all";
    } else {
      try {
        args["limit"] = std::stoi(limit_str);
      } catch (...) {
        args["limit"] = 3;
      }
    }
    if (start_from > 0)
      args["startFrom"] = start_from;

    auto resp = client.CallTool("get_instagram_posts", args, 180.0);
    auto data = ExtractContent(resp);

    // Normalise output: always include username + count at top level
    nlohmann::json out;
    out["username"] = username;
    out["posts"] = data.value("posts", nlohmann::json::array());
    out["pagination"] = data.value("pagination", nlohmann::json::object());
    out["count"] = out["posts"].size();
    client.Close();
    return out.dump();

  } catch (const std::exception& e) {
    client.Close();
    return ErrorJson(e.what());
  }
}

std::string InstagramController::ListTools(const std::string& server) const {
  McpClient client;
  try {
    client.Connect(server);
    client.Initialize();

    auto tools = client.ListTools();

    nlohmann::json arr = nlohmann::json::array();
    for (const auto& t : tools)
      arr.push_back({{"name", t.name}, {"description", t.description}});

    client.Close();
    return nlohmann::json{{"tools", arr}}.dump();

  } catch (const std::exception& e) {
    client.Close();
    return ErrorJson(e.what());
  }
}

}  // namespace cli
}  // namespace tizenclaw
