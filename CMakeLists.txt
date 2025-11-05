cmake_minimum_required(VERSION 3.23)

project(
  VibrantLanguage
  VERSION 0.1.0
)

# Hint for buildsystem
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt")
  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/compiler")
else()
  message(FATAL_ERROR "Invalid path to Vibrant compiler: ${CMAKE_CURRENT_SOURCE_DIR}")
endif()

# Install
include(GNUInstallDirs)

install(TARGETS VibrantCompiler
  FILE_SET HEADERS DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)