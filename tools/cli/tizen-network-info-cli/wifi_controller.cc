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

#include "wifi_controller.hh"

#include <wifi-manager.h>

#include <cstdlib>
#include <algorithm>
#include <string>
#include <vector>

namespace tizenclaw {
namespace cli {

namespace {

constexpr const char* kStateNames[] = {
    "disconnected", "association", "configuration",
    "connected", "failure"};

}  // namespace

std::string WifiController::GetWifiInfo() const {
  wifi_manager_h mgr = nullptr;
  if (wifi_manager_initialize(&mgr) != 0)
    return "{\"error\": \"wifi_manager_initialize\"}";

  bool activated = false;
  wifi_manager_is_activated(mgr, &activated);

  std::string essid;
  std::string cs = "unknown";

  if (activated) {
    wifi_manager_connection_state_e state;
    wifi_manager_get_connection_state(mgr, &state);
    cs = (state <= 4) ? kStateNames[state] : "unknown";

    if (state ==
        WIFI_MANAGER_CONNECTION_STATE_CONNECTED) {
      wifi_manager_ap_h ap = nullptr;
      if (wifi_manager_get_connected_ap(
              mgr, &ap) == 0) {
        char* e = nullptr;
        if (wifi_manager_ap_get_essid(ap, &e) == 0 &&
            e) {
          essid = e;
          free(e);
        }

        wifi_manager_ap_destroy(ap);
      }
    }
  }

  wifi_manager_deinitialize(mgr);

  return "{\"activated\": " +
         std::string(activated ? "true" : "false") +
         ", \"connection_state\": \"" + cs +
         "\", \"essid\": \"" + essid + "\"}";
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

constexpr const char* kSecNames[] = {
    "none", "wep", "wpa_psk", "wpa2_psk",
    "wpa_eap", "wpa2_eap", "wpa_ftp",
    "wpa2_ftp", "wpa3_sae", "open_owe"};

struct ApEntry {
  std::string essid;
  int rssi = 0;
  int freq = 0;
  std::string security;
  int max_speed = 0;
};

bool FoundApCb(wifi_manager_ap_h ap,
               void* user_data) {
  auto* list =
      static_cast<std::vector<ApEntry>*>(user_data);
  ApEntry entry;

  char* e = nullptr;
  if (wifi_manager_ap_get_essid(ap, &e) == 0 && e) {
    entry.essid = e;
    free(e);
  }

  wifi_manager_ap_get_rssi(ap, &entry.rssi);
  wifi_manager_ap_get_frequency(ap, &entry.freq);
  wifi_manager_ap_get_max_speed(ap,
                                &entry.max_speed);

  wifi_manager_security_type_e sec;
  if (wifi_manager_ap_get_security_type(
          ap, &sec) == 0 &&
      sec <= 9) {
    entry.security = kSecNames[sec];
  } else {
    entry.security = "unknown";
  }

  list->push_back(std::move(entry));
  return true;
}

}  // namespace

std::string WifiController::ScanNetworks() const {
  wifi_manager_h mgr = nullptr;
  if (wifi_manager_initialize(&mgr) != 0)
    return "{\"error\": "
           "\"wifi_manager_initialize\"}";

  // Request a synchronous scan
  wifi_manager_scan(mgr, nullptr, nullptr);

  std::vector<ApEntry> aps;
  wifi_manager_foreach_found_ap(
      mgr, FoundApCb, &aps);

  wifi_manager_deinitialize(mgr);

  // Sort by RSSI descending
  std::sort(aps.begin(), aps.end(),
            [](const ApEntry& a, const ApEntry& b) {
              return a.rssi > b.rssi;
            });

  std::string result = "{\"networks\": [";
  for (size_t i = 0; i < aps.size(); ++i) {
    if (i > 0) result += ", ";
    result += "{\"essid\": \"" +
              EscapeJson(aps[i].essid) +
              "\", \"rssi_dbm\": " +
              std::to_string(aps[i].rssi) +
              ", \"frequency_mhz\": " +
              std::to_string(aps[i].freq) +
              ", \"max_speed_mbps\": " +
              std::to_string(aps[i].max_speed) +
              ", \"security\": \"" +
              aps[i].security + "\"}";
  }
  result += "], \"count\": " +
            std::to_string(aps.size()) + "}";
  return result;
}

}  // namespace cli
}  // namespace tizenclaw
