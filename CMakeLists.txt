cmake_minimum_required(VERSION 3.5)
project(wiapa_modbus_server)

set(CMAKE_C_STANDARD 99)

set(SOURCE_FILES main.c socket.c socket.h modbus_data.h get_config.c get_config.h)
add_executable(wiapa_modbus_server ${SOURCE_FILES})

target_link_libraries(wiapa_modbus_server modbus pthread)