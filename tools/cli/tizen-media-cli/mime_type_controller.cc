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

#include "mime_type_controller.hh"

#include <mime_type.h>

#include <cstdlib>
#include <string>

namespace tizenclaw {
namespace cli {

namespace {

std::string EscapeJson(const std::string& s) {
  std::string result;
  result.reserve(s.size());
  for (char c : s) {
    switch (c) {
      case '"':  result += "\\\""; break;
      case '\\': result += "\\\\"; break;
      default:   result += c;      break;
    }
  }
  return result;
}

}  // namespace

std::string MimeTypeController::GetMimeType(
    const std::string& file_path) const {
  char* mime = nullptr;
  int ret = mime_type_get_mime_type(
      file_path.c_str(), &mime);
  if (ret != 0 || !mime)
    return "{\"error\": \"mime_type_get_mime_type\"}";

  std::string result =
      "{\"file_path\": \"" +
      EscapeJson(file_path) +
      "\", \"mime_type\": \"" +
      EscapeJson(mime) + "\"}";
  free(mime);
  return result;
}

std::string MimeTypeController::GetExtensions(
    const std::string& mime_type) const {
  char** exts = nullptr;
  int length = 0;
  int ret = mime_type_get_file_extension(
      mime_type.c_str(), &exts, &length);
  if (ret != 0 || !exts)
    return "{\"error\": "
           "\"mime_type_get_file_extension\"}";

  std::string result =
      "{\"mime_type\": \"" +
      EscapeJson(mime_type) +
      "\", \"extensions\": [";
  for (int i = 0; i < length; ++i) {
    if (i > 0) result += ", ";
    result += "\"" +
              EscapeJson(exts[i] ? exts[i] : "") +
              "\"";
    free(exts[i]);
  }
  free(exts);
  result += "]}";
  return result;
}

}  // namespace cli
}  // namespace tizenclaw
