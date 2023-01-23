cmake_minimum_required(VERSION 3.9)
cmake_policy(SET CMP0042 NEW)
set (CMAKE_CXX_STANDARD 11)
project (vnot)

include_directories(${CMAKE_JS_INC})
add_definitions(-DNAPI_VERSION=3)
execute_process(COMMAND node -p "require('node-addon-api').include" WORKING_DIRECTORY ${CMAKE_SOURCE_DIR} OUTPUT_VARIABLE NODE_ADDON_API_DIR)
string(REGEX REPLACE "[\r\n\"]" "" NODE_ADDON_API_DIR ${NODE_ADDON_API_DIR})

if(WIN32)
  # Windows
  file(GLOB SOURCE_FILES "src/TUN/win32/main.cc")
  add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES} ${CMAKE_JS_SRC})
  target_link_libraries(${PROJECT_NAME} ${CMAKE_JS_LIB})
  set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "" SUFFIX ".node")

elseif(LINUX)
  # Linux
  file(GLOB SOURCE_FILES "src/TUN/linux/main.cc")

elseif(APPLE)
  # Mac
  file(GLOB SOURCE_FILES "src/TUN/darwin/main.cc")

endif()


if(MSVC AND CMAKE_JS_NODELIB_DEF AND CMAKE_JS_NODELIB_TARGET)
  # Generate node.lib
  execute_process(COMMAND ${CMAKE_AR} /def:${CMAKE_JS_NODELIB_DEF} /out:${CMAKE_JS_NODELIB_TARGET} ${CMAKE_STATIC_LINKER_FLAGS})
endif()