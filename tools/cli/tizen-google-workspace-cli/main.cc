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

#include <iostream>
#include <string>

namespace {

constexpr const char kUsage[] = R"(Usage:
  tizen-google-workspace-cli --action <list|read|send> [options]

Options for list:
  --query <QUERY>    Gmail search query (e.g. "is:unread")

Options for read:
  --id <MSG_ID>      Gmail message ID to read

Options for send:
  --to <EMAIL>       Recipient email address
  --subject <SUBJ>   Email subject
  --message <BODY>   Email body text
)";

void PrintUsage() {
  std::cerr << kUsage;
}

}  // namespace

int main(int argc, char* argv[]) {
  std::string action;
  std::string query;
  std::string id;
  std::string to;
  std::string subject;
  std::string message;

  for (int i = 1; i < argc - 1; ++i) {
    std::string arg = argv[i];
    if (arg == "--action")
      action = argv[i + 1];
    else if (arg == "--query")
      query = argv[i + 1];
    else if (arg == "--id")
      id = argv[i + 1];
    else if (arg == "--to")
      to = argv[i + 1];
    else if (arg == "--subject")
      subject = argv[i + 1];
    else if (arg == "--message")
      message = argv[i + 1];
  }

  if (action.empty()) {
    PrintUsage();
    return 1;
  }

  tizenclaw::cli::WorkspaceController c;

  if (action == "list") {
    std::cout << c.ListEmails(query) << std::endl;
  } else if (action == "read") {
    if (id.empty()) {
      std::cerr << "Error: --id is required for read action\n";
      return 1;
    }
    std::cout << c.ReadEmail(id) << std::endl;
  } else if (action == "send") {
    if (to.empty() || subject.empty() || message.empty()) {
      std::cerr << "Error: --to, --subject, and --message are required for send action\n";
      return 1;
    }
    std::cout << c.SendEmail(to, subject, message) << std::endl;
  } else {
    std::cerr << "Unknown action: " << action << "\n";
    PrintUsage();
    return 1;
  }

  return 0;
}
