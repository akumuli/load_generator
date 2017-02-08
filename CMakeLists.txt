project(load_generator)
cmake_minimum_required(VERSION 2.8)

add_definitions("--std=c++11")

set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)

find_package(Boost 1.54.0
    COMPONENTS
    program_options
    system
    thread
    date_time
    REQUIRED
)

aux_source_directory(. SRC_LIST)
add_executable(${PROJECT_NAME} ${SRC_LIST})

target_link_libraries(${PROJECT_NAME} "${Boost_LIBRARIES}")
