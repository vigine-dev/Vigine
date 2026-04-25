cmake_minimum_required(VERSION 3.10)

project(vigine_tests)

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

# Apply the central warning / strict-mode compile flags to keep the
# test executables held to the same /WX + -Werror gate as the vigine
# library. GoogleTest itself is added via add_subdirectory above and
# does NOT receive these flags -- third-party noise stays out of our
# error budget.
vigine_apply_compile_options(${GRAPH_CONTRACT_TARGET})

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

vigine_apply_compile_options(${PAYLOAD_SMOKE_TARGET})

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

vigine_apply_compile_options(${MESSAGING_SMOKE_TARGET})

gtest_discover_tests(${MESSAGING_SMOKE_TARGET}
    PROPERTIES LABELS "messaging-smoke"
    DISCOVERY_TIMEOUT 60
    DISCOVERY_MODE PRE_TEST
)

# ====================== Event-Scheduler Smoke Target =======================
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

vigine_apply_compile_options(${EVENTSCHEDULER_SMOKE_TARGET})

gtest_discover_tests(${EVENTSCHEDULER_SMOKE_TARGET}
    PROPERTIES LABELS "eventscheduler-smoke"
    DISCOVERY_TIMEOUT 60
    DISCOVERY_MODE PRE_TEST
)

# ====================== Topic-Bus Smoke Target =======================
set(TOPICBUS_SMOKE_TARGET topicbus-smoke)

add_executable(${TOPICBUS_SMOKE_TARGET}
    topicbus/smoke_test.cpp
)

target_include_directories(${TOPICBUS_SMOKE_TARGET}
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${TOPICBUS_SMOKE_TARGET}
    PRIVATE
    gtest
    gtest_main
    vigine
    ${GLM_LIBRARIES}
)

set_target_properties(${TOPICBUS_SMOKE_TARGET} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

vigine_apply_compile_options(${TOPICBUS_SMOKE_TARGET})

gtest_discover_tests(${TOPICBUS_SMOKE_TARGET}
    PROPERTIES LABELS "topicbus-smoke"
    DISCOVERY_TIMEOUT 60
    DISCOVERY_MODE PRE_TEST
)

# ====================== ChannelFactory Smoke Target =======================
set(CHANNELFACTORY_SMOKE_TARGET channelfactory-smoke)

add_executable(${CHANNELFACTORY_SMOKE_TARGET}
    channelfactory/smoke_test.cpp
)

target_include_directories(${CHANNELFACTORY_SMOKE_TARGET}
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${CHANNELFACTORY_SMOKE_TARGET}
    PRIVATE
    gtest
    gtest_main
    vigine
    ${GLM_LIBRARIES}
)

set_target_properties(${CHANNELFACTORY_SMOKE_TARGET} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

vigine_apply_compile_options(${CHANNELFACTORY_SMOKE_TARGET})

gtest_discover_tests(${CHANNELFACTORY_SMOKE_TARGET}
    PROPERTIES LABELS "channelfactory-smoke"
    DISCOVERY_TIMEOUT 60
    DISCOVERY_MODE PRE_TEST
)

# ====================== RequestBus Smoke Target =======================
set(REQUESTBUS_SMOKE_TARGET requestbus-smoke)

add_executable(${REQUESTBUS_SMOKE_TARGET}
    requestbus/smoke_test.cpp
)

target_include_directories(${REQUESTBUS_SMOKE_TARGET}
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${REQUESTBUS_SMOKE_TARGET}
    PRIVATE
    gtest
    gtest_main
    vigine
    ${GLM_LIBRARIES}
)

set_target_properties(${REQUESTBUS_SMOKE_TARGET} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

vigine_apply_compile_options(${REQUESTBUS_SMOKE_TARGET})

gtest_discover_tests(${REQUESTBUS_SMOKE_TARGET}
    PROPERTIES LABELS "requestbus-smoke"
    DISCOVERY_TIMEOUT 60
    DISCOVERY_MODE PRE_TEST
)

# ====================== ReactiveStream Smoke Target =======================
set(REACTIVESTREAM_SMOKE_TARGET reactivestream-smoke)

add_executable(${REACTIVESTREAM_SMOKE_TARGET}
    reactivestream/smoke_test.cpp
)

target_include_directories(${REACTIVESTREAM_SMOKE_TARGET}
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${REACTIVESTREAM_SMOKE_TARGET}
    PRIVATE
    gtest
    gtest_main
    vigine
    ${GLM_LIBRARIES}
)

set_target_properties(${REACTIVESTREAM_SMOKE_TARGET} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

vigine_apply_compile_options(${REACTIVESTREAM_SMOKE_TARGET})

gtest_discover_tests(${REACTIVESTREAM_SMOKE_TARGET}
    PROPERTIES LABELS "reactivestream-smoke"
    DISCOVERY_TIMEOUT 60
    DISCOVERY_MODE PRE_TEST
)

# ====================== ActorHost Smoke Target =======================
set(ACTORHOST_SMOKE_TARGET actorhost-smoke)

add_executable(${ACTORHOST_SMOKE_TARGET}
    actorhost/smoke_test.cpp
)

target_include_directories(${ACTORHOST_SMOKE_TARGET}
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${ACTORHOST_SMOKE_TARGET}
    PRIVATE
    gtest
    gtest_main
    vigine
    ${GLM_LIBRARIES}
)

set_target_properties(${ACTORHOST_SMOKE_TARGET} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

vigine_apply_compile_options(${ACTORHOST_SMOKE_TARGET})

gtest_discover_tests(${ACTORHOST_SMOKE_TARGET}
    PROPERTIES LABELS "actorhost-smoke"
    DISCOVERY_TIMEOUT 60
    DISCOVERY_MODE PRE_TEST
)

# ====================== PipelineBuilder Smoke Target =======================
set(PIPELINEBUILDER_SMOKE_TARGET pipelinebuilder-smoke)

add_executable(${PIPELINEBUILDER_SMOKE_TARGET}
    pipelinebuilder/smoke_test.cpp
)

target_include_directories(${PIPELINEBUILDER_SMOKE_TARGET}
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${PIPELINEBUILDER_SMOKE_TARGET}
    PRIVATE
    gtest
    gtest_main
    vigine
    ${GLM_LIBRARIES}
)

set_target_properties(${PIPELINEBUILDER_SMOKE_TARGET} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

vigine_apply_compile_options(${PIPELINEBUILDER_SMOKE_TARGET})

gtest_discover_tests(${PIPELINEBUILDER_SMOKE_TARGET}
    PROPERTIES LABELS "pipelinebuilder-smoke"
    DISCOVERY_TIMEOUT 60
    DISCOVERY_MODE PRE_TEST
)

# ====================== Engine Smoke Target ============================
# Smoke coverage for IEngine lifecycle: construct-without-run, run +
# shutdown-from-other-thread, shutdown-from-main-thread-callback, and
# double-shutdown idempotency. Also exercises the freeze-after-run
# boundary that blocks context mutation once the pump is live.
set(ENGINE_SMOKE_TARGET engine-smoke)

add_executable(${ENGINE_SMOKE_TARGET}
    engine/smoke_test.cpp
)

target_include_directories(${ENGINE_SMOKE_TARGET}
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${ENGINE_SMOKE_TARGET}
    PRIVATE
    gtest
    gtest_main
    vigine
    ${GLM_LIBRARIES}
)

set_target_properties(${ENGINE_SMOKE_TARGET} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

vigine_apply_compile_options(${ENGINE_SMOKE_TARGET})

gtest_discover_tests(${ENGINE_SMOKE_TARGET}
    PROPERTIES LABELS "engine-smoke"
    DISCOVERY_TIMEOUT 60
    DISCOVERY_MODE PRE_TEST
)

# ====================== EngineToken Smoke Target =======================
# Smoke coverage for the concrete @ref vigine::engine::EngineToken: bound-
# state introspection, hybrid gating (gated accessors flip to Expired on
# invalidation while ungated accessors stay valid), FSM-driven invalidation
# via @ref AbstractStateMachine::addInvalidationListener, exactly-once
# expiration callback firing, defensive same-thread fire when subscribing
# post-invalidation, no-op-transition-no-fire, multi-subscriber + cancel
# round trip, and per-state isolation between independent tokens.
set(ENGINETOKEN_SMOKE_TARGET enginetoken-smoke)

add_executable(${ENGINETOKEN_SMOKE_TARGET}
    engine_token/smoke_test.cpp
)

target_include_directories(${ENGINETOKEN_SMOKE_TARGET}
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${ENGINETOKEN_SMOKE_TARGET}
    PRIVATE
    gtest
    gtest_main
    vigine
    ${GLM_LIBRARIES}
)

set_target_properties(${ENGINETOKEN_SMOKE_TARGET} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

vigine_apply_compile_options(${ENGINETOKEN_SMOKE_TARGET})

gtest_discover_tests(${ENGINETOKEN_SMOKE_TARGET}
    PROPERTIES LABELS "enginetoken-smoke"
    DISCOVERY_TIMEOUT 60
    DISCOVERY_MODE PRE_TEST
)

# ====================== Sync Primitives Smoke Target ===================
# Smoke coverage for the four factory-produced sync primitives on
# IThreadManager: IMutex (lock / tryLock / lockFor timeout), ISemaphore
# (acquire / release / count / tryAcquire), IBarrier (two-party
# arriveAndWait round trip), IMessageChannel (send / receive / close).
# Level-0 primitive — wrappers above all depend on these working; a
# dedicated smoke target turns regressions into localised failures.
set(SYNC_SMOKE_TARGET sync-smoke)

add_executable(${SYNC_SMOKE_TARGET}
    threading/sync_smoke_test.cpp
)

target_include_directories(${SYNC_SMOKE_TARGET}
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${SYNC_SMOKE_TARGET}
    PRIVATE
    gtest
    gtest_main
    vigine
    ${GLM_LIBRARIES}
)

set_target_properties(${SYNC_SMOKE_TARGET} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

vigine_apply_compile_options(${SYNC_SMOKE_TARGET})

gtest_discover_tests(${SYNC_SMOKE_TARGET}
    PROPERTIES LABELS "sync-smoke"
    DISCOVERY_TIMEOUT 60
    DISCOVERY_MODE PRE_TEST
)

# ====================== ThreadManager Smoke Target ====================
# Smoke coverage for the IThreadManager lifecycle and scheduling surface:
# factory returns a non-null manager; poolSize matches config; schedule
# with Pool / Any / Dedicated affinities runs runnables and resolves
# their handles; scheduleOnNamed round trip registers, executes, and
# unregisters a named thread; stale-id scheduling returns an error handle;
# postToMainThread + runMainThreadPump fires queued runnables exactly once
# on the calling thread; shutdown joins workers and rejects subsequent
# schedules; double-shutdown is a no-op; pending pool tasks drain before
# shutdown returns.
set(THREADMANAGER_SMOKE_TARGET threadmanager-smoke)

add_executable(${THREADMANAGER_SMOKE_TARGET}
    threading/threadmanager_smoke_test.cpp
)

target_include_directories(${THREADMANAGER_SMOKE_TARGET}
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${THREADMANAGER_SMOKE_TARGET}
    PRIVATE
    gtest
    gtest_main
    vigine
    ${GLM_LIBRARIES}
)

set_target_properties(${THREADMANAGER_SMOKE_TARGET} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

vigine_apply_compile_options(${THREADMANAGER_SMOKE_TARGET})

gtest_discover_tests(${THREADMANAGER_SMOKE_TARGET}
    PROPERTIES LABELS "threadmanager-smoke"
    DISCOVERY_TIMEOUT 60
    DISCOVERY_MODE PRE_TEST
)

# ====================== Service Smoke Target ===========================
# Smoke coverage for the IService Level-1 wrapper: createService()
# hands back a live default service; dependencies() is empty on a
# fresh one; isInitialised() flips across onInit / onShutdown and
# stays stable under a repeat onInit. Exercises the wrapper without
# the context-aggregator fixture so regressions fail in isolation.
set(SERVICE_SMOKE_TARGET service-smoke)

add_executable(${SERVICE_SMOKE_TARGET}
    service/smoke_test.cpp
)

target_include_directories(${SERVICE_SMOKE_TARGET}
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${SERVICE_SMOKE_TARGET}
    PRIVATE
    gtest
    gtest_main
    vigine
    ${GLM_LIBRARIES}
)

set_target_properties(${SERVICE_SMOKE_TARGET} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

vigine_apply_compile_options(${SERVICE_SMOKE_TARGET})

gtest_discover_tests(${SERVICE_SMOKE_TARGET}
    PROPERTIES LABELS "service-smoke"
    DISCOVERY_TIMEOUT 60
    DISCOVERY_MODE PRE_TEST
)

# ====================== StateMachine Smoke Target =======================
# Smoke coverage for the IStateMachine Level-1 wrapper: default state
# auto-provisioning, addState / setInitial / transition happy paths,
# stale-id rejection, hierarchy round trips (addChildState + parent +
# isAncestorOf), single-parent invariant, cycle detection at the
# addChildState boundary, and RouteMode getter/setter.
set(STATEMACHINE_SMOKE_TARGET statemachine-smoke)

add_executable(${STATEMACHINE_SMOKE_TARGET}
    statemachine/smoke_test.cpp
)

target_include_directories(${STATEMACHINE_SMOKE_TARGET}
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${STATEMACHINE_SMOKE_TARGET}
    PRIVATE
    gtest
    gtest_main
    vigine
    ${GLM_LIBRARIES}
)

set_target_properties(${STATEMACHINE_SMOKE_TARGET} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

vigine_apply_compile_options(${STATEMACHINE_SMOKE_TARGET})

gtest_discover_tests(${STATEMACHINE_SMOKE_TARGET}
    PROPERTIES LABELS "statemachine-smoke"
    DISCOVERY_TIMEOUT 60
    DISCOVERY_MODE PRE_TEST
)

# ====================== ECS Smoke Target ================================
# Smoke coverage for the ECS Level-1 wrapper: create + attach churn
# across 1k entities, component lookup round-trip, entitiesWith
# iteration under attach/remove churn, and the cascade on
# removeEntity. Exercises `createECS()` directly so regressions in
# the underlying entity-world storage (tombstone accumulation, missed
# cascades, iteration-under-churn) fail here instead of hiding
# behind a full engine fixture.
set(ECS_SMOKE_TARGET ecs-smoke)

add_executable(${ECS_SMOKE_TARGET}
    ecs/smoke_test.cpp
)

target_include_directories(${ECS_SMOKE_TARGET}
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(${ECS_SMOKE_TARGET}
    PRIVATE
    gtest
    gtest_main
    vigine
    ${GLM_LIBRARIES}
)

set_target_properties(${ECS_SMOKE_TARGET} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

vigine_apply_compile_options(${ECS_SMOKE_TARGET})

gtest_discover_tests(${ECS_SMOKE_TARGET}
    PROPERTIES LABELS "ecs-smoke"
    DISCOVERY_TIMEOUT 60
    DISCOVERY_MODE PRE_TEST
)

# ====================== Full-Stack Contract Suite =======================
# End-to-end contract coverage for the unified substrate: engine
# lifecycle, threading primitives, messaging bus, every Level-2 facade
# (signal / event / topic / channel / request / reactive / actor /
# pipeline), and the Level-1 wrappers (service / ECS / statemachine /
# taskflow) through the public IContext aggregator. Each scenario lives
# in its own .cpp under test/contract/ and shares the engine fixture in
# test/contract/fixtures/engine_fixture.h.
set(FULL_CONTRACT_TARGET full-contract)

add_executable(${FULL_CONTRACT_TARGET}
    contract/scenario_01_engine_lifecycle.cpp
    contract/scenario_02_threading_primitives.cpp
    contract/scenario_03_messaging_bus.cpp
    contract/scenario_04_signal_emitter.cpp
    contract/scenario_05_event_scheduler.cpp
    contract/scenario_06_topic_isolation.cpp
    contract/scenario_07_channel_backpressure.cpp
    contract/scenario_08_request_response.cpp
    contract/scenario_09_reactive_pipeline.cpp
    contract/scenario_10_actor_host.cpp
    contract/scenario_11_pipeline_builder.cpp
    contract/scenario_12_service_lifecycle.cpp
    contract/scenario_13_ecs_entity_lifecycle.cpp
    contract/scenario_14_statemachine_hsm.cpp
    contract/scenario_18_fsm_async_transition.cpp
    contract/scenario_21_stale_engine_token.cpp
)

target_include_directories(${FULL_CONTRACT_TARGET}
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/contract
)

target_link_libraries(${FULL_CONTRACT_TARGET}
    PRIVATE
    gtest
    gtest_main
    vigine
    ${GLM_LIBRARIES}
)

set_target_properties(${FULL_CONTRACT_TARGET} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

vigine_apply_compile_options(${FULL_CONTRACT_TARGET})

gtest_discover_tests(${FULL_CONTRACT_TARGET}
    PROPERTIES LABELS "full-contract"
    DISCOVERY_TIMEOUT 60
    DISCOVERY_MODE PRE_TEST
)

# ====================== Linux Platform Smoke Target =======================
# Three scenarios: XCB window open/close, SIGTERM self-pipe delivery,
# SIGUSR1/SIGUSR2 subscribe succeeds. Gated to Linux only; invisible to
# Windows/macOS CI jobs.
if(UNIX AND NOT APPLE)
    set(LINUX_PLATFORM_SMOKE_TARGET linux-platform-smoke)

    add_executable(${LINUX_PLATFORM_SMOKE_TARGET}
        platform/linux_smoke_test.cpp
    )

    target_include_directories(${LINUX_PLATFORM_SMOKE_TARGET}
        PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${XCB_INCLUDE_DIRS}
    )

    target_link_libraries(${LINUX_PLATFORM_SMOKE_TARGET}
        PRIVATE
        gtest
        gtest_main
        vigine
        ${GLM_LIBRARIES}
        ${XCB_LIBRARIES}
    )

    set_target_properties(${LINUX_PLATFORM_SMOKE_TARGET} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )

    vigine_apply_compile_options(${LINUX_PLATFORM_SMOKE_TARGET})

    include(GoogleTest)
    gtest_discover_tests(${LINUX_PLATFORM_SMOKE_TARGET}
        PROPERTIES LABELS "linux-platform-smoke"
        DISCOVERY_TIMEOUT 60
        DISCOVERY_MODE PRE_TEST
    )
endif() # UNIX AND NOT APPLE

