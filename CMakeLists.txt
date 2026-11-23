cmake_minimum_required(VERSION 3.16)
project(linux_ipc_chat LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

# Библиотека для упаковки и распаковки сообщений
add_library(chat_frame STATIC
    src/chat/frame.cpp
)

target_include_directories(chat_frame PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_compile_options(chat_frame PRIVATE
    -Wall -Wextra -Werror
)

# Приложение чата
add_executable(chat_app
    src/host.cpp
    src/ui/chat_window.hpp
    src/ui/chat_window.cpp
    src/ipc/conn_pipe.cpp
)

set_target_properties(chat_app PROPERTIES
    AUTOMOC ON
)

target_compile_options(chat_app PRIVATE
    -Wall -Wextra -Werror
)

target_link_libraries(chat_app PRIVATE
    Qt6::Widgets
    chat_frame
)

# Проверка формата сообщений
enable_testing()

add_executable(frame_check
    tests/frame_check.cpp
)

target_link_libraries(frame_check PRIVATE
    chat_frame
)

target_compile_options(frame_check PRIVATE
    -Wall -Wextra -Werror
)

add_test(NAME frame_check COMMAND frame_check)