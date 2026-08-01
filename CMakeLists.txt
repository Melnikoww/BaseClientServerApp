cmake_minimum_required(VERSION 3.15)
project(Client-ServerApp)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(Client
    src/client.c
)

add_executable(Server
    src/server.c
)
