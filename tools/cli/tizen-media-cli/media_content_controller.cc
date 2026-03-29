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

#include <media_content.h>

#include <cstdlib>
#include <string>
#include <vector>

namespace tizenclaw {
namespace cli {

namespace {

struct MediaEntry {
  std::string name;
  std::string path;
  std::string type;
  std::string mime_type;
  unsigned long long size = 0;
};

std::string EscapeJson(const std::string& s) {
  std::string result;
  result.reserve(s.size());
  for (char c : s) {
    switch (c) {
      case '"':  result += "\\\""; break;
      case '\\': result += "\\\\"; break;
      case '\n': result += "\\n";  break;
      case '\r': result += "\\r";  break;
      case '\t': result += "\\t";  break;
      default:   result += c;      break;
    }
  }
  return result;
}

std::string MediaTypeToStr(media_content_type_e t) {
  switch (t) {
    case MEDIA_CONTENT_TYPE_IMAGE: return "image";
    case MEDIA_CONTENT_TYPE_VIDEO: return "video";
    case MEDIA_CONTENT_TYPE_SOUND: return "sound";
    case MEDIA_CONTENT_TYPE_MUSIC: return "music";
    default: return "other";
  }
}

media_content_type_e StrToMediaType(
    const std::string& s) {
  if (s == "image") return MEDIA_CONTENT_TYPE_IMAGE;
  if (s == "video") return MEDIA_CONTENT_TYPE_VIDEO;
  if (s == "sound") return MEDIA_CONTENT_TYPE_SOUND;
  if (s == "music") return MEDIA_CONTENT_TYPE_MUSIC;
  return MEDIA_CONTENT_TYPE_OTHERS;
}

struct CbData {
  std::vector<MediaEntry>* entries;
  std::string filter_type;
  int max_count;
};

bool MediaInfoCb(media_info_h media,
                 void* user_data) {
  auto* data = static_cast<CbData*>(user_data);
  if (static_cast<int>(data->entries->size()) >=
      data->max_count) {
    return false;
  }

  media_content_type_e mtype;
  media_info_get_media_type(media, &mtype);

  if (data->filter_type != "all") {
    if (MediaTypeToStr(mtype) != data->filter_type)
      return true;  // skip
  }

  MediaEntry entry;
  entry.type = MediaTypeToStr(mtype);

  char* v = nullptr;
  if (media_info_get_display_name(media, &v) == 0 &&
      v) {
    entry.name = v;
    free(v);
  }
  v = nullptr;
  if (media_info_get_file_path(media, &v) == 0 &&
      v) {
    entry.path = v;
    free(v);
  }
  v = nullptr;
  if (media_info_get_mime_type(media, &v) == 0 &&
      v) {
    entry.mime_type = v;
    free(v);
  }

  unsigned long long sz = 0;
  media_info_get_size(media, &sz);
  entry.size = sz;

  data->entries->push_back(std::move(entry));
  return static_cast<int>(data->entries->size()) <
         data->max_count;
}

std::string EntryToJson(const MediaEntry& e) {
  return "{\"name\": \"" + EscapeJson(e.name) +
         "\", \"path\": \"" + EscapeJson(e.path) +
         "\", \"type\": \"" + e.type +
         "\", \"mime_type\": \"" +
         EscapeJson(e.mime_type) +
         "\", \"size_bytes\": " +
         std::to_string(e.size) + "}";
}

}  // namespace

std::string MediaContentController::ListMedia(
    const std::string& type, int max_count) const {
  if (media_content_connect() != 0)
    return "{\"error\": \"media_content_connect\"}";

  std::vector<MediaEntry> entries;
  CbData data{&entries, type, max_count};

  media_info_foreach_media_from_db(
      nullptr, MediaInfoCb, &data);

  media_content_disconnect();

  std::string result = "{\"media_files\": [";
  for (size_t i = 0; i < entries.size(); ++i) {
    if (i > 0) result += ", ";
    result += EntryToJson(entries[i]);
  }
  result += "], \"count\": " +
            std::to_string(entries.size()) +
            ", \"filter\": \"" + type + "\"}";
  return result;
}

}  // namespace cli
}  // namespace tizenclaw
