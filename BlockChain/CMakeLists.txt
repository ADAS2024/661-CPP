cmake_minimum_required(VERSION 3.14)
project(Blockchain CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(blockchain_test main.cpp)
target_include_directories(blockchain_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(blockchain_test PRIVATE -Wall -O2)
