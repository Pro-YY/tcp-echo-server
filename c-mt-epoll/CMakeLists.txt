cmake_minimum_required(VERSION 3.5)

set (PROJECT server)

SET (CMAKE_C_COMPILER                  "/usr/bin/clang")
SET (CMAKE_C_FLAGS_INIT                "-Wall -std=c99")
SET (CMAKE_C_FLAGS_DEBUG_INIT          "-g")
SET (CMAKE_C_FLAGS_MINSIZEREL_INIT     "-Os -DNDEBUG")
SET (CMAKE_C_FLAGS_RELEASE_INIT        "-O4 -DNDEBUG")
SET (CMAKE_C_FLAGS_RELWITHDEBINFO_INIT "-O2 -g")

file (GLOB PROJECT_SRCS *.c)
project (${PROJECT} C)
add_executable (${PROJECT} ${PROJECT_SRCS})

set (THREADS_PREFER_PTHREAD_FLAG ON)
find_package (Threads REQUIRED)
target_link_libraries (${PROJECT} ${CMAKE_THREAD_LIBS_INIT})
