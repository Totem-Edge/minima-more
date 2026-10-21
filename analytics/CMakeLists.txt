find_path(DUCKDB_INCLUDE_DIR duckdb.hpp
    HINTS /opt/homebrew/include /usr/local/include /usr/include)
find_library(DUCKDB_LIBRARY duckdb
    HINTS /opt/homebrew/lib /usr/local/lib /usr/lib)
if(NOT DUCKDB_INCLUDE_DIR OR NOT DUCKDB_LIBRARY)
    message(FATAL_ERROR "minima-analytics requires DuckDB headers + libduckdb (set DUCKDB_INCLUDE_DIR/DUCKDB_LIBRARY)")
endif()

add_executable(minima-analytics
    main.cpp
    api_server.cpp
    archive_reader.cpp
    indexer.cpp
    backup.cpp
    $<TARGET_OBJECTS:minima_core>
)

target_compile_features(minima-analytics PRIVATE cxx_std_17)
target_include_directories(minima-analytics PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${DUCKDB_INCLUDE_DIR})
if(APPLE)
    target_include_directories(minima-analytics PRIVATE /opt/homebrew/include)
endif()
target_compile_definitions(minima-analytics PRIVATE
    MINIMA_ANALYTICS_SCHEMA="${CMAKE_CURRENT_SOURCE_DIR}/schema.duckdb.sql"
    MINIMA_ANALYTICS_KNOWN_SCRIPTS="${CMAKE_CURRENT_SOURCE_DIR}/known_scripts.csv")

target_link_libraries(minima-analytics PRIVATE
    ${DUCKDB_LIBRARY}
    ${MINIMA_TEST_LIBS})
