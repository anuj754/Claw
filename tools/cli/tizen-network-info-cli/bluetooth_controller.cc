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

#include "bluetooth_controller.hh"

#include <bluetooth.h>
#include <system_info.h>

#include <cstdlib>
#include <string>
#include <vector>

namespace tizenclaw {
namespace cli {

std::string BluetoothController::GetInfo() const {
  bool supported = false;
  system_info_get_platform_bool(
      "http://tizen.org/feature/network.bluetooth",
      &supported);
  if (!supported)
    return "{\"error\": \"Bluetooth not supported\"}";

  if (bt_initialize() != 0)
    return "{\"error\": \"bt_initialize failed\"}";

  bt_adapter_state_e state;
  bt_adapter_get_state(&state);
  bool active = (state == BT_ADAPTER_ENABLED);

  std::string name;
  std::string addr;
  if (active) {
    char* n = nullptr;
    if (bt_adapter_get_name(&n) == 0 && n) {
      name = n;
      free(n);
    }
    char* a = nullptr;
    if (bt_adapter_get_address(&a) == 0 && a) {
      addr = a;
      free(a);
    }
  }

  bt_deinitialize();

  return "{\"activated\": " +
         std::string(active ? "true" : "false") +
         ", \"name\": \"" + name +
         "\", \"address\": \"" + addr + "\"}";
}

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

struct BondedDevice {
  std::string name;
  std::string address;
  bool connected = false;
};

bool BondedDeviceCb(bt_device_info_s* info,
                    void* user_data) {
  if (!info) return false;
  auto* list =
      static_cast<std::vector<BondedDevice>*>(
          user_data);
  BondedDevice dev;
  if (info->remote_name)
    dev.name = info->remote_name;
  if (info->remote_address)
    dev.address = info->remote_address;
  dev.connected = info->is_connected;
  list->push_back(std::move(dev));
  return true;
}

}  // namespace

std::string
BluetoothController::ListBondedDevices() const {
  bool supported = false;
  system_info_get_platform_bool(
      "http://tizen.org/feature/"
      "network.bluetooth",
      &supported);
  if (!supported)
    return "{\"error\": "
           "\"Bluetooth not supported\"}";

  if (bt_initialize() != 0)
    return "{\"error\": \"bt_initialize\"}";

  std::vector<BondedDevice> devices;
  bt_adapter_foreach_bonded_device(
      BondedDeviceCb, &devices);

  bt_deinitialize();

  std::string result = "{\"devices\": [";
  for (size_t i = 0; i < devices.size(); ++i) {
    if (i > 0) result += ", ";
    result += "{\"name\": \"" +
              EscapeJson(devices[i].name) +
              "\", \"address\": \"" +
              EscapeJson(devices[i].address) +
              "\", \"connected\": " +
              (devices[i].connected ? "true"
                                    : "false") +
              "}";
  }
  result += "], \"count\": " +
            std::to_string(devices.size()) + "}";
  return result;
}

}  // namespace cli
}  // namespace tizenclaw
