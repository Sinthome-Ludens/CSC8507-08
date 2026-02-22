cmake_minimum_required(VERSION 3.16)

file(GLOB ENET_SOURCES "*.c" "*.h")
add_library(ENet STATIC ${ENET_SOURCES})

if(WIN32)
    target_link_libraries(ENet PUBLIC ws2_32 winmm)
endif()

target_include_directories(ENet PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
