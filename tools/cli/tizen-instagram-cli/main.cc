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

#include <curl/curl.h>

#include <iostream>
#include <string>

namespace {

constexpr const char kDefaultServer[] = "http://192.168.1.3:3000";

constexpr const char kUsage[] = R"(Usage:
  tizen-instagram-cli <subcommand> [options]

Subcommands:
  fetch-posts   Fetch Instagram posts for a username
  list-tools    List tools available on the MCP server

Options for fetch-posts:
  --server <url>       MCP server URL (default: http://192.168.1.3:3000)
  --username <name>    Instagram username to fetch posts from
  --limit <n|all>      Number of posts, or "all" (default: 3)
  --start-from <n>     Pagination offset (default: 0)

Options for list-tools:
  --server <url>       MCP server URL (default: http://192.168.1.3:3000)
)";

void PrintUsage() { std::cerr << kUsage; }

std::string GetArg(int argc, char* argv[], const std::string& key) {
  for (int i = 2; i < argc - 1; ++i) {
    if (std::string(argv[i]) == key) return argv[i + 1];
  }
  return "";
}

}  // namespace

int main(int argc, char* argv[]) {
  curl_global_init(CURL_GLOBAL_DEFAULT);

  int exit_code = 0;

  if (argc < 2) {
    PrintUsage();
    exit_code = 1;
  } else {
    std::string cmd = argv[1];

    if (cmd == "fetch-posts") {
      std::string server = GetArg(argc, argv, "--server");
      if (server.empty()) server = kDefaultServer;

      std::string username = GetArg(argc, argv, "--username");
      if (username.empty()) {
        std::cerr << "--username is required\n";
        exit_code = 1;
      } else {
        std::string limit = GetArg(argc, argv, "--limit");
        if (limit.empty()) limit = "3";

        int start_from = 0;
        std::string sf = GetArg(argc, argv, "--start-from");
        if (!sf.empty()) {
          try { start_from = std::stoi(sf); } catch (...) {}
        }

        tizenclaw::cli::InstagramController c;
        std::cout << c.FetchPosts(server, username, limit, start_from)
                  << std::endl;
      }

    } else if (cmd == "list-tools") {
      std::string server = GetArg(argc, argv, "--server");
      if (server.empty()) server = kDefaultServer;

      tizenclaw::cli::InstagramController c;
      std::cout << c.ListTools(server) << std::endl;

    } else {
      PrintUsage();
      exit_code = 1;
    }
  }

  curl_global_cleanup();
  return exit_code;
}
