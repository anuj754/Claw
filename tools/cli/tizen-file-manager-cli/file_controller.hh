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

#ifndef FILE_CONTROLLER_HH_
#define FILE_CONTROLLER_HH_

#include <string>

namespace tizenclaw {
namespace cli {

class FileController {
 public:
  std::string Read(const std::string& path);
  std::string Write(const std::string& path,
                    const std::string& content);
  std::string Append(const std::string& path,
                     const std::string& content);
  std::string Remove(const std::string& path);
  std::string Mkdir(const std::string& path);
  std::string List(const std::string& path);
  std::string Stat(const std::string& path);
  std::string Copy(const std::string& src,
                   const std::string& dst);
  std::string Move(const std::string& src,
                   const std::string& dst);
  std::string Download(const std::string& url,
                       const std::string& dest);
};

}  // namespace cli
}  // namespace tizenclaw

#endif  // FILE_CONTROLLER_HH_
