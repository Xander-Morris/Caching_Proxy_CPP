cmake_minimum_required(VERSION 3.28)

project(Caching_Proxy_CPP LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)

add_executable(Caching_Proxy_CPP
    src/main.cpp
    src/Cache.cpp
    src/Proxy.cpp
)

target_include_directories(Caching_Proxy_CPP PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

find_package(OpenSSL REQUIRED)

target_link_libraries(Caching_Proxy_CPP
    OpenSSL::SSL
    OpenSSL::Crypto
)

target_compile_definitions(Caching_Proxy_CPP
    PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT
)