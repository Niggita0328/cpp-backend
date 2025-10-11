cmake_minimum_required(VERSION 3.11)

project(game_server CXX)

set(CMAKE_CXX_STANDARD 20)

find_package(Boost 1.71.0 REQUIRED CONFIG COMPONENTS system program_options json log thread filesystem date_time serialization)
find_package(Catch2 3 REQUIRED)
find_package(libpqxx REQUIRED)

add_library(game_server_lib
    src/model.h
    src/model.cpp
    src/json_loader.h
    src/json_loader.cpp
    src/extra_data.h
    src/extra_data.cpp
    src/loot_generator.h
    src/loot_generator.cpp
    src/application.h
    src/application.cpp
    src/json_serializer.h
    src/json_serializer.cpp
    src/tagged.h
    src/model_serialization.cpp
    src/serializing_listener.cpp
    src/postgres.h
    src/postgres.cpp
)

target_include_directories(game_server_lib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)


target_link_libraries(game_server_lib
    PUBLIC
        Boost::program_options
        Boost::system
        Boost::json
        Boost::log
        Boost::thread
        Boost::filesystem
        Boost::date_time
        Boost::serialization
        Boost::log_setup
        libpqxx::pqxx
)

add_executable(game_server 
    src/main.cpp
    src/http_server.h
    src/http_server.cpp
    src/api_handler.h
    src/api_handler.cpp
    src/request_handler.h
    src/request_handler.cpp
    src/boost_json.cpp
)

target_link_libraries(game_server PRIVATE game_server_lib)

enable_testing()

add_executable(game_server_tests tests/loot_generator_tests.cpp tests/model_tests.cpp tests/gathering_tests.cpp)

target_link_libraries(game_server_tests
    PRIVATE
        game_server_lib
        Catch2::Catch2
        Catch2::Catch2WithMain
)

add_test(NAME unit_tests COMMAND game_server_tests)
