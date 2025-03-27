cmake_minimum_required(VERSION 3.10)

set(PROJECT_TEST_NAME unittest)

project(${PROJECT_TEST_NAME})

# Set C++23 standard for tests
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++23")

# ====================== Add Google Test =======================
add_subdirectory(${CMAKE_SOURCE_DIR}/external/googletest ${CMAKE_BINARY_DIR}/googletest)

# ====================== Create Test Executable =======================
add_executable(${PROJECT_TEST_NAME}
    test_vigine.cpp
    component/ComponentManagerTest.cpp
    EntityTest.cpp
    architecture/ArchitectureTest.cpp
    architecture/StateMachineTest.cpp
    architecture/AbstractTaskFlowTest.cpp
    architecture/AbstractTaskTest.cpp
    architecture/AbstractServiceTest.cpp
    architecture/AbstractStateTest.cpp
    architecture/ecs/AbstractSystemTest.cpp
    architecture/ecs/AbstractComponentTest.cpp
    architecture/ecs/AbstractEntityTest.cpp
)

# ====================== Add Include Directories =======================
target_include_directories(${PROJECT_TEST_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# ====================== Link Google Test Libraries =======================
target_link_libraries(${PROJECT_TEST_NAME}
    gtest
    gtest_main
    vigine
)

