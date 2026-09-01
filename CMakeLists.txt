cmake_minimum_required(VERSION 3.16)

project(Aegis LANGUAGES CXX)

add_library(Aegis
    src/arena.cpp
)

target_include_directories(Aegis
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(Aegis PUBLIC cxx_std_17)

add_library(Aegis::Aegis ALIAS Aegis)