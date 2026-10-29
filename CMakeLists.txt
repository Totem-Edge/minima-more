cmake_minimum_required(VERSION 3.16)

project(minima VERSION 0.5.8 LANGUAGES CXX)

include(CTest)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(MINIMA_STATIC "Fully static link (requires static deps)" OFF)
option(MINIMA_BUILD_ANALYTICS "Build the optional minima-analytics sidecar" OFF)
option(MINIMA_STATIC_NO_LMDB "Disable LMDB for fully static builds" OFF)

set(MINIMA_SOURCES
    org/minima/database/minima_d_b.cpp
    org/minima/database/state_report.cpp
    org/minima/database/archive/archive_manager.cpp
    org/minima/database/archive/raw_archive_input.cpp
    org/minima/database/archive/h2_sql_import.cpp
    org/minima/database/archive/tx_block_d_b.cpp
    org/minima/database/cascade/cascade.cpp
    org/minima/database/cascade/cascade_node.cpp
    org/minima/database/txpowdb/tx_po_w_d_b.cpp
    org/minima/database/txpowdb/onchain/tx_po_w_on_chain_d_b.cpp
    org/minima/database/txpowdb/ram/ram_data.cpp
    org/minima/database/txpowdb/ram/ram_d_b.cpp
    org/minima/database/txpowdb/sql/tx_po_w_list.cpp
    org/minima/database/txpowdb/sql/tx_po_w_sql_d_b.cpp
    org/minima/database/txpowtree/tx_pow_tree.cpp
    org/minima/database/txpowtree/tx_po_w_tree_node.cpp
    org/minima/database/txpowtree/tx_po_w_tree_node_action.cpp
    org/minima/database/userprefs/user_d_b.cpp
    org/minima/database/userprefs/txndb/txn_d_b.cpp
    org/minima/database/userprefs/txndb/txn_row.cpp
    org/minima/database/wallet/key_row.cpp
    org/minima/database/wallet/script_row.cpp
    org/minima/database/wallet/seed_row.cpp
    org/minima/database/wallet/wallet.cpp
    org/minima/kissvm/contract.cpp
    org/minima/kissvm/exceptions/execution_exception.cpp
    org/minima/kissvm/exceptions/minima_exception.cpp
    org/minima/kissvm/exceptions/minima_parse_exception.cpp
    org/minima/kissvm/expressions/boolean_expression.cpp
    org/minima/kissvm/expressions/constant_expression.cpp
    org/minima/kissvm/expressions/expression.cpp
    org/minima/kissvm/expressions/expression_parser.cpp
    org/minima/kissvm/expressions/function_expression.cpp
    org/minima/kissvm/expressions/global_expression.cpp
    org/minima/kissvm/expressions/operator_expression.cpp
    org/minima/kissvm/expressions/variable_expression.cpp
    org/minima/kissvm/functions/minima_function.cpp
    org/minima/kissvm/functions/cast/a_s_c_i_i.cpp
    org/minima/kissvm/functions/cast/b_o_o_l.cpp
    org/minima/kissvm/functions/cast/h_e_x.cpp
    org/minima/kissvm/functions/cast/n_u_m_b_e_r.cpp
    org/minima/kissvm/functions/cast/s_t_r_i_n_g.cpp
    org/minima/kissvm/functions/cast/u_t_f8.cpp
    org/minima/kissvm/functions/general/a_d_d_r_e_s_s.cpp
    org/minima/kissvm/functions/general/e_x_i_s_t_s.cpp
    org/minima/kissvm/functions/general/f_u_n_c_t_i_o_n.cpp
    org/minima/kissvm/functions/general/g_e_t.cpp
    org/minima/kissvm/functions/hex/b_i_t_c_o_u_n_t.cpp
    org/minima/kissvm/functions/hex/b_i_t_g_e_t.cpp
    org/minima/kissvm/functions/hex/b_i_t_s_e_t.cpp
    org/minima/kissvm/functions/hex/c_o_n_c_a_t.cpp
    org/minima/kissvm/functions/hex/l_e_n.cpp
    org/minima/kissvm/functions/hex/o_v_e_r_w_r_i_t_e.cpp
    org/minima/kissvm/functions/hex/r_e_v.cpp
    org/minima/kissvm/functions/hex/s_e_t_l_e_n.cpp
    org/minima/kissvm/functions/hex/s_u_b_s_e_t.cpp
    org/minima/kissvm/functions/number/a_b_s.cpp
    org/minima/kissvm/functions/number/c_e_i_l.cpp
    org/minima/kissvm/functions/number/d_e_c.cpp
    org/minima/kissvm/functions/number/f_l_o_o_r.cpp
    org/minima/kissvm/functions/number/i_n_c.cpp
    org/minima/kissvm/functions/number/m_a_x.cpp
    org/minima/kissvm/functions/number/m_i_n.cpp
    org/minima/kissvm/functions/number/p_o_w.cpp
    org/minima/kissvm/functions/number/s_i_g_d_i_g.cpp
    org/minima/kissvm/functions/number/s_q_r_t.cpp
    org/minima/kissvm/functions/sha/p_r_o_o_f.cpp
    org/minima/kissvm/functions/sha/s_h_a2.cpp
    org/minima/kissvm/functions/sha/s_h_a3.cpp
    org/minima/kissvm/functions/sigs/c_h_e_c_k_s_i_g.cpp
    org/minima/kissvm/functions/sigs/m_u_l_t_i_s_i_g.cpp
    org/minima/kissvm/functions/sigs/s_i_g_n_e_d_b_y.cpp
    org/minima/kissvm/functions/state/p_r_e_v_s_t_a_t_e.cpp
    org/minima/kissvm/functions/state/s_a_m_e_s_t_a_t_e.cpp
    org/minima/kissvm/functions/state/s_t_a_t_e.cpp
    org/minima/kissvm/functions/string/r_e_p_l_a_c_e.cpp
    org/minima/kissvm/functions/string/r_e_p_l_a_c_e_f_i_r_s_t.cpp
    org/minima/kissvm/functions/string/s_u_b_s_t_r.cpp
    org/minima/kissvm/functions/txn/input/g_e_t_i_n_a_d_d_r.cpp
    org/minima/kissvm/functions/txn/input/g_e_t_i_n_a_m_t.cpp
    org/minima/kissvm/functions/txn/input/g_e_t_i_n_i_d.cpp
    org/minima/kissvm/functions/txn/input/g_e_t_i_n_t_o_k.cpp
    org/minima/kissvm/functions/txn/input/s_u_m_i_n_p_u_t_s.cpp
    org/minima/kissvm/functions/txn/input/v_e_r_i_f_y_i_n.cpp
    org/minima/kissvm/functions/txn/output/g_e_t_o_u_t_a_d_d_r.cpp
    org/minima/kissvm/functions/txn/output/g_e_t_o_u_t_a_m_t.cpp
    org/minima/kissvm/functions/txn/output/g_e_t_o_u_t_k_e_e_p_s_t_a_t_e.cpp
    org/minima/kissvm/functions/txn/output/g_e_t_o_u_t_t_o_k.cpp
    org/minima/kissvm/functions/txn/output/s_u_m_o_u_t_p_u_t_s.cpp
    org/minima/kissvm/functions/txn/output/v_e_r_i_f_y_o_u_t.cpp
    org/minima/kissvm/statements/statement.cpp
    org/minima/kissvm/statements/statement_block.cpp
    org/minima/kissvm/statements/statement_parser.cpp
    org/minima/kissvm/statements/commands/a_s_s_e_r_tstatement.cpp
    org/minima/kissvm/statements/commands/e_x_e_cstatement.cpp
    org/minima/kissvm/statements/commands/i_fstatement.cpp
    org/minima/kissvm/statements/commands/l_e_tstatement.cpp
    org/minima/kissvm/statements/commands/m_a_s_tstatement.cpp
    org/minima/kissvm/statements/commands/r_e_t_u_r_nstatement.cpp
    org/minima/kissvm/statements/commands/w_h_i_l_estatement.cpp
    org/minima/kissvm/tokens/lexical_tokenizer.cpp
    org/minima/kissvm/tokens/script_token.cpp
    org/minima/kissvm/tokens/script_tokenizer.cpp
    org/minima/kissvm/values/boolean_value.cpp
    org/minima/kissvm/values/hex_value.cpp
    org/minima/kissvm/values/number_value.cpp
    org/minima/kissvm/values/string_value.cpp
    org/minima/kissvm/values/value.cpp
    org/minima/objects/address.cpp
    org/minima/objects/coin.cpp
    org/minima/objects/coin_proof.cpp
    org/minima/objects/greeting.cpp
    org/minima/objects/i_b_d.cpp
    org/minima/objects/magic.cpp
    org/minima/objects/pulse.cpp
    org/minima/objects/script_proof.cpp
    org/minima/objects/state_variable.cpp
    org/minima/objects/token.cpp
    org/minima/objects/transaction.cpp
    org/minima/objects/tx_block.cpp
    org/minima/objects/tx_body.cpp
    org/minima/objects/tx_header.cpp
    org/minima/objects/tx_po_w.cpp
    org/minima/objects/witness.cpp
    org/minima/objects/base/mini_byte.cpp
    org/minima/objects/base/mini_data.cpp
    org/minima/objects/base/mini_number.cpp
    org/minima/objects/base/mini_string.cpp
    org/minima/objects/keys/signature.cpp
    org/minima/objects/keys/signature_proof.cpp
    org/minima/objects/keys/tree_key.cpp
    org/minima/objects/keys/tree_key_node.cpp
    org/minima/objects/keys/winternitz.cpp
    org/minima/objects/mmr/mega_m_m_r.cpp
    org/minima/objects/coins/coin_store.cpp
    org/minima/objects/mmr/m_m_r.cpp
    org/minima/objects/mmr/m_m_r_data.cpp
    org/minima/objects/mmr/m_m_r_entry.cpp
    org/minima/objects/mmr/m_m_r_entry_number.cpp
    org/minima/objects/mmr/m_m_r_proof.cpp
    org/minima/system/main.cpp
    org/minima/system/brains/timed_checker.cpp
    org/minima/system/brains/tx_po_w_checker.cpp
    org/minima/system/brains/tx_po_w_generator.cpp
    org/minima/system/brains/tx_po_w_miner.cpp
    org/minima/system/brains/tx_po_w_processor.cpp
    org/minima/system/brains/tx_po_w_searcher.cpp
    org/minima/system/commands/command.cpp
    org/minima/system/commands/command_exception.cpp
    org/minima/system/commands/command_runner.cpp
    org/minima/system/commands/mcp/mcp_server.cpp
    org/minima/system/commands/help.cpp
    org/minima/system/commands/tutorial.cpp
    org/minima/system/commands/whitepaper.cpp
    org/minima/system/commands/backup/archive.cpp
    org/minima/system/commands/backup/backup.cpp
    org/minima/system/commands/backup/decryptbackup.cpp
    org/minima/system/commands/backup/mysql.cpp
    org/minima/system/commands/backup/mysqlcoins.cpp
    org/minima/system/commands/backup/reset.cpp
    org/minima/system/commands/backup/restore.cpp
    org/minima/system/commands/backup/restoresync.cpp
    org/minima/system/commands/backup/vault.cpp
    org/minima/system/commands/backup/mmrsync/megammr.cpp
    org/minima/system/commands/backup/mmrsync/megammrsync.cpp
    org/minima/system/commands/backup/mmrsync/mega_m_m_r_backup.cpp
    org/minima/system/commands/backup/mmrsync/mega_m_m_r_i_b_d.cpp
    org/minima/system/commands/backup/mmrsync/mega_m_m_r_sync_data.cpp
    org/minima/system/commands/base/automine.cpp
    org/minima/system/commands/base/balance.cpp
    org/minima/system/commands/base/block.cpp
    org/minima/system/commands/base/burn.cpp
    org/minima/system/commands/base/capabilities.cpp
    org/minima/system/commands/base/memreport.cpp
    org/minima/system/commands/base/checkaddress.cpp
    org/minima/system/commands/base/coincheck.cpp
    org/minima/system/commands/base/coinexport.cpp
    org/minima/system/commands/base/coinimport.cpp
    org/minima/system/commands/base/coinnotify.cpp
    org/minima/system/commands/base/cointrack.cpp
    org/minima/system/commands/base/consolidate.cpp
    org/minima/system/commands/base/convert.cpp
    org/minima/system/commands/base/debugflag.cpp
    org/minima/system/commands/base/getaddress.cpp
    org/minima/system/commands/base/hash.cpp
    org/minima/system/commands/base/hashtest.cpp
    org/minima/system/commands/base/healthcheck.cpp
    org/minima/system/commands/base/incentivecash.cpp
    org/minima/system/commands/base/logs.cpp
    org/minima/system/commands/base/magic.cpp
    org/minima/system/commands/base/maths.cpp
    org/minima/system/commands/base/mempool.cpp
    org/minima/system/commands/base/missingcmd.cpp
    org/minima/system/commands/base/mmrcreate.cpp
    org/minima/system/commands/base/mmrproof.cpp
    org/minima/system/commands/base/newaddress.cpp
    org/minima/system/commands/base/printmmr.cpp
    org/minima/system/commands/base/printtree.cpp
    org/minima/system/commands/base/quit.cpp
    org/minima/system/commands/base/random.cpp
    org/minima/system/commands/base/scanchain.cpp
    org/minima/system/commands/base/seedrandom.cpp
    org/minima/system/commands/base/slavenode.cpp
    org/minima/system/commands/base/status.cpp
    org/minima/system/commands/base/state.cpp
    org/minima/system/commands/base/systemcheck.cpp
    org/minima/system/commands/base/test.cpp
    org/minima/system/commands/base/timemilli.cpp
    org/minima/system/commands/base/tokencreate.cpp
    org/minima/system/commands/base/tokenvalidate.cpp
    org/minima/system/commands/base/trace.cpp
    org/minima/system/commands/base/tutorial.cpp
    org/minima/system/commands/network/connect.cpp
    org/minima/system/commands/network/disconnect.cpp
    org/minima/system/commands/network/message.cpp
    org/minima/system/commands/network/network.cpp
    org/minima/system/commands/network/nodecount.cpp
    org/minima/system/commands/network/p2pstate.cpp
    org/minima/system/commands/network/peers.cpp
    org/minima/system/commands/network/ping.cpp
    org/minima/system/commands/network/rpc.cpp
    org/minima/system/commands/network/webhooks.cpp
    org/minima/system/commands/scripts/newscript.cpp
    org/minima/system/commands/scripts/removescript.cpp
    org/minima/system/commands/scripts/runscript.cpp
    org/minima/system/commands/scripts/scripts.cpp
    org/minima/system/commands/search/coins.cpp
    org/minima/system/commands/search/history.cpp
    org/minima/system/commands/search/keys.cpp
    org/minima/system/commands/search/tokens.cpp
    org/minima/system/commands/search/txpow.cpp
    org/minima/system/commands/send/multisig.cpp
    org/minima/system/commands/send/multisigread.cpp
    org/minima/system/commands/send/send.cpp
    org/minima/system/commands/send/sendnosign.cpp
    org/minima/system/commands/send/sendpoll.cpp
    org/minima/system/commands/send/sendpost.cpp
    org/minima/system/commands/send/sendsign.cpp
    org/minima/system/commands/send/sendview.cpp
    org/minima/system/commands/send/wallet/consolidatefrom.cpp
    org/minima/system/commands/send/wallet/constructfrom.cpp
    org/minima/system/commands/send/wallet/createfrom.cpp
    org/minima/system/commands/send/wallet/postfrom.cpp
    org/minima/system/commands/send/wallet/rawfrom.cpp
    org/minima/system/commands/send/wallet/sendfrom.cpp
    org/minima/system/commands/send/wallet/signfrom.cpp
    org/minima/system/commands/sendpoll/send_poll_manager.cpp
    org/minima/system/commands/sendpoll/send_poll_message.cpp
    org/minima/system/commands/signatures/sign.cpp
    org/minima/system/commands/signatures/verify.cpp
    org/minima/system/commands/txn/txnaddamount.cpp
    org/minima/system/commands/txn/txnauto.cpp
    org/minima/system/commands/txn/txnbasics.cpp
    org/minima/system/commands/txn/txncheck.cpp
    org/minima/system/commands/txn/txnclear.cpp
    org/minima/system/commands/txn/txncoinlock.cpp
    org/minima/system/commands/txn/txncreate.cpp
    org/minima/system/commands/txn/txndelete.cpp
    org/minima/system/commands/txn/txnexport.cpp
    org/minima/system/commands/txn/txnimport.cpp
    org/minima/system/commands/txn/txninput.cpp
    org/minima/system/commands/txn/txnlist.cpp
    org/minima/system/commands/txn/txnlock.cpp
    org/minima/system/commands/txn/txnmine.cpp
    org/minima/system/commands/txn/txnminepost.cpp
    org/minima/system/commands/txn/txnmmr.cpp
    org/minima/system/commands/txn/txnoutput.cpp
    org/minima/system/commands/txn/txnpost.cpp
    org/minima/system/commands/txn/txnscript.cpp
    org/minima/system/commands/txn/txnsign.cpp
    org/minima/system/commands/txn/txnstate.cpp
    org/minima/system/commands/txn/txnutils.cpp
    org/minima/system/commands/txn/txnview.cpp
    org/minima/system/genesis/genesis_coin.cpp
    org/minima/system/genesis/genesis_m_m_r.cpp
    org/minima/system/genesis/genesis_tx_po_w.cpp
    org/minima/system/network/network_manager.cpp
    org/minima/system/network/minima/n_i_o_client.cpp
    org/minima/system/network/minima/connection_backpressure.cpp
    org/minima/system/network/minima/n_i_o_client_info.cpp
    org/minima/system/network/minima/n_i_o_manager.cpp
    org/minima/system/network/minima/n_i_o_message.cpp
    org/minima/system/network/minima/n_i_o_server.cpp
    org/minima/system/network/minima/n_i_o_traffic.cpp
    org/minima/system/network/minima/relay_policy.cpp
    org/minima/system/network/p2p/p2_p_d_b.cpp
    org/minima/system/network/p2p/p2_p_functions.cpp
    org/minima/system/network/p2p/p2_p_manager.cpp
    org/minima/system/network/p2p/p2_p_peers_checker.cpp
    org/minima/system/network/p2p/p2_p_state.cpp
    org/minima/system/network/p2p/swap_links_functions.cpp
    org/minima/system/network/p2p/util_funcs.cpp
    org/minima/system/network/p2p/walk_links_funcs.cpp
    org/minima/system/network/p2p/messages/inet_socket_address_i_o.cpp
    org/minima/system/network/p2p/messages/p2_p_do_swap.cpp
    org/minima/system/network/p2p/messages/p2_p_greeting.cpp
    org/minima/system/network/p2p/messages/p2_p_swap_link.cpp
    org/minima/system/network/p2p/messages/p2_p_walk_links.cpp
    org/minima/system/network/p2p/params/p2_p_params.cpp
    org/minima/system/network/p2p/params/p2_p_test_params.cpp
    org/minima/system/network/p2p2/p2_p2_d_b.cpp
    org/minima/system/network/p2p2/p2_p2_functions.cpp
    org/minima/system/network/p2p2/p2_p2_manager.cpp
    org/minima/system/network/rpc/authorizer.cpp
    org/minima/system/network/rpc/c_m_d_handler.cpp
    org/minima/system/network/rpc/h_t_t_p_server.cpp
    org/minima/system/network/rpc/h_t_t_p_s_server.cpp
    org/minima/system/network/rpc/server.cpp
    org/minima/system/network/webhooks/notify_manager.cpp
    org/minima/system/params/general_params.cpp
    org/minima/system/params/global_params.cpp
    org/minima/system/params/param_configurer.cpp
    org/minima/system/params/test_params.cpp
    org/minima/utils/base_converter.cpp
    org/minima/utils/b_i_p39.cpp
    org/minima/utils/crypto.cpp
    org/minima/utils/fast_byte_array_stream.cpp
    org/minima/utils/json_d_b.cpp
    org/minima/utils/maths.cpp
    org/minima/utils/minima_logger.cpp
    org/minima/utils/file_downloader.cpp
    org/minima/utils/minima_r_p_c_client.cpp
    org/minima/utils/minima_uncaught_exception.cpp
    org/minima/utils/mini_file.cpp
    org/minima/utils/mini_format.cpp
    org/minima/utils/mini_util.cpp
    org/minima/utils/r_p_c_client.cpp
    org/minima/utils/sql_d_b.cpp
    org/minima/utils/stack.cpp
    org/minima/utils/streamable.cpp
    org/minima/utils/timed_request.cpp
    $<$<BOOL:${HAS_LIBZIP}>:org/minima/utils/zip_extractor.cpp>
    org/minima/utils/encrypt/crypto_package.cpp
    org/minima/utils/encrypt/encrypt_decrypt.cpp
    org/minima/utils/encrypt/generate_key.cpp
    org/minima/utils/encrypt/password_crypto.cpp
    org/minima/utils/encrypt/sign_verify.cpp
    org/minima/utils/encrypt/javajs/aes_util.cpp
    org/minima/utils/json/item_list.cpp
    org/minima/utils/json/j_s_o_n_array.cpp
    org/minima/utils/json/j_s_o_n_aware.cpp
    org/minima/utils/json/j_s_o_n_object.cpp
    org/minima/utils/json/j_s_o_n_stream_aware.cpp
    org/minima/utils/json/j_s_o_n_value.cpp
    org/minima/utils/json/j_s_o_n_writer.cpp
    org/minima/utils/json/parser/container_factory.cpp
    org/minima/utils/json/parser/content_handler.cpp
    org/minima/utils/json/parser/j_s_o_n_parser.cpp
    org/minima/utils/json/parser/parse_exception.cpp
    org/minima/utils/json/parser/yylex.cpp
    org/minima/utils/json/parser/yytoken.cpp
    org/minima/utils/messages/message.cpp
    org/minima/utils/messages/message_listener.cpp
    org/minima/utils/messages/message_processor.cpp
    org/minima/utils/messages/message_stack.cpp
    org/minima/utils/messages/timer_message.cpp
    org/minima/utils/messages/timer_processor.cpp
    org/minima/utils/mysql/archive_server.cpp
    org/minima/utils/mysql/my_s_q_l_connect.cpp
    org/minima/utils/ssl/minima_trust_manager.cpp
    org/minima/utils/ssl/self_signed_cert_generator.cpp
    org/minima/utils/ssl/s_s_l_manager.cpp
)

# Shared object library: the node executable and the manifest/MCP tests compile
# the same sources once.
add_library(minima_core OBJECT ${MINIMA_SOURCES})
target_compile_definitions(minima_core PUBLIC TOTEM_NODE_VERSION="${PROJECT_VERSION}")

add_executable(minima
    org/minima/minima.cpp
    $<TARGET_OBJECTS:minima_core>
)

set_target_properties(minima PROPERTIES
    WIN32_EXECUTABLE OFF
    MACOSX_BUNDLE OFF
)

target_compile_features(minima PRIVATE cxx_std_17)
target_include_directories(minima PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Apple Silicon Homebrew prefix
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
    target_include_directories(minima PRIVATE /opt/homebrew/include)
endif()

if(MSVC)
    target_compile_options(minima PRIVATE /W4 /permissive-)
    target_compile_definitions(minima PRIVATE _CRT_SECURE_NO_WARNINGS WIN32_LEAN_AND_MEAN NOMINMAX _WINSOCK_DEPRECATED_NO_WARNINGS)
else()
    target_compile_options(minima PRIVATE -Wall -Wextra -Wpedantic)
    # Linux / Unix does not use -mconsole, only Windows does
    if(WIN32)
        target_link_options(minima PRIVATE -mconsole -Wl,--stack,8388608)
    endif()
endif()

find_package(Threads REQUIRED)
target_link_libraries(minima PRIVATE Threads::Threads)

if(WIN32)
    target_link_libraries(minima PRIVATE ws2_32 bcrypt crypt32 iphlpapi)
elseif(APPLE)
    # Add Apple-specific frameworks here if needed
else()
    find_library(DL_LIBRARY dl)
    if(DL_LIBRARY)
        target_link_libraries(minima PRIVATE ${DL_LIBRARY})
    endif()
endif()

# SQLite3 (H2 Database -> SQLite)
find_package(SQLite3 REQUIRED)
target_link_libraries(minima PRIVATE SQLite::SQLite3)

# Botan was listed as a dependency but is never used in any source file.
# The code uses OpenSSL for crypto and Boost for big integers, not Botan.

# OpenSSL (for crypto, SSL, signature verification)
find_package(OpenSSL REQUIRED)
if(NOT MINIMA_STATIC)
    target_link_libraries(minima PRIVATE OpenSSL::SSL OpenSSL::Crypto)
endif()

# zlib (for password_crypto.cpp, mini_file.cpp)
find_package(ZLIB REQUIRED)
target_link_directories(minima PRIVATE ${ZLIB_LIBRARIES_DIR})
target_include_directories(minima PRIVATE ${ZLIB_INCLUDE_DIRS})
if(NOT MINIMA_STATIC)
    target_link_libraries(minima PRIVATE ZLIB::ZLIB)
endif()

# libcurl (for r_p_c_client.cpp)
find_package(CURL REQUIRED)
if(NOT MINIMA_STATIC)
    target_link_libraries(minima PRIVATE CURL::libcurl)
endif()

# LMDB (RFC-0001 R2 coin store: mmap-backed UTXO/coin store). Optional: without
# it MegaMMR falls back to its in-memory coin map.
if(NOT MINIMA_STATIC_NO_LMDB)
find_path(LMDB_INCLUDE_DIR lmdb.h)
find_library(LMDB_LIBRARY lmdb)
if(LMDB_INCLUDE_DIR AND LMDB_LIBRARY)
    target_compile_definitions(minima PRIVATE MINIMA_HAVE_LMDB)
    target_include_directories(minima PRIVATE ${LMDB_INCLUDE_DIR})
    target_link_libraries(minima PRIVATE ${LMDB_LIBRARY})
    target_compile_definitions(minima_core PRIVATE MINIMA_HAVE_LMDB)
    target_include_directories(minima_core PRIVATE ${LMDB_INCLUDE_DIR})
    target_link_libraries(minima_core PRIVATE ${LMDB_LIBRARY})
    message(STATUS "minima: using LMDB (${LMDB_LIBRARY})")
else()
    message(STATUS "minima: LMDB not found - MegaMMR coin store uses memory")
endif()
endif()

# Fully static link. Static curl must precede its OpenSSL/zlib deps; the early
# target links above are skipped in this mode so CMake cannot reorder/dedupe.
if(MINIMA_STATIC)
    target_link_options(minima PRIVATE -static)
    target_link_libraries(minima PRIVATE
        "${CURL_LIBRARY}"
        "${OPENSSL_SSL_LIBRARY}"
        "${OPENSSL_CRYPTO_LIBRARY}"
        "${ZLIB_LIBRARY}")
    if(LMDB_LIBRARY)
        target_link_libraries(minima PRIVATE "${LMDB_LIBRARY}")
    endif()
    if(UNIX AND NOT APPLE)
        target_link_libraries(minima PRIVATE m)
    endif()
endif()

# Optional allocator: jemalloc measurably reduces steady-state RSS (freed
# key-generation memory is returned to the OS instead of retained by glibc).
option(MINIMA_USE_JEMALLOC "Link jemalloc when available" OFF)
if(MINIMA_USE_JEMALLOC)
    find_library(JEMALLOC_LIBRARY jemalloc)
    if(JEMALLOC_LIBRARY)
        target_link_libraries(minima PRIVATE ${JEMALLOC_LIBRARY})
        message(STATUS "minima: using jemalloc (${JEMALLOC_LIBRARY})")
    else()
        message(WARNING "MINIMA_USE_JEMALLOC requested but jemalloc not found")
    endif()
endif()

# libzip (for zip_extractor.cpp) — optional, not available on all platforms
find_package(libzip QUIET)
set(HAS_LIBZIP FALSE)
if(libzip_FOUND OR TARGET libzip::zip)
    if(TARGET libzip::zip)
        target_link_libraries(minima PRIVATE libzip::zip)
    else()
        target_link_libraries(minima PRIVATE ${LIBZIP_LIBRARIES})
        target_include_directories(minima PRIVATE ${LIBZIP_INCLUDE_DIRS})
    endif()
    target_compile_definitions(minima PRIVATE HAS_LIBZIP)
    set(HAS_LIBZIP TRUE)
else()
    message(WARNING "libzip not found — archive/zip features will be disabled. Install libzip for full functionality.")
endif()

# MySQL Connector/C++ 8 — optional, only needed for MySQL archive server
find_package(mysql-connector-cpp CONFIG QUIET)
set(MYSQLCPP_OK FALSE)
if(TARGET MySQL::mysqlcppconn8)
    target_link_libraries(minima PRIVATE MySQL::mysqlcppconn8)
    set(MYSQLCPP_OK TRUE)
elseif(TARGET mysqlcppconn8)
    target_link_libraries(minima PRIVATE mysqlcppconn8)
    set(MYSQLCPP_OK TRUE)
elseif(TARGET MySQL::mysqlcppconn)
    target_link_libraries(minima PRIVATE MySQL::mysqlcppconn)
    set(MYSQLCPP_OK TRUE)
elseif(TARGET mysqlcppconn)
    target_link_libraries(minima PRIVATE mysqlcppconn)
    set(MYSQLCPP_OK TRUE)
endif()
if(MYSQLCPP_OK)
    target_compile_definitions(minima PRIVATE HAS_MYSQL)
else()
    message(WARNING "MySQL Connector/C++ not found — MySQL archive features will be disabled. Install mysql-connector-c++ for full functionality.")
endif()

# Compile settings for the shared object library (tests reuse its objects).
target_compile_features(minima_core PRIVATE cxx_std_17)
target_include_directories(minima_core PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
    target_include_directories(minima_core PRIVATE /opt/homebrew/include)
endif()
if(MSVC)
    target_compile_options(minima_core PRIVATE /W4 /permissive-)
    target_compile_definitions(minima_core PRIVATE _CRT_SECURE_NO_WARNINGS WIN32_LEAN_AND_MEAN NOMINMAX _WINSOCK_DEPRECATED_NO_WARNINGS)
else()
    target_compile_options(minima_core PRIVATE -Wall -Wextra -Wpedantic)
endif()
target_include_directories(minima_core PRIVATE ${ZLIB_INCLUDE_DIRS})
if(TARGET OpenSSL::SSL)
    target_link_libraries(minima_core PRIVATE OpenSSL::SSL OpenSSL::Crypto)
endif()
if(TARGET SQLite::SQLite3)
    target_link_libraries(minima_core PRIVATE SQLite::SQLite3)
endif()
if(TARGET CURL::libcurl)
    target_link_libraries(minima_core PRIVATE CURL::libcurl)
endif()
if(TARGET ZLIB::ZLIB)
    target_link_libraries(minima_core PRIVATE ZLIB::ZLIB)
endif()
if(HAS_LIBZIP)
    target_compile_definitions(minima_core PRIVATE HAS_LIBZIP)
    target_include_directories(minima_core PRIVATE ${LIBZIP_INCLUDE_DIRS})
endif()
if(MYSQLCPP_OK)
    target_compile_definitions(minima_core PRIVATE HAS_MYSQL)
endif()
target_link_libraries(minima_core PRIVATE Threads::Threads)

# Optional tests (JUnit -> GoogleTest)
if(BUILD_TESTING)
    find_package(GTest QUIET)
    # No test sources provided; users can add tests linking against GTest::gtest(GTest::gtest_main) if desired.
endif()

# Standalone Winternitz self-test - standalone to avoid deep project dependencies
add_executable(test_winternitz
    EXCLUDE_FROM_ALL
    tests/test_winternitz.cpp
)
target_compile_features(test_winternitz PRIVATE cxx_std_17)
target_link_libraries(test_winternitz PRIVATE OpenSSL::Crypto)

# Standalone serialization parity test - tests base primitives (MiniNumber, MiniData, MiniByte, MiniString)
add_executable(test_serialization
    EXCLUDE_FROM_ALL
    tests/test_serialization.cpp
    org/minima/objects/base/mini_data.cpp
    org/minima/objects/base/mini_number.cpp
    org/minima/objects/base/mini_byte.cpp
    org/minima/objects/base/mini_string.cpp
    org/minima/utils/streamable.cpp
    org/minima/utils/crypto.cpp
    org/minima/utils/fast_byte_array_stream.cpp
    org/minima/utils/base_converter.cpp
    org/minima/utils/json/j_s_o_n_aware.cpp
    org/minima/utils/json/j_s_o_n_array.cpp
    org/minima/utils/json/j_s_o_n_object.cpp
    org/minima/utils/json/j_s_o_n_stream_aware.cpp
    org/minima/utils/json/j_s_o_n_value.cpp
    org/minima/utils/json/j_s_o_n_writer.cpp
    org/minima/utils/json/item_list.cpp
    org/minima/utils/json/parser/container_factory.cpp
    org/minima/utils/json/parser/content_handler.cpp
    org/minima/utils/json/parser/j_s_o_n_parser.cpp
    org/minima/utils/json/parser/parse_exception.cpp
    org/minima/utils/json/parser/yylex.cpp
    org/minima/utils/json/parser/yytoken.cpp
    tests/stubs.cpp


)
target_compile_features(test_serialization PRIVATE cxx_std_17)
target_include_directories(test_serialization PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(APPLE)
    target_include_directories(test_serialization PRIVATE /opt/homebrew/include)
endif()
target_link_libraries(test_serialization PRIVATE OpenSSL::Crypto Threads::Threads)

# signtoken verification: tokencreate signs the create-output coin ID, not the
# token descriptor's 0x00 placeholder.
add_executable(test_signtoken_coinid
    EXCLUDE_FROM_ALL
    tests/test_signtoken_coinid.cpp
)
target_compile_features(test_signtoken_coinid PRIVATE cxx_std_17)
target_include_directories(test_signtoken_coinid PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(APPLE)
    target_include_directories(test_signtoken_coinid PRIVATE /opt/homebrew/include)
endif()
target_link_libraries(test_signtoken_coinid PRIVATE minima_core OpenSSL::Crypto Threads::Threads)
add_test(NAME test_signtoken_coinid COMMAND test_signtoken_coinid)

# Standalone MMR correctness test - exercises peak/root caching without heavy deps
add_executable(test_mmr
    EXCLUDE_FROM_ALL
    tests/test_mmr.cpp
    org/minima/objects/mmr/m_m_r.cpp
    org/minima/objects/mmr/m_m_r_data.cpp
    org/minima/objects/mmr/m_m_r_entry.cpp
    org/minima/objects/mmr/m_m_r_entry_number.cpp
    org/minima/objects/mmr/m_m_r_proof.cpp
    org/minima/objects/base/mini_data.cpp
    org/minima/objects/base/mini_number.cpp
    org/minima/objects/base/mini_byte.cpp
    org/minima/objects/base/mini_string.cpp
    org/minima/utils/crypto.cpp
    org/minima/utils/streamable.cpp
    org/minima/utils/fast_byte_array_stream.cpp
    org/minima/utils/base_converter.cpp
    org/minima/system/params/general_params.cpp
    org/minima/utils/json/j_s_o_n_aware.cpp
    org/minima/utils/json/j_s_o_n_array.cpp
    org/minima/utils/json/j_s_o_n_object.cpp
    org/minima/utils/json/j_s_o_n_stream_aware.cpp
    org/minima/utils/json/j_s_o_n_value.cpp
    org/minima/utils/json/j_s_o_n_writer.cpp
    org/minima/utils/json/item_list.cpp
    org/minima/utils/json/parser/container_factory.cpp
    org/minima/utils/json/parser/content_handler.cpp
    org/minima/utils/json/parser/j_s_o_n_parser.cpp
    org/minima/utils/json/parser/parse_exception.cpp
    org/minima/utils/json/parser/yylex.cpp
    org/minima/utils/json/parser/yytoken.cpp
    tests/stubs.cpp
)
target_compile_features(test_mmr PRIVATE cxx_std_17)
target_include_directories(test_mmr PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(APPLE)
    target_include_directories(test_mmr PRIVATE /opt/homebrew/include)
endif()
target_link_libraries(test_mmr PRIVATE OpenSSL::Crypto Threads::Threads)

# Standalone consensus golden-vector verification test - guards consensus-critical
# values (genesis, addresses, coin-id derivation, WOTS public key/signature).
add_executable(test_consensus_vectors
    EXCLUDE_FROM_ALL
    tests/test_consensus_vectors.cpp
    org/minima/objects/address.cpp
    org/minima/objects/coin.cpp
    org/minima/objects/token.cpp
    org/minima/objects/state_variable.cpp
    org/minima/objects/keys/winternitz.cpp
    org/minima/objects/mmr/m_m_r.cpp
    org/minima/objects/mmr/m_m_r_data.cpp
    org/minima/objects/mmr/m_m_r_entry.cpp
    org/minima/objects/mmr/m_m_r_entry_number.cpp
    org/minima/objects/mmr/m_m_r_proof.cpp
    org/minima/system/genesis/genesis_coin.cpp
    org/minima/system/genesis/genesis_m_m_r.cpp
    org/minima/objects/base/mini_data.cpp
    org/minima/objects/base/mini_number.cpp
    org/minima/objects/base/mini_byte.cpp
    org/minima/objects/base/mini_string.cpp
    org/minima/utils/crypto.cpp
    org/minima/utils/streamable.cpp
    org/minima/utils/fast_byte_array_stream.cpp
    org/minima/utils/base_converter.cpp
    org/minima/system/params/general_params.cpp
    org/minima/system/params/global_params.cpp
    org/minima/utils/json/j_s_o_n_aware.cpp
    org/minima/utils/json/j_s_o_n_array.cpp
    org/minima/utils/json/j_s_o_n_object.cpp
    org/minima/utils/json/j_s_o_n_stream_aware.cpp
    org/minima/utils/json/j_s_o_n_value.cpp
    org/minima/utils/json/j_s_o_n_writer.cpp
    org/minima/utils/json/item_list.cpp
    org/minima/utils/json/parser/container_factory.cpp
    org/minima/utils/json/parser/content_handler.cpp
    org/minima/utils/json/parser/j_s_o_n_parser.cpp
    org/minima/utils/json/parser/parse_exception.cpp
    org/minima/utils/json/parser/yylex.cpp
    org/minima/utils/json/parser/yytoken.cpp
    tests/stubs.cpp
)
target_compile_features(test_consensus_vectors PRIVATE cxx_std_17)
target_include_directories(test_consensus_vectors PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(APPLE)
    target_include_directories(test_consensus_vectors PRIVATE /opt/homebrew/include)
endif()
target_link_libraries(test_consensus_vectors PRIVATE OpenSSL::Crypto Threads::Threads)

# Standalone WOTS signature benchmark: scalar vs batched+pooled (asserts identical output).
add_executable(bench_wots_signature
    EXCLUDE_FROM_ALL
    tests/mshs2/bench/bench_wots_signature.cpp
    org/minima/objects/keys/winternitz.cpp
    org/minima/objects/base/mini_data.cpp
    org/minima/objects/base/mini_number.cpp
    org/minima/objects/base/mini_byte.cpp
    org/minima/objects/base/mini_string.cpp
    org/minima/utils/crypto.cpp
    org/minima/utils/streamable.cpp
    org/minima/utils/fast_byte_array_stream.cpp
    org/minima/utils/base_converter.cpp
    org/minima/system/params/general_params.cpp
    org/minima/system/params/global_params.cpp
    org/minima/utils/json/j_s_o_n_aware.cpp
    org/minima/utils/json/j_s_o_n_array.cpp
    org/minima/utils/json/j_s_o_n_object.cpp
    org/minima/utils/json/j_s_o_n_stream_aware.cpp
    org/minima/utils/json/j_s_o_n_value.cpp
    org/minima/utils/json/j_s_o_n_writer.cpp
    org/minima/utils/json/item_list.cpp
    org/minima/utils/json/parser/container_factory.cpp
    org/minima/utils/json/parser/content_handler.cpp
    org/minima/utils/json/parser/j_s_o_n_parser.cpp
    org/minima/utils/json/parser/parse_exception.cpp
    org/minima/utils/json/parser/yylex.cpp
    org/minima/utils/json/parser/yytoken.cpp
    tests/stubs.cpp
)
target_compile_features(bench_wots_signature PRIVATE cxx_std_17)
target_include_directories(bench_wots_signature PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(APPLE)
    target_include_directories(bench_wots_signature PRIVATE /opt/homebrew/include)
endif()
target_link_libraries(bench_wots_signature PRIVATE OpenSSL::Crypto Threads::Threads)

# Standalone RamDB mempool coin-index equivalence test (add/remove/setInCascade/cleanDB/wipe).
add_executable(test_mempool_index
    EXCLUDE_FROM_ALL
    tests/test_mempool_index.cpp
    org/minima/database/txpowdb/ram/ram_d_b.cpp
    org/minima/database/txpowdb/ram/ram_data.cpp
    org/minima/objects/tx_po_w.cpp
    org/minima/objects/tx_body.cpp
    org/minima/objects/tx_header.cpp
    org/minima/objects/transaction.cpp
    org/minima/objects/witness.cpp
    org/minima/objects/coin.cpp
    org/minima/objects/coin_proof.cpp
    org/minima/objects/token.cpp
    org/minima/objects/script_proof.cpp
    org/minima/objects/magic.cpp
    org/minima/objects/state_variable.cpp
    org/minima/objects/address.cpp
    org/minima/objects/keys/signature.cpp
    org/minima/objects/keys/signature_proof.cpp
    org/minima/objects/keys/winternitz.cpp
    org/minima/objects/keys/tree_key.cpp
    org/minima/objects/keys/tree_key_node.cpp
    org/minima/objects/mmr/m_m_r_proof.cpp
    org/minima/objects/mmr/m_m_r.cpp
    org/minima/objects/mmr/m_m_r_data.cpp
    org/minima/objects/mmr/m_m_r_entry.cpp
    org/minima/objects/mmr/m_m_r_entry_number.cpp
    org/minima/objects/base/mini_data.cpp
    org/minima/objects/base/mini_number.cpp
    org/minima/objects/base/mini_byte.cpp
    org/minima/objects/base/mini_string.cpp
    org/minima/utils/crypto.cpp
    org/minima/utils/streamable.cpp
    org/minima/utils/fast_byte_array_stream.cpp
    org/minima/utils/base_converter.cpp
    org/minima/system/params/general_params.cpp
    org/minima/system/params/global_params.cpp
    org/minima/utils/json/j_s_o_n_aware.cpp
    org/minima/utils/json/j_s_o_n_array.cpp
    org/minima/utils/json/j_s_o_n_object.cpp
    org/minima/utils/json/j_s_o_n_stream_aware.cpp
    org/minima/utils/json/j_s_o_n_value.cpp
    org/minima/utils/json/j_s_o_n_writer.cpp
    org/minima/utils/json/item_list.cpp
    org/minima/utils/json/parser/container_factory.cpp
    org/minima/utils/json/parser/content_handler.cpp
    org/minima/utils/json/parser/j_s_o_n_parser.cpp
    org/minima/utils/json/parser/parse_exception.cpp
    org/minima/utils/json/parser/yylex.cpp
    org/minima/utils/json/parser/yytoken.cpp
    tests/stubs.cpp
)
target_compile_features(test_mempool_index PRIVATE cxx_std_17)
target_include_directories(test_mempool_index PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(APPLE)
    target_include_directories(test_mempool_index PRIVATE /opt/homebrew/include)
endif()
target_link_libraries(test_mempool_index PRIVATE OpenSSL::Crypto Threads::Threads)

# Standalone TxPoWSqlDB batched-write correctness test (batch boundaries / reopen).
add_executable(test_sql_batch
    EXCLUDE_FROM_ALL
    tests/test_sql_batch.cpp
    org/minima/database/txpowdb/sql/tx_po_w_sql_d_b.cpp
    org/minima/utils/sql_d_b.cpp
    org/minima/database/txpowdb/ram/ram_d_b.cpp
    org/minima/database/txpowdb/ram/ram_data.cpp
    org/minima/objects/tx_po_w.cpp
    org/minima/objects/tx_body.cpp
    org/minima/objects/tx_header.cpp
    org/minima/objects/transaction.cpp
    org/minima/objects/witness.cpp
    org/minima/objects/coin.cpp
    org/minima/objects/coin_proof.cpp
    org/minima/objects/token.cpp
    org/minima/objects/script_proof.cpp
    org/minima/objects/magic.cpp
    org/minima/objects/state_variable.cpp
    org/minima/objects/address.cpp
    org/minima/objects/keys/signature.cpp
    org/minima/objects/keys/signature_proof.cpp
    org/minima/objects/keys/winternitz.cpp
    org/minima/objects/keys/tree_key.cpp
    org/minima/objects/keys/tree_key_node.cpp
    org/minima/objects/mmr/m_m_r_proof.cpp
    org/minima/objects/mmr/m_m_r.cpp
    org/minima/objects/mmr/m_m_r_data.cpp
    org/minima/objects/mmr/m_m_r_entry.cpp
    org/minima/objects/mmr/m_m_r_entry_number.cpp
    org/minima/objects/base/mini_data.cpp
    org/minima/objects/base/mini_number.cpp
    org/minima/objects/base/mini_byte.cpp
    org/minima/objects/base/mini_string.cpp
    org/minima/utils/crypto.cpp
    org/minima/utils/streamable.cpp
    org/minima/utils/fast_byte_array_stream.cpp
    org/minima/utils/base_converter.cpp
    org/minima/system/params/general_params.cpp
    org/minima/system/params/global_params.cpp
    org/minima/utils/json/j_s_o_n_aware.cpp
    org/minima/utils/json/j_s_o_n_array.cpp
    org/minima/utils/json/j_s_o_n_object.cpp
    org/minima/utils/json/j_s_o_n_stream_aware.cpp
    org/minima/utils/json/j_s_o_n_value.cpp
    org/minima/utils/json/j_s_o_n_writer.cpp
    org/minima/utils/json/item_list.cpp
    org/minima/utils/json/parser/container_factory.cpp
    org/minima/utils/json/parser/content_handler.cpp
    org/minima/utils/json/parser/j_s_o_n_parser.cpp
    org/minima/utils/json/parser/parse_exception.cpp
    org/minima/utils/json/parser/yylex.cpp
    org/minima/utils/json/parser/yytoken.cpp
    tests/stubs.cpp
)
target_compile_features(test_sql_batch PRIVATE cxx_std_17)
target_include_directories(test_sql_batch PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(APPLE)
    target_include_directories(test_sql_batch PRIVATE /opt/homebrew/include)
endif()
target_link_libraries(test_sql_batch PRIVATE SQLite::SQLite3 OpenSSL::Crypto Threads::Threads)

# TxPoWSqlDB write-throughput bench: batched vs one-transaction-per-row.
add_executable(bench_sql_batch
    EXCLUDE_FROM_ALL
    reference-bench/cpp/bench_sqlbatch.cpp
    org/minima/database/txpowdb/sql/tx_po_w_sql_d_b.cpp
    org/minima/utils/sql_d_b.cpp
    org/minima/database/txpowdb/ram/ram_d_b.cpp
    org/minima/database/txpowdb/ram/ram_data.cpp
    org/minima/objects/tx_po_w.cpp
    org/minima/objects/tx_body.cpp
    org/minima/objects/tx_header.cpp
    org/minima/objects/transaction.cpp
    org/minima/objects/witness.cpp
    org/minima/objects/coin.cpp
    org/minima/objects/coin_proof.cpp
    org/minima/objects/token.cpp
    org/minima/objects/script_proof.cpp
    org/minima/objects/magic.cpp
    org/minima/objects/state_variable.cpp
    org/minima/objects/address.cpp
    org/minima/objects/keys/signature.cpp
    org/minima/objects/keys/signature_proof.cpp
    org/minima/objects/keys/winternitz.cpp
    org/minima/objects/keys/tree_key.cpp
    org/minima/objects/keys/tree_key_node.cpp
    org/minima/objects/mmr/m_m_r_proof.cpp
    org/minima/objects/mmr/m_m_r.cpp
    org/minima/objects/mmr/m_m_r_data.cpp
    org/minima/objects/mmr/m_m_r_entry.cpp
    org/minima/objects/mmr/m_m_r_entry_number.cpp
    org/minima/objects/base/mini_data.cpp
    org/minima/objects/base/mini_number.cpp
    org/minima/objects/base/mini_byte.cpp
    org/minima/objects/base/mini_string.cpp
    org/minima/utils/crypto.cpp
    org/minima/utils/streamable.cpp
    org/minima/utils/fast_byte_array_stream.cpp
    org/minima/utils/base_converter.cpp
    org/minima/system/params/general_params.cpp
    org/minima/system/params/global_params.cpp
    org/minima/utils/json/j_s_o_n_aware.cpp
    org/minima/utils/json/j_s_o_n_array.cpp
    org/minima/utils/json/j_s_o_n_object.cpp
    org/minima/utils/json/j_s_o_n_stream_aware.cpp
    org/minima/utils/json/j_s_o_n_value.cpp
    org/minima/utils/json/j_s_o_n_writer.cpp
    org/minima/utils/json/item_list.cpp
    org/minima/utils/json/parser/container_factory.cpp
    org/minima/utils/json/parser/content_handler.cpp
    org/minima/utils/json/parser/j_s_o_n_parser.cpp
    org/minima/utils/json/parser/parse_exception.cpp
    org/minima/utils/json/parser/yylex.cpp
    org/minima/utils/json/parser/yytoken.cpp
    tests/stubs.cpp
)
target_compile_features(bench_sql_batch PRIVATE cxx_std_17)
target_include_directories(bench_sql_batch PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(APPLE)
    target_include_directories(bench_sql_batch PRIVATE /opt/homebrew/include)
endif()
target_link_libraries(bench_sql_batch PRIVATE SQLite::SQLite3 OpenSSL::Crypto Threads::Threads)

# ============================================================================
# CLEANUP TARGETS - For managing build artifacts
# ============================================================================

# Standard enhanced clean (removes object files but keeps CMake cache)
add_custom_target(clean-build
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target clean
    COMMAND find ${CMAKE_BINARY_DIR} -name "*.o" -type f -delete 2>/dev/null || true
    COMMAND find ${CMAKE_BINARY_DIR} -name "*.o.d" -type f -delete 2>/dev/null || true
    COMMENT "Cleaning build artifacts including .o and .o.d files"
)

# Deep clean (removes everything including CMake cache - forces reconfigure)
add_custom_target(clean-all
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/CMakeFiles
    COMMAND ${CMAKE_COMMAND} -E remove -f ${CMAKE_BINARY_DIR}/cmake_install.cmake
    COMMAND ${CMAKE_COMMAND} -E remove -f ${CMAKE_BINARY_DIR}/CMakeCache.txt
    COMMAND ${CMAKE_COMMAND} -E remove -f ${CMAKE_BINARY_DIR}/Makefile
    COMMAND ${CMAKE_COMMAND} -E remove -f ${CMAKE_BINARY_DIR}/minima
    COMMAND find ${CMAKE_BINARY_DIR} -name "*.o" -type f -delete 2>/dev/null || true
    COMMAND find ${CMAKE_BINARY_DIR} -name "*.o.d" -type f -delete 2>/dev/null || true
    COMMAND find ${CMAKE_BINARY_DIR} -name "*.a" -type f -delete 2>/dev/null || true
    COMMENT "Deep clean - removing all build artifacts (requires cmake .. again)"
)

# Help target to show available clean commands
add_custom_target(help-clean
    COMMAND ${CMAKE_COMMAND} -E echo ""
    COMMAND ${CMAKE_COMMAND} -E echo "Available clean targets:"
    COMMAND ${CMAKE_COMMAND} -E echo "  make clean       - Standard clean (keeps .o/.o.d for fast rebuild)"
    COMMAND ${CMAKE_COMMAND} -E echo "  make clean-build - Enhanced clean (removes .o/.o.d files)"
    COMMAND ${CMAKE_COMMAND} -E echo "  make clean-all   - Deep clean (removes everything, need to re-run cmake)"
    COMMAND ${CMAKE_COMMAND} -E echo ""
)

# ============================================================================
# END CLEANUP TARGETS
# ============================================================================
# ---------------------------------------------------------------------------
# RFC-0011: capability manifest + in-node MCP tests (share the node objects).
# ---------------------------------------------------------------------------
set(MINIMA_TEST_LIBS Threads::Threads)
if(TARGET SQLite::SQLite3)
    list(APPEND MINIMA_TEST_LIBS SQLite::SQLite3)
endif()
if(TARGET OpenSSL::SSL)
    list(APPEND MINIMA_TEST_LIBS OpenSSL::SSL OpenSSL::Crypto)
endif()
if(TARGET ZLIB::ZLIB)
    list(APPEND MINIMA_TEST_LIBS ZLIB::ZLIB)
endif()
if(TARGET CURL::libcurl)
    list(APPEND MINIMA_TEST_LIBS CURL::libcurl)
endif()
if(UNIX AND NOT APPLE)
    list(APPEND MINIMA_TEST_LIBS ${CMAKE_DL_LIBS})
endif()
if(LMDB_LIBRARY)
    list(APPEND MINIMA_TEST_LIBS ${LMDB_LIBRARY})
endif()

foreach(tgt test_capabilities test_mcp_server test_docs_manifest test_megammr_import test_h2_import test_archive_records test_archive_tail test_script_conformance gen_command_reference)
    add_executable(${tgt} EXCLUDE_FROM_ALL
        tests/${tgt}.cpp
        $<TARGET_OBJECTS:minima_core>)
    target_compile_features(${tgt} PRIVATE cxx_std_17)
    target_include_directories(${tgt} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    if(APPLE)
        target_include_directories(${tgt} PRIVATE /opt/homebrew/include)
    endif()
    target_link_libraries(${tgt} PRIVATE ${MINIMA_TEST_LIBS})
endforeach()

add_test(NAME capabilities COMMAND test_capabilities)
add_test(NAME mcp_server COMMAND test_mcp_server)
add_test(NAME docs_manifest COMMAND test_docs_manifest ${CMAKE_CURRENT_SOURCE_DIR}/COMMAND_REFERENCE.md)
add_test(NAME megammr_import COMMAND test_megammr_import)
add_test(NAME h2_import COMMAND test_h2_import)

if(MINIMA_BUILD_ANALYTICS AND UNIX)
    add_subdirectory(analytics)
endif()
