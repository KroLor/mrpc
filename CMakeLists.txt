cmake_minimum_required(VERSION 4.1.0)
project(MRPC LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE SOURCES "src/*.cpp")

set(FREERTOS_KERNEL_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/FreeRTOS-Kernel")

if(NOT ESP_PLATFORM)
    list(APPEND SOURCES
        "${FREERTOS_KERNEL_DIR}/tasks.c"
        "${FREERTOS_KERNEL_DIR}/list.c"
        "${FREERTOS_KERNEL_DIR}/queue.c"
        "${FREERTOS_KERNEL_DIR}/timers.c"
        "${FREERTOS_KERNEL_DIR}/event_groups.c"
        "${FREERTOS_KERNEL_DIR}/portable/MemMang/heap_4.c"
        "${FREERTOS_KERNEL_DIR}/portable/MSVC-MingW/port.c"
    )
endif()

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE src)
target_include_directories(${PROJECT_NAME} PRIVATE src/application)
target_include_directories(${PROJECT_NAME} PRIVATE src/channels)
target_include_directories(${PROJECT_NAME} PRIVATE src/freeRTOS_w)
target_include_directories(${PROJECT_NAME} PRIVATE src/physics)
target_include_directories(${PROJECT_NAME} PRIVATE src/transport)

if(NOT ESP_PLATFORM)
    target_include_directories(${PROJECT_NAME} PRIVATE "${FREERTOS_KERNEL_DIR}/include")
    target_include_directories(${PROJECT_NAME} PRIVATE "${FREERTOS_KERNEL_DIR}/portable/MSVC-MingW")
    
    # if(WIN32)
    #     target_link_libraries(${PROJECT_NAME} PRIVATE winmm)
    # endif()
endif()