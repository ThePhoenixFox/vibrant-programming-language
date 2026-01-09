# Options
option(VIBRANT_BUILD_STRICT "Sets the highest optimization and errors" ON)
set(VIBRANT_BUILD_OPTIONS "" CACHE STRING "Extra build options for the Vibrant compiler")

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the build type" FORCE)
endif()

# Compiler requirements
message(STATUS "Compiler ID: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "Compiler Version: ${CMAKE_CXX_COMPILER_VERSION}")

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 5.0)
        message(FATAL_ERROR "Clang version must be at least 5.0")
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 7.4)
        message(FATAL_ERROR "GCC version must be at least 7.4")
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 19.28)
        message(FATAL_ERROR "MSVC version must be at least 19.28 (VS 2019 16.8)")
    endif()
else()
    message(FATAL_ERROR "Unsupported compiler: ${CMAKE_CXX_COMPILER_ID}")
endif()

set(CMAKE_C_STANDARD 20)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Get LLVM
find_package(LLVM 21.1.0 REQUIRED CONFIG)

# Report which version of LLVM was found on host
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

llvm_map_components_to_libnames(VIBRANT_LLVM_LIBS
  core
  support
  passes
  irreader
)

add_executable(VibrantCompiler)

# Vibrant Compiler executable properties
set_target_properties(VibrantCompiler PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  # vbc stands for ViBrant Compiler
  OUTPUT_NAME "vbc"
)

# Add files
target_sources(VibrantCompiler
  PRIVATE
    src/main.cpp
    src/lexer.cpp
    src/parser.cpp
    src/analyser.cpp
    src/code-gen.cpp
    src/error.cpp

  PUBLIC
    FILE_SET HEADERS
    BASE_DIRS
      include
    FILES
      include/AST.hpp
      include/common.hpp
      include/passes.hpp
      include/token.hpp
      include/error.hpp
)

# Add headers
target_include_directories(VibrantCompiler
  PUBLIC
    include
    ${LLVM_INCLUDE_DIRS}
)

# Add LLVM definitions
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
target_compile_definitions(VibrantCompiler PRIVATE ${LLVM_DEFINITIONS_LIST})

# Add compile options
set(VIBRANT_TARGET_OPTIONS)

if(VIBRANT_BUILD_STRICT)
  list(APPEND VIBRANT_TARGET_OPTIONS 
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /permissive->
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic -Werror>
  )
endif()

if(VIBRANT_BUILD_OPTIONS)
  separate_arguments(ADDITIONAL_OPTIONS NATIVE_COMMAND ${VIBRANT_BUILD_OPTIONS})
  list(APPEND VIBRANT_TARGET_OPTIONS ${ADDITIONAL_OPTIONS})
endif()

if(VIBRANT_TARGET_OPTIONS)
  target_compile_options(VibrantCompiler PRIVATE ${VIBRANT_TARGET_OPTIONS})
endif()

# Link against LLVM libaries
target_link_libraries(VibrantCompiler
  PUBLIC
    ${VIBRANT_LLVM_LIBS}
)

# Install
include(GNUInstallDirs)

install(TARGETS VibrantCompiler
  FILE_SET HEADERS DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)