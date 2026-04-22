cmake_minimum_required(VERSION 3.10)

set(PROJECT_TEST_NAME unittest)

project(${PROJECT_TEST_NAME})

# Set C++23 standard for tests
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED YES)

# ====================== Add Google Test =======================
# Align GoogleTest's CRT with the rest of the tree so the test
# executables link against the same runtime as the vigine library
# (prevents LNK2038 MTd_StaticDebug vs MDd_DynamicDebug mismatches on
# MSVC).
set(gtest_force_shared_crt ON CACHE BOOL "Use shared (DLL) CRT for GoogleTest" FORCE)
add_subdirectory(${CMAKE_SOURCE_DIR}/external/googletest ${CMAKE_BINARY_DIR}/googletest)

# ====================== Create Test Executable =======================
add_executable(${PROJECT_TEST_NAME}
    test_vigine.cpp
    component/ComponentManagerTest.cpp
    EntityTest.cpp
    architecture/ArchitectureTest.cpp
    architecture/StateMachineTest.cpp
    architecture/TaskFlowTest.cpp
    architecture/AbstractTaskTest.cpp
    architecture/AbstractServiceTest.cpp
    architecture/AbstractStateTest.cpp
    architecture/ecs/AbstractSystemTest.cpp
    architecture/ecs/AbstractComponentTest.cpp
    architecture/ecs/AbstractEntityTest.cpp
    graph/smoke_test.cpp
)

# ====================== Add Include Directories =======================
target_include_directories(${PROJECT_TEST_NAME}
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# ====================== Link Libraries =======================
target_link_libraries(${PROJECT_TEST_NAME}
    PRIVATE
    gtest
    gtest_main
    vigine
    ${GLM_LIBRARIES}
)

# ====================== Set Output Directory =======================
set_target_properties(${PROJECT_TEST_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# ====================== Graph Contract Test Target =======================
# The graph contract suite exercises IGraph against the pure-virtual
# public surface. Its cases are parametrised on a factory callable so
# that additional concrete IGraph implementations inherit the same
# coverage by instantiating with another factory.
set(GRAPH_CONTRACT_TARGET graph-contract)

add_executable(${GRAPH_CONTRACT_TARGET}
    graph/contract_igraph.cpp
    graph/contract_inode.cpp
    graph/contract_iedge.cpp
    graph/contract_traversal.cpp
    graph/contract_query.cpp
    graph/contract_generational_id.cpp
    graph/contract_hsm_bubble.cpp
    graph/contract_fanout.cpp
    graph/contract_async_timeout.cpp
    graph/contract_result_deferred.cpp
)

target_include_directories(${GRAPH_CONTRACT_TARGET}
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${GRAPH_CONTRACT_TARGET}
    PRIVATE
    gtest
    gtest_main
    vigine
    ${GLM_LIBRARIES}
)

set_target_properties(${GRAPH_CONTRACT_TARGET} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Register the contract suite with CTest, tagged so it can be selected
# with `ctest -L graph-contract`.
include(GoogleTest)
gtest_discover_tests(${GRAPH_CONTRACT_TARGET}
    PROPERTIES LABELS "graph-contract"
    DISCOVERY_TIMEOUT 60
    DISCOVERY_MODE PRE_TEST
)

# ====================== Payload Registry Smoke Target =======================
# Smoke coverage for IPayloadRegistry: factory ownership, engine
# bootstrap, happy-path user registration, overlap and out-of-range
# rejection, unregister, and a concurrent register/resolve case that
# the thread sanitiser verifies under -fsanitize=thread.
set(PAYLOAD_SMOKE_TARGET payload-smoke)

add_executable(${PAYLOAD_SMOKE_TARGET}
    payload/smoke_test.cpp
)

target_include_directories(${PAYLOAD_SMOKE_TARGET}
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${PAYLOAD_SMOKE_TARGET}
    PRIVATE
    gtest
    gtest_main
    vigine
    ${GLM_LIBRARIES}
)

set_target_properties(${PAYLOAD_SMOKE_TARGET} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

gtest_discover_tests(${PAYLOAD_SMOKE_TARGET}
    PROPERTIES LABELS "payload-registry-smoke"
    DISCOVERY_TIMEOUT 60
    DISCOVERY_MODE PRE_TEST
)

# ====================== Messaging Core Smoke Target =======================
# Smoke coverage for the unified message bus: factory ownership, the five
# routing modes (FirstMatch / FanOut / Chain / Bubble / Broadcast),
# subscription cancel, shutdown rejection, and exception isolation.
set(MESSAGING_SMOKE_TARGET messaging-smoke)

add_executable(${MESSAGING_SMOKE_TARGET}
    messaging/smoke_test.cpp
)

target_include_directories(${MESSAGING_SMOKE_TARGET}
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${MESSAGING_SMOKE_TARGET}
    PRIVATE
    gtest
    gtest_main
    vigine
    ${GLM_LIBRARIES}
)

set_target_properties(${MESSAGING_SMOKE_TARGET} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

gtest_discover_tests(${MESSAGING_SMOKE_TARGET}
    PROPERTIES LABELS "messaging-smoke"
    DISCOVERY_TIMEOUT 60
    DISCOVERY_MODE PRE_TEST
)

# ====================== Event-Scheduler Smoke Target =======================
# Smoke coverage for IEventScheduler: one-shot timer fires once, cancel
# prevents delivery, and OS signal triggers delivery via mock sources.
set(EVENTSCHEDULER_SMOKE_TARGET eventscheduler-smoke)

add_executable(${EVENTSCHEDULER_SMOKE_TARGET}
    eventscheduler/smoke_test.cpp
)

target_include_directories(${EVENTSCHEDULER_SMOKE_TARGET}
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${EVENTSCHEDULER_SMOKE_TARGET}
    PRIVATE
    gtest
    gtest_main
    vigine
    ${GLM_LIBRARIES}
)

set_target_properties(${EVENTSCHEDULER_SMOKE_TARGET} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

gtest_discover_tests(${EVENTSCHEDULER_SMOKE_TARGET}
    PROPERTIES LABELS "eventscheduler-smoke"
    DISCOVERY_TIMEOUT 60
    DISCOVERY_MODE PRE_TEST
)

