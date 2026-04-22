if(EXISTS "D:/work/project/CodeMap/external/Vigine/build_contract/bin/Debug/payload-smoke.exe")
  if(NOT EXISTS "D:/work/project/CodeMap/external/Vigine/build_contract/test/payload-smoke[1]_tests-Debug.cmake" OR
     NOT "D:/work/project/CodeMap/external/Vigine/build_contract/test/payload-smoke[1]_tests-Debug.cmake" IS_NEWER_THAN "D:/work/project/CodeMap/external/Vigine/build_contract/bin/Debug/payload-smoke.exe" OR
     NOT "D:/work/project/CodeMap/external/Vigine/build_contract/test/payload-smoke[1]_tests-Debug.cmake" IS_NEWER_THAN "${CMAKE_CURRENT_LIST_FILE}")
    include("C:/Program Files/Microsoft Visual Studio/18/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/share/cmake-4.1/Modules/GoogleTestAddTests.cmake")
    gtest_discover_tests_impl(
      TEST_EXECUTABLE [==[D:/work/project/CodeMap/external/Vigine/build_contract/bin/Debug/payload-smoke.exe]==]
      TEST_EXECUTOR [==[]==]
      TEST_WORKING_DIR [==[D:/work/project/CodeMap/external/Vigine/build_contract/test]==]
      TEST_EXTRA_ARGS [==[]==]
      TEST_PROPERTIES [==[LABELS;payload-registry-smoke]==]
      TEST_PREFIX [==[]==]
      TEST_SUFFIX [==[]==]
      TEST_FILTER [==[]==]
      NO_PRETTY_TYPES [==[FALSE]==]
      NO_PRETTY_VALUES [==[FALSE]==]
      TEST_LIST [==[payload-smoke_TESTS]==]
      CTEST_FILE [==[D:/work/project/CodeMap/external/Vigine/build_contract/test/payload-smoke[1]_tests-Debug.cmake]==]
      TEST_DISCOVERY_TIMEOUT [==[60]==]
      TEST_DISCOVERY_EXTRA_ARGS [==[]==]
      TEST_XML_OUTPUT_DIR [==[]==]
    )
  endif()
  include("D:/work/project/CodeMap/external/Vigine/build_contract/test/payload-smoke[1]_tests-Debug.cmake")
else()
  add_test(payload-smoke_NOT_BUILT payload-smoke_NOT_BUILT)
endif()
