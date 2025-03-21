cmake_minimum_required(VERSION 3.20)

project(CoreLog)

add_library(CoreLog INTERFACE)
target_include_directories(CoreLog 
INTERFACE ./Public/)

message(NOTICE "CoreLog Include ${INCLUDE_DIRECTORIES}")
message(NOTICE "CoreLog Interface ${INTERFACE_INCLUDE_DIRECTORIES}")