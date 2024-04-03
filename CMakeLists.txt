cmake_minimum_required(VERSION 3.25)

set(PLATFORM "win" CACHE STRING "Platform")
set(ARCH "x64" CACHE STRING "Arch")

option(BUILD_SHARED "Option to build shared library" ON)
option(BUILD_STATIC "Option to build static library" ON)

message(STATUS "PLATFORM: ${PLATFORM}")
message(STATUS "ARCH: ${ARCH}")

message(STATUS "BUILD_SHARED: ${BUILD_SHARED}")
message(STATUS "BUILD_STATIC: ${BUILD_STATIC}")

if(PLATFORM STREQUAL "ios")
   set(CMAKE_SYSTEM_NAME iOS)
elseif(PLATFORM STREQUAL "tvos")
   set(CMAKE_SYSTEM_NAME tvOS)
elseif(PLATFORM STREQUAL "android")
   set(CMAKE_SYSTEM_NAME Android)
   set(CMAKE_SYSTEM_VERSION 30)
   set(CMAKE_ANDROID_ARCH_ABI arm64-v8a)
endif()

file(READ src/pupdmd.h version)
string(REGEX MATCH "PUPDMD_VERSION_MAJOR[ ]+([0-9]+)" _tmp ${version})
set(VERSION_MAJOR "${CMAKE_MATCH_1}")
string(REGEX MATCH "PUPDMD_VERSION_MINOR[ ]+([0-9]+)" _tmp ${version})
set(VERSION_MINOR "${CMAKE_MATCH_1}")
string(REGEX MATCH "PUPDMD_VERSION_PATCH[ ]+([0-9]+)" _tmp ${version})
set(VERSION_PATCH "${CMAKE_MATCH_1}")

project(pupdmd VERSION "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}"
   DESCRIPTION "Cross-platform library for trigger PUP events form DMD drame matching.")

if(PLATFORM STREQUAL "win")
   if(ARCH STREQUAL "x86")
      add_compile_definitions(WIN32)
   endif()
elseif(PLATFORM STREQUAL "macos")
   set(CMAKE_OSX_DEPLOYMENT_TARGET 13.0)
   if (ARCH STREQUAL "arm64")
      set(CMAKE_OSX_ARCHITECTURES arm64)
   elseif(ARCH STREQUAL "x64")
      set(CMAKE_OSX_ARCHITECTURES x86_64)
   endif()
   set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
   set(CMAKE_INSTALL_RPATH "@executable_path")
elseif(PLATFORM STREQUAL "ios" OR PLATFORM STREQUAL "tvos")
   set(CMAKE_OSX_DEPLOYMENT_TARGET 16.0)
   set(CMAKE_OSX_ARCHITECTURES arm64)
elseif(PLATFORM STREQUAL "linux" OR PLATFORM STREQUAL "android")
   set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
   set(CMAKE_INSTALL_RPATH "$ORIGIN")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 99)

set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_C_VISIBILITY_PRESET hidden)

set(PUPDMD_SOURCES
   src/pupdmd.h
   src/pupdmd.cpp
)

set(PUPDMD_INCLUDE_DIRS
   src
   third-party/include
)

if(BUILD_SHARED)
   add_library(pupdmd_shared SHARED ${PUPDMD_SOURCES})

   target_include_directories(pupdmd_shared PUBLIC ${PUPDMD_INCLUDE_DIRS})

   if(PLATFORM STREQUAL "win" AND ARCH STREQUAL "x64")
      set(PUPDMD_OUTPUT_NAME "pupdmd64")
   else()
      set(PUPDMD_OUTPUT_NAME "pupdmd")
   endif()

   set_target_properties(pupdmd_shared PROPERTIES
      OUTPUT_NAME ${PUPDMD_OUTPUT_NAME}
      VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}
   )

   install(TARGETS pupdmd_shared
      LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
   )
   install(FILES src/pupdmd.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include)

   if(PLATFORM STREQUAL "win" OR PLATFORM STREQUAL "macos" OR PLATFORM STREQUAL "linux")
      add_executable(pupdmd_test
         src/test.cpp
      )

      target_link_libraries(pupdmd_test PUBLIC pupdmd_shared)
   endif()
endif()

if(BUILD_STATIC)
   add_library(pupdmd_static STATIC ${PUPDMD_SOURCES})

   target_include_directories(pupdmd_static PUBLIC ${PUPDMD_INCLUDE_DIRS})

   if(PLATFORM STREQUAL "win")
      set_target_properties(pupdmd_static PROPERTIES
         OUTPUT_NAME "pupdmd_static"
      )
   else()
      set_target_properties(pupdmd_static PROPERTIES
         OUTPUT_NAME "pupdmd"
      )
   endif()

   install(TARGETS pupdmd_static
      LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
   )
   install(FILES src/pupdmd.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include)

   if(PLATFORM STREQUAL "win" OR PLATFORM STREQUAL "macos" OR PLATFORM STREQUAL "linux")
      add_executable(pupdmd_test_s
         src/test.cpp
      )

      target_link_libraries(pupdmd_test_s PUBLIC pupdmd_static)
   endif()
endif()
