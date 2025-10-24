cmake_minimum_required(VERSION 3.20)

#set(CMAKE_C_COMPILER gcc-14)
#set(CMAKE_CXX_COMPILER g++-14)

project(BountyBoard_System
        VERSION 0.1.0
        LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -g")

include_directories(${PROJECT_SOURCE_DIR})

add_executable(server
        src/server.cpp
)

add_executable(client
        src/client.cpp
)

set_target_properties(server client PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}
)