cmake_minimum_required(VERSION 3.16)
project(pureminima_benchmarks LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(APPLE)
    # Homebrew OpenSSL / SQLite on Apple Silicon
    include_directories(/opt/homebrew/include)
    link_directories(/opt/homebrew/lib)
endif()

find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(SQLite3 REQUIRED)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)

# Common base objects needed by both MMR versions
set(MMR_BASE_SRC
    ../org/minima/objects/base/mini_data.cpp
    ../org/minima/objects/base/mini_number.cpp
    ../org/minima/objects/base/mini_byte.cpp
    ../org/minima/objects/base/mini_string.cpp
    ../org/minima/utils/crypto.cpp
    ../org/minima/utils/streamable.cpp
    ../org/minima/utils/fast_byte_array_stream.cpp
    ../org/minima/utils/base_converter.cpp
    ../org/minima/system/params/general_params.cpp
    ../org/minima/utils/json/j_s_o_n_aware.cpp
    ../org/minima/utils/json/j_s_o_n_array.cpp
    ../org/minima/utils/json/j_s_o_n_object.cpp
    ../org/minima/utils/json/j_s_o_n_stream_aware.cpp
    ../org/minima/utils/json/j_s_o_n_value.cpp
    ../org/minima/utils/json/j_s_o_n_writer.cpp
    ../org/minima/utils/json/item_list.cpp
    ../org/minima/utils/json/parser/container_factory.cpp
    ../org/minima/utils/json/parser/content_handler.cpp
    ../org/minima/utils/json/parser/j_s_o_n_parser.cpp
    ../org/minima/utils/json/parser/parse_exception.cpp
    ../org/minima/utils/json/parser/yylex.cpp
    ../org/minima/utils/json/parser/yytoken.cpp
    ../tests/stubs.cpp
)

# New (cached) MMR benchmark
add_executable(bench_mmr_new
    bench_mmr.cpp
    ../org/minima/objects/mmr/m_m_r.cpp
    ../org/minima/objects/mmr/m_m_r_data.cpp
    ../org/minima/objects/mmr/m_m_r_entry.cpp
    ../org/minima/objects/mmr/m_m_r_entry_number.cpp
    ../org/minima/objects/mmr/m_m_r_proof.cpp
    ${MMR_BASE_SRC}
)
target_compile_definitions(bench_mmr_new PRIVATE HAS_PEAKS_REF)
target_link_libraries(bench_mmr_new PRIVATE OpenSSL::Crypto Threads::Threads)

# Old (pre-optimisation) MMR benchmark
# old_include shadows org/minima/objects/mmr/*.hpp so bench_mmr.cpp uses the old headers.
add_executable(bench_mmr_old
    bench_mmr.cpp
    old_mmr/m_m_r.cpp
    old_mmr/m_m_r_data.cpp
    old_mmr/m_m_r_entry.cpp
    old_mmr/m_m_r_entry_number.cpp
    old_mmr/m_m_r_proof.cpp
    ${MMR_BASE_SRC}
)
target_include_directories(bench_mmr_old BEFORE PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/old_include)
target_include_directories(bench_mmr_old PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/old_mmr)
target_link_libraries(bench_mmr_old PRIVATE OpenSSL::Crypto Threads::Threads)

# SQLite tuning benchmark
add_executable(bench_sqlite
    bench_sqlite.cpp
)
target_link_libraries(bench_sqlite PRIVATE SQLite::SQLite3 Threads::Threads)
