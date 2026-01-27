cmake_minimum_required(VERSION 3.28)
project(Pawn)


set(CMAKE_CXX_STANDARD 20)

option(PAWN_LINUX "Use the Linux build configuration" OFF)

if(PAWN_LINUX)
        include("${CMAKE_CURRENT_LIST_DIR}/cmake/linux.cmake")
else()
        include("${CMAKE_CURRENT_LIST_DIR}/cmake/osx.cmake")
endif()

