#ifndef MCP_HTTP_CLIENT_HH
#define MCP_HTTP_CLIENT_HH

#include <json.hpp>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>

namespace tizenclaw {

// MCP client that communicates over HTTP (Streamable HTTP transport).
// Each method call is an independent HTTP POST to the configured endpoint.
class McpHttpClient {
 public:
  struct ToolInfo {
    std::string name;
    std::string description;
    nlohmann::json input_schema;
  };

  McpHttpClient(const std::string& server_name, const std::string& url,
                const std::string& access_token, int timeout_ms = 30000);

  // Performs the MCP initialize handshake to verify the endpoint is reachable.
  bool Connect();

  // No-op for HTTP transport (stateless).
  void Disconnect();

  // Fetches the tools/list from the remote HTTP MCP server.
  std::vector<ToolInfo> GetTools();

  // Invokes tools/call on the remote HTTP MCP server.
  nlohmann::json CallTool(const std::string& tool_name,
                          const nlohmann::json& arguments);

  const std::string& GetServerName() const { return server_name_; }
  bool IsConnected() const { return is_connected_; }

 private:
  std::string server_name_;
  std::string url_;
  std::string access_token_;
  int timeout_ms_;

  std::atomic<bool> is_connected_{false};
  std::atomic<int> next_req_id_{1};
  std::mutex mutex_;

  // Sends a JSON-RPC request via HTTP POST and returns the parsed response.
  nlohmann::json PostRequest(const std::string& method,
                             const nlohmann::json& params);
};

}  // namespace tizenclaw

#endif  // MCP_HTTP_CLIENT_HH
