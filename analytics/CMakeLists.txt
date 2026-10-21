add_executable(minima-analytics
    main.cpp
    api_server.cpp
    archive_reader.cpp
)

target_compile_features(minima-analytics PRIVATE cxx_std_17)
target_include_directories(minima-analytics PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/..)
if(APPLE)
    target_include_directories(minima-analytics PRIVATE /opt/homebrew/include)
endif()

if(TARGET SQLite::SQLite3)
    target_link_libraries(minima-analytics PRIVATE SQLite::SQLite3)
else()
    target_link_libraries(minima-analytics PRIVATE sqlite3)
endif()
target_link_libraries(minima-analytics PRIVATE Threads::Threads)
