cmake_minimum_required(VERSION 3.16)
project(linux_ipc_chat LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(chat_app src/host.cpp)

target_compile_options(chat_app PRIVATE -Wall -Wextra -Werror)
target_link_libraries(chat_app PRIVATE Qt6::Widgets)