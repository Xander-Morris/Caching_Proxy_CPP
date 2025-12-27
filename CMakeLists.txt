cmake_minimum_required(VERSION 3.28)

project(Caching_Proxy_CPP LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)

add_executable(Caching_Proxy_CPP
    src/main.cpp
    src/Cache.cpp
)

target_include_directories(Caching_Proxy_CPP PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)