#pragma once
//
// mcp_server.hpp  (RFC-0011 §6, Phases D + E)
//
// In-node MCP (Model Context Protocol) server. A thin JSON-RPC 2.0 adapter over
// the command registry, driven entirely by the capability manifest (RFC-0011 §5):
// tools/list is generated from the manifest, tools/call maps to the
// CommandRunner. No per-command MCP code.
//
// Transports:
//   stdio            `-mcp stdio` (runStdio).
//   HTTP (POST /mcp) `-mcp http`  (handleLine via the RPC server).
//   HTTP/SSE         GET /mcp opens a server->client event stream (SseSink +
//                    broadcast) for MCP notifications.
// Gating: GeneralParams::MCP_SERVER / MCP_STDIO.
//
// Note: the node's generated JSON lexer returns EOF for every input, so the
// request side uses a small self-contained parser rather than JSONParser. The
// response side uses the (working) JSON writer.

#include <memory>
#include <string>

namespace org {
namespace minima {
namespace system {
namespace commands {
namespace mcp {

// A server->client SSE channel (opaque). One per GET /mcp connection.
struct SseSink;

class McpServer {
public:
    // Run the MCP stdio loop: newline-delimited JSON-RPC 2.0 on stdin/stdout.
    // Blocking; returns when stdin closes or the node stops.
    static void runStdio();

    // Handle one JSON-RPC request line and return the response line (empty for
    // notifications, which must not be answered). Exposed for testing.
    static std::string handleLine(const std::string& line);

    // ---- SSE (server->client stream), RFC-0011 §6 Phase E ----
    static std::shared_ptr<SseSink> openSseSink();
    static void closeSseSink(const std::shared_ptr<SseSink>& sink);

    // Queue a JSON-RPC notification to every open sink.
    static void broadcast(const std::string& jsonrpcNotification);

    // Wait up to timeoutMs for the next queued message; returns false on timeout.
    static bool nextSseMessage(const std::shared_ptr<SseSink>& sink,
                               std::string& out, int timeoutMs);

    // Number of open sinks (testing/introspection).
    static std::size_t sseSinkCount();
};

} // namespace mcp
} // namespace commands
} // namespace system
} // namespace minima
} // namespace org
