cmake_minimum_required(VERSION 3.17)
project(queue_reader_writer)

set(CMAKE_CXX_STANDARD 17)

add_executable(queue_reader_writer main.cpp queue.h)

add_subdirectory(Google_tests)