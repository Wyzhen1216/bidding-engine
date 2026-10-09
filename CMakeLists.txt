cmake_minimum_required(VERSION 3.16)
project(BiddingEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(bidding_engine
    src/main.cpp
)

target_include_directories(bidding_engine PRIVATE src)

# 编译选项: 统一UTF-8; GCC/Clang下静态链接, 生成可独立运行的exe
if(MSVC)
    target_compile_options(bidding_engine PRIVATE /utf-8)
elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(bidding_engine PRIVATE -finput-charset=utf-8 -fexec-charset=utf-8)
    target_link_options(bidding_engine PRIVATE -static -static-libgcc -static-libstdc++)
endif()
