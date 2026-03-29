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

#include "media_content_controller.hh"
#include "metadata_controller.hh"
#include "mime_type_controller.hh"

#include <iostream>
#include <string>

namespace {

constexpr const char kUsage[] = R"(Usage:
  tizen-media-cli <subcommand>

Subcommands:
  content       List media files from DB
  metadata      Extract metadata from a file
  mime          Get MIME type for a file
  mime-ext      Get file extensions for MIME type
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

  if (cmd == "content") {
    std::string type = GetArg(argc, argv, "--type");
    if (type.empty()) type = "all";
    std::string max_s =
        GetArg(argc, argv, "--max");
    int max_count = max_s.empty() ? 20
                                  : std::stoi(max_s);
    tizenclaw::cli::MediaContentController c;
    std::cout << c.ListMedia(type, max_count)
              << std::endl;
  } else if (cmd == "metadata") {
    std::string path =
        GetArg(argc, argv, "--path");
    if (path.empty()) {
      std::cerr << "--path required\n";
      return 1;
    }
    tizenclaw::cli::MetadataController c;
    std::cout << c.GetMetadata(path) << std::endl;
  } else if (cmd == "mime") {
    std::string path =
        GetArg(argc, argv, "--path");
    if (path.empty()) {
      std::cerr << "--path required\n";
      return 1;
    }
    tizenclaw::cli::MimeTypeController c;
    std::cout << c.GetMimeType(path) << std::endl;
  } else if (cmd == "mime-ext") {
    std::string mime =
        GetArg(argc, argv, "--mime");
    if (mime.empty()) {
      std::cerr << "--mime required\n";
      return 1;
    }
    tizenclaw::cli::MimeTypeController c;
    std::cout << c.GetExtensions(mime)
              << std::endl;
  } else {
    PrintUsage();
    return 1;
  }

  return 0;
}
