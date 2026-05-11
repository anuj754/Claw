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
#ifndef TIZENCLAW_CLI_INSTAGRAM_CONTROLLER_HH_
#define TIZENCLAW_CLI_INSTAGRAM_CONTROLLER_HH_

#include <string>

namespace tizenclaw {
namespace cli {

class InstagramController {
 public:
  InstagramController() = default;
  ~InstagramController() = default;

  // Fetch posts for an Instagram username via the MCP server.
  // limit_str: number as string, or "all"
  // Returns JSON string.
  std::string FetchPosts(const std::string& server,
                         const std::string& username,
                         const std::string& limit_str,
                         int start_from) const;

  // List tools available on the MCP server.
  // Returns JSON string.
  std::string ListTools(const std::string& server) const;
};

}  // namespace cli
}  // namespace tizenclaw

#endif  // TIZENCLAW_CLI_INSTAGRAM_CONTROLLER_HH_
