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

#include "metadata_controller.hh"

#include <metadata_extractor.h>

#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

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
      case '\n': result += "\\n";  break;
      case '\r': result += "\\r";  break;
      case '\t': result += "\\t";  break;
      default:   result += c;      break;
    }
  }
  return result;
}

struct AttrDef {
  metadata_extractor_attr_e attr;
  const char* name;
  bool is_numeric;
};

const AttrDef kAttrs[] = {
    {METADATA_DURATION, "duration", true},
    {METADATA_VIDEO_BITRATE, "video_bitrate", true},
    {METADATA_VIDEO_FPS, "video_fps", true},
    {METADATA_VIDEO_WIDTH, "video_width", true},
    {METADATA_VIDEO_HEIGHT, "video_height", true},
    {METADATA_HAS_VIDEO, "has_video", false},
    {METADATA_HAS_AUDIO, "has_audio", false},
    {METADATA_ARTIST, "artist", false},
    {METADATA_TITLE, "title", false},
    {METADATA_ALBUM, "album", false},
    {METADATA_ALBUM_ARTIST, "album_artist", false},
    {METADATA_GENRE, "genre", false},
    {METADATA_COMPOSER, "composer", false},
    {METADATA_DATE, "date", false},
    {METADATA_AUDIO_BITRATE, "audio_bitrate", true},
    {METADATA_AUDIO_CHANNELS, "audio_channels", true},
    {METADATA_AUDIO_SAMPLERATE, "audio_samplerate",
     true},
    {METADATA_ROTATE, "rotate", true},
    {METADATA_TRACK_NUM, "track_num", false},
};

}  // namespace

std::string MetadataController::GetMetadata(
    const std::string& file_path) const {
  metadata_extractor_h handle = nullptr;
  int ret = metadata_extractor_create(&handle);
  if (ret != 0)
    return "{\"error\": \"metadata_extractor_create\"}";

  ret = metadata_extractor_set_path(
      handle, file_path.c_str());
  if (ret != 0) {
    metadata_extractor_destroy(handle);
    return "{\"error\": "
           "\"metadata_extractor_set_path\"}";
  }

  std::vector<std::pair<std::string, std::string>>
      fields;
  fields.emplace_back("file_path",
                       "\"" + EscapeJson(file_path) +
                           "\"");

  for (const auto& a : kAttrs) {
    char* v = nullptr;
    ret = metadata_extractor_get_metadata(
        handle, a.attr, &v);
    if (ret != 0 || !v) continue;

    std::string val(v);
    free(v);
    if (val.empty()) continue;

    if (a.is_numeric) {
      fields.emplace_back(a.name, val);
    } else {
      fields.emplace_back(
          a.name, "\"" + EscapeJson(val) + "\"");
    }
  }

  // Duration formatted
  for (const auto& f : fields) {
    if (f.first == "duration") {
      try {
        int ms = std::stoi(f.second);
        int s = ms / 1000;
        std::string fmt = std::to_string(s / 60) +
                          ":" +
                          (s % 60 < 10 ? "0" : "") +
                          std::to_string(s % 60);
        fields.emplace_back("duration_formatted",
                            "\"" + fmt + "\"");
      } catch (...) {
      }
      break;
    }
  }

  metadata_extractor_destroy(handle);

  std::string result = "{";
  for (size_t i = 0; i < fields.size(); ++i) {
    if (i > 0) result += ", ";
    result +=
        "\"" + fields[i].first + "\": " +
        fields[i].second;
  }
  result += "}";
  return result;
}

}  // namespace cli
}  // namespace tizenclaw
