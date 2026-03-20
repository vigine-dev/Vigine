cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

set(PROJECT_POSTGRESQL_NAME example-postgresql)
project(${PROJECT_POSTGRESQL_NAME})

set(EXAMPLE_POSTGRESQL_DATA_TASK_SOURCES
    task/data/addsomedatatask.h task/data/addsomedatatask.cpp
    task/data/readsomedatatask.h task/data/readsomedatatask.cpp
    task/data/removesomedatatask.h task/data/removesomedatatask.cpp)

set(EXAMPLE_POSTGRESQL_DB_BOOTSTRAP_SOURCES
    task/db/initbdtask.h task/db/initbdtask.cpp
    task/db/checkbdshecmetask.h task/db/checkbdshecmetask.cpp)

set(EXAMPLE_POSTGRESQL_STATE_SOURCES
    state/initstate.h state/initstate.cpp
    state/workstate.h state/workstate.cpp
    state/errorstate.h state/errorstate.cpp
    state/closestate.h state/closestate.cpp)

add_executable(${PROJECT_POSTGRESQL_NAME}
    main.cpp
    ${EXAMPLE_POSTGRESQL_DATA_TASK_SOURCES}
    ${EXAMPLE_POSTGRESQL_DB_BOOTSTRAP_SOURCES}
    ${EXAMPLE_POSTGRESQL_STATE_SOURCES})

target_link_libraries(${PROJECT_POSTGRESQL_NAME}
    PRIVATE
    vigine
)

target_include_directories(${PROJECT_POSTGRESQL_NAME}
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_SOURCE_DIR}/include
)
