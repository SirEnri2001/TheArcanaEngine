cmake_minimum_required(VERSION 3.20)

project(stb_image)

add_library(stb_image INTERFACE)

target_include_directories(stb_image INTERFACE ./)