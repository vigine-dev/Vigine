cmake_minimum_required(VERSION 3.10)

set(PROJECT_NAME vigine)

project(${PROJECT_NAME})

# Set C++23 standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++23")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

set(INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)
set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)

# Set the CMake module path to include the cmake directory
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

# Find and include glm
find_package(GLM REQUIRED)

# Option to build examples
option(ENABLE_EXAMPLE "Build example projects" OFF)
option(ENABLE_UNITTEST "Enable tests" OFF)

if(ENABLE_UNITTEST AND EXISTS "${CMAKE_SOURCE_DIR}/test/CMakeLists.txt")
    add_subdirectory(test)
endif()

# Add the example project if the option is enabled
if(ENABLE_EXAMPLE AND EXISTS "${CMAKE_SOURCE_DIR}/example/CMakeLists.txt")
    add_subdirectory(example)
endif()

# Add source files
set(HEADER
    ${INCLUDE_DIR}/vigine/vigine.h
    ${INCLUDE_DIR}/vigine/result.h
    ${INCLUDE_DIR}/vigine/statemachine.h
    ${INCLUDE_DIR}/vigine/abstractstate.h
    ${INCLUDE_DIR}/vigine/taskflow.h
    ${INCLUDE_DIR}/vigine/abstracttask.h
    ${INCLUDE_DIR}/vigine/abstractservice.h
    ${INCLUDE_DIR}/vigine/context.h
    ${INCLUDE_DIR}/vigine/property.h
    ${INCLUDE_DIR}/vigine/ecs/abstractsystem.h
    ${INCLUDE_DIR}/vigine/ecs/abstractcomponent.h
    ${INCLUDE_DIR}/vigine/ecs/entity.h
    ${INCLUDE_DIR}/vigine/system/rendersystem.h
    ${INCLUDE_DIR}/vigine/component/componentmanager.h
    ${INCLUDE_DIR}/vigine/component/rendercomponent.h
    ${INCLUDE_DIR}/vigine/contextholder.h
    ${INCLUDE_DIR}/vigine/contextholder.cpp
    ${INCLUDE_DIR}/vigine/ecs/entitymanager.h
    ${INCLUDE_DIR}/vigine/ecs/postgresqlsystem.h
    ${INCLUDE_DIR}/vigine/entitybindinghost.h
)

set(SOURCES
    ${SRC_DIR}/vigine.cpp
    ${SRC_DIR}/result.cpp
    ${SRC_DIR}/statemachine.cpp
    ${SRC_DIR}/taskflow.cpp
    ${SRC_DIR}/context.cpp
    ${SRC_DIR}/system/rendersystem.cpp
    ${SRC_DIR}/component/componentmanager.cpp
    ${SRC_DIR}/component/rendercomponent.cpp
    ${SRC_DIR}/ecs/entity.cpp
    ${SRC_DIR}/abstracttask.cpp
    ${SRC_DIR}/abstractservice.cpp
    ${SRC_DIR}/ecs/entitymanager.cpp
    ${SRC_DIR}/ecs/postgresqlsystem.cpp
    ${SRC_DIR}/ecs/abstractsystem.cpp
    ${SRC_DIR}/entitybindinghost.cpp
)

# Add source files
set(HEADER_SERVICE
    ${INCLUDE_DIR}/vigine/service/databaseservice.h
)

set(SOURCES_SERVICE
    ${SRC_DIR}/service/databaseservice.cpp
)

# Add a library
add_library(${PROJECT_NAME}
    STATIC
    ${HEADER}
    ${SOURCES}
    ${HEADER_SERVICE}
    ${SOURCES_SERVICE}
)

# Include directories
target_include_directories(${PROJECT_NAME}
    PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${GLM_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(${PROJECT_NAME}
    PUBLIC
    ${GLM_LIBRARIES}
    pqxx
    pq
)

# Install the library and headers
install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_SOURCE_DIR}/build/lib)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/ DESTINATION ${CMAKE_SOURCE_DIR}/build/include)
