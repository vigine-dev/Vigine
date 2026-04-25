cmake_minimum_required(VERSION 3.10)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

set(PROJECT_FANOUT_FSM_NAME example-fanout-fsm)
project(${PROJECT_FANOUT_FSM_NAME})

add_executable(${PROJECT_FANOUT_FSM_NAME}
    main.cpp
)

target_link_libraries(${PROJECT_FANOUT_FSM_NAME}
    PRIVATE
    vigine
)

# The vigine target PUBLIC-exports its include dirs, so consumers
# linking against it (PRIVATE here) inherit them automatically. No
# need to add ${CMAKE_SOURCE_DIR}/include manually -- that variable
# would point at a parent project when Vigine is added via
# add_subdirectory and could leak the wrong include root.
