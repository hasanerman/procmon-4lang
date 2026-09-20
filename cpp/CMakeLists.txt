cmake_minimum_required(VERSION 3.24)
project(procmon_cpp CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(WIN32)
    set(PLATFORM_SOURCE src/WinProcessSource.cpp)
    set(PLATFORM_LIBS psapi)
else()
    set(PLATFORM_SOURCE src/LinuxProcessSource.cpp)
    set(PLATFORM_LIBS "")
endif()

add_library(procmon_core STATIC
    src/Args.cpp
    src/ProcessModel.cpp
    src/Renderer.cpp
    ${PLATFORM_SOURCE}
)
target_include_directories(procmon_core PUBLIC src)
target_link_libraries(procmon_core PUBLIC ${PLATFORM_LIBS})

if(MSVC)
    target_compile_options(procmon_core PUBLIC /W4 /WX /utf-8)
else()
    target_compile_options(procmon_core PUBLIC -Wall -Wextra -Wpedantic -Werror)
endif()

add_executable(procmon src/main.cpp)
target_link_libraries(procmon PRIVATE procmon_core)

enable_testing()

add_executable(test_args tests/test_args.cpp)
target_link_libraries(test_args PRIVATE procmon_core)
add_test(NAME args COMMAND test_args)

add_executable(test_model tests/test_model.cpp)
target_link_libraries(test_model PRIVATE procmon_core)
add_test(NAME model COMMAND test_model)
