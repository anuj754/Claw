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

#ifndef TIZENCLAW_CLI_WORKSPACE_CONTROLLER_HH_
#define TIZENCLAW_CLI_WORKSPACE_CONTROLLER_HH_

#include <string>

namespace tizenclaw {
namespace cli {

class WorkspaceController {
 public:
  WorkspaceController();
  ~WorkspaceController() = default;

  std::string ListEmails(const std::string& query) const;
  std::string ReadEmail(const std::string& id) const;
  std::string SendEmail(const std::string& to,
                        const std::string& subject,
                        const std::string& body) const;

 private:
  std::string GetAccessToken() const;
  std::string config_path_;
};

}  // namespace cli
}  // namespace tizenclaw

#endif  // TIZENCLAW_CLI_WORKSPACE_CONTROLLER_HH_
