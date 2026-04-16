/*
 * Copyright (c) 2026 Samsung Electronics Co., Ltd All Rights Reserved
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 */

#ifndef CANVAS_IPC_SERVER_HH
#define CANVAS_IPC_SERVER_HH

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <json.hpp>

namespace tizenclaw {

class CanvasIpcServer {
 public:
  CanvasIpcServer();
  ~CanvasIpcServer();

  // Non-copyable/movable
  CanvasIpcServer(const CanvasIpcServer&) = delete;
  CanvasIpcServer& operator=(const CanvasIpcServer&) = delete;

  // Initialize and start the UDS server thread
  bool Start();

  // Stop the server
  void Stop();

  // Broadcast agent state to connected Canvas clients
  void BroadcastState(const std::string& state, const std::string& content);

  // Broadcast a tool execution result to connected Canvas clients.
  // Sends a dedicated "tool_result" event with structured fields so the NUI
  // application can display each tool's output as it completes.
  void BroadcastToolResult(const std::string& tool_name,
                           const std::string& session_id,
                           const nlohmann::json& result);

  // Broadcast an A2UI-processed result to connected Canvas clients.
  // Sends an "a2ui_result" event carrying the structured UI descriptor
  // produced by A2UIAgent from the raw tool result. The NUI application
  // uses this to render typed UI components (view, list, chart, text, action)
  // without needing per-tool rendering logic.
  void BroadcastA2UIResult(const std::string& tool_name,
                           const std::string& session_id,
                           const nlohmann::json& a2ui_json);

 private:
  void ServerLoop();
  void RemoveClient(int fd);

  int server_fd_ = -1;
  std::atomic<bool> running_{false};
  std::thread server_thread_;

  std::mutex clients_mutex_;
  std::vector<int> client_fds_;

  static constexpr const char* kSocketPath = "/run/tizenclaw/canvas.sock";
};

}  // namespace tizenclaw

#endif  // CANVAS_IPC_SERVER_HH
