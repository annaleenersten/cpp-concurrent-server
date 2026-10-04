cmake_minimum_required(VERSION 3.16)

project(CppConcurrentServer
    VERSION 1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(server
    src/main.cpp
    src/Server.cpp
    src/HttpRequest.cpp
    src/HttpResponse.cpp
    src/Router.cpp
    src/ThreadPool.cpp
    src/Cache.cpp
)

target_include_directories(server PRIVATE
    include
)

include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
)

FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(server_tests
    tests/CacheTest.cpp
    tests/HttpRequestTest.cpp
    tests/HttpResponseTest.cpp
    tests/RouterTest.cpp
    tests/ThreadPoolTest.cpp
    tests/ServerIntegrationTest.cpp

    src/Cache.cpp
    src/HttpRequest.cpp
    src/HttpResponse.cpp
    src/Router.cpp
    src/ThreadPool.cpp
)

target_include_directories(server_tests PRIVATE
    include
)

target_link_libraries(server_tests
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(server_tests)