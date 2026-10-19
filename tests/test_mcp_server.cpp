/**
 * test_mcp_server.cpp  (RFC-0011 §6, Phase D)
 *
 * Asserts the in-node MCP server: initialize / tools/list / tools/call are
 * generated from the capability manifest and dispatch to the CommandRunner.
 * Links the shared node object library (it needs the full command registry).
 */

#include <iostream>
#include <string>

#include "org/minima/system/commands/mcp/mcp_server.hpp"

using org::minima::system::commands::mcp::McpServer;

static int g_pass = 0, g_total = 0;
static void check(bool ok, const std::string& what) {
    ++g_total;
    if (ok) ++g_pass;
    std::cout << "  " << (ok ? "ok  " : "FAIL") << " " << what << "\n";
}
static bool has(const std::string& s, const std::string& sub) {
    return s.find(sub) != std::string::npos;
}

int main() {
    std::cout << std::unitbuf;
    std::cout << "=== in-node MCP server (RFC-0011 §6, Phase D) ===\n\n";

    // initialize
    {
        std::string r = McpServer::handleLine(
            "{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{\"protocolVersion\":\"2024-11-05\"}}");
        check(has(r, "\"id\":1") && has(r, "\"result\""), "initialize returns a result with the id");
        check(has(r, "\"protocolVersion\":\"2024-11-05\""), "initialize advertises the protocol version");
        check(has(r, "\"serverInfo\"") && has(r, "\"minima-more\""), "initialize advertises serverInfo");
        check(has(r, "\"capabilities\"") && has(r, "\"tools\""), "initialize advertises tools capability");
    }

    // notification: no response
    {
        std::string r = McpServer::handleLine(
            "{\"jsonrpc\":\"2.0\",\"method\":\"notifications/initialized\"}");
        check(r.empty(), "a notification (no id) produces no response");
    }

    // tools/list: generated from the manifest
    {
        std::string r = McpServer::handleLine("{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/list\"}");
        check(has(r, "\"tools\""), "tools/list returns tools");
        check(has(r, "\"name\":\"status\""), "tools/list includes the status tool");
        check(has(r, "\"name\":\"getaddress\""), "tools/list includes the getaddress tool");
        check(has(r, "\"inputSchema\""), "tools carry an inputSchema");
        check(has(r, "\"annotations\""), "tools carry annotations");
    }

    // tools/call: dispatches to the runner (DB-free command)
    {
        std::string r = McpServer::handleLine(
            "{\"jsonrpc\":\"2.0\",\"id\":3,\"method\":\"tools/call\",\"params\":{\"name\":\"capabilities\",\"arguments\":{}}}");
        check(has(r, "\"content\"") && has(r, "\"type\":\"text\""), "tools/call returns text content");
        check(has(r, "\"isError\":false"), "capabilities tool call is not an error");
        check(has(r, "commands"), "tools/call returned the manifest");
    }

    // tools/call with a bad tool name -> error content
    {
        std::string r = McpServer::handleLine(
            "{\"jsonrpc\":\"2.0\",\"id\":4,\"method\":\"tools/call\",\"params\":{\"name\":\"no_such_tool\",\"arguments\":{}}}");
        check(has(r, "\"isError\":true"), "unknown tool is an error");
    }

    // unknown method -> JSON-RPC method-not-found
    {
        std::string r = McpServer::handleLine("{\"jsonrpc\":\"2.0\",\"id\":9,\"method\":\"nope\"}");
        check(has(r, "-32601"), "unknown method returns -32601");
    }

    // malformed JSON -> parse error
    {
        std::string r = McpServer::handleLine("{not json");
        check(has(r, "-32700"), "malformed request returns -32700");
    }

    // SSE (server->client) channel
    {
        check(McpServer::sseSinkCount() == 0, "no SSE sinks initially");
        auto sink = McpServer::openSseSink();
        check(sink != nullptr && McpServer::sseSinkCount() == 1, "SSE sink opens");

        std::string out;
        check(!McpServer::nextSseMessage(sink, out, 50), "empty SSE sink times out");

        McpServer::broadcast("{\"jsonrpc\":\"2.0\",\"method\":\"notifications/message\"}");
        check(McpServer::nextSseMessage(sink, out, 200), "SSE sink receives a broadcast");
        check(has(out, "notifications/message"), "SSE message is the notification");

        McpServer::closeSseSink(sink);
        check(McpServer::sseSinkCount() == 0, "SSE sink closes");
        McpServer::broadcast("{\"jsonrpc\":\"2.0\",\"method\":\"x\"}");
        check(!McpServer::nextSseMessage(sink, out, 50), "closed SSE sink gets nothing");
    }

    std::cout << "\n=== " << g_pass << "/" << g_total << " checks passed ===\n";
    return (g_pass == g_total) ? 0 : 1;
}
