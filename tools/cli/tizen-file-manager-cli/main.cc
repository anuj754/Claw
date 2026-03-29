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

#include "file_controller.hh"

#include <iostream>
#include <string>

namespace {

constexpr const char kUsage[] = R"(Usage:
  tizen-file-manager-cli <subcommand> [options]

Subcommands:
  read      --path <PATH>
  write     --path <PATH> --content <TEXT>
  append    --path <PATH> --content <TEXT>
  remove    --path <PATH>
  mkdir     --path <PATH>
  list      --path <PATH>
  stat      --path <PATH>
  copy      --src <PATH> --dst <PATH>
  move      --src <PATH> --dst <PATH>
  download  --url <URL> --dest <PATH>
)";

void PrintUsage() {
  std::cerr << kUsage;
}

std::string GetArg(int argc, char* argv[],
                   const std::string& key) {
  for (int i = 2; i < argc - 1; ++i) {
    if (std::string(argv[i]) == key)
      return argv[i + 1];
  }

  return "";
}

}  // namespace

int main(int argc, char* argv[]) {
  if (argc < 2) {
    PrintUsage();
    return 1;
  }

  std::string cmd = argv[1];
  tizenclaw::cli::FileController c;

  if (cmd == "read") {
    std::string path = GetArg(argc, argv, "--path");
    if (path.empty()) {
      std::cerr << "--path required\n";
      return 1;
    }

    std::cout << c.Read(path) << std::endl;
  } else if (cmd == "write") {
    std::string path = GetArg(argc, argv, "--path");
    std::string content =
        GetArg(argc, argv, "--content");
    if (path.empty()) {
      std::cerr << "--path required\n";
      return 1;
    }

    std::cout << c.Write(path, content) << std::endl;
  } else if (cmd == "append") {
    std::string path = GetArg(argc, argv, "--path");
    std::string content =
        GetArg(argc, argv, "--content");
    if (path.empty()) {
      std::cerr << "--path required\n";
      return 1;
    }

    std::cout << c.Append(path, content) << std::endl;
  } else if (cmd == "remove") {
    std::string path = GetArg(argc, argv, "--path");
    if (path.empty()) {
      std::cerr << "--path required\n";
      return 1;
    }

    std::cout << c.Remove(path) << std::endl;
  } else if (cmd == "mkdir") {
    std::string path = GetArg(argc, argv, "--path");
    if (path.empty()) {
      std::cerr << "--path required\n";
      return 1;
    }

    std::cout << c.Mkdir(path) << std::endl;
  } else if (cmd == "list") {
    std::string path = GetArg(argc, argv, "--path");
    if (path.empty()) {
      std::cerr << "--path required\n";
      return 1;
    }

    std::cout << c.List(path) << std::endl;
  } else if (cmd == "stat") {
    std::string path = GetArg(argc, argv, "--path");
    if (path.empty()) {
      std::cerr << "--path required\n";
      return 1;
    }

    std::cout << c.Stat(path) << std::endl;
  } else if (cmd == "copy") {
    std::string src = GetArg(argc, argv, "--src");
    std::string dst = GetArg(argc, argv, "--dst");
    if (src.empty() || dst.empty()) {
      std::cerr << "--src and --dst required\n";
      return 1;
    }

    std::cout << c.Copy(src, dst) << std::endl;
  } else if (cmd == "move") {
    std::string src = GetArg(argc, argv, "--src");
    std::string dst = GetArg(argc, argv, "--dst");
    if (src.empty() || dst.empty()) {
      std::cerr << "--src and --dst required\n";
      return 1;
    }

    std::cout << c.Move(src, dst) << std::endl;
  } else if (cmd == "download") {
    std::string url = GetArg(argc, argv, "--url");
    std::string dest = GetArg(argc, argv, "--dest");
    if (url.empty() || dest.empty()) {
      std::cerr << "--url and --dest required\n";
      return 1;
    }

    std::cout << c.Download(url, dest)
              << std::endl;
  } else {
    PrintUsage();
    return 1;
  }

  return 0;
}
