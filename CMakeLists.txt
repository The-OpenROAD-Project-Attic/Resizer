# Resizer, Verilog/DEF gate resizer
# Copyright (c) 2019, Parallax Software, Inc.
# 
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

cmake_minimum_required (VERSION 3.9)

project(resizer VERSION 1.2.1)

set(RESIZER_HOME ${PROJECT_SOURCE_DIR})
set(OPENSTA_HOME ${PROJECT_SOURCE_DIR}/module/OpenSTA)
set(LEF_HOME ${PROJECT_SOURCE_DIR}/module/lef)
set(DEF_HOME ${PROJECT_SOURCE_DIR}/module/def)
set(FLUTE_HOME ${PROJECT_SOURCE_DIR}/module/flute)

set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Disable compiler specific extensions like gnu++11.
set(CMAKE_CXX_EXTENSIONS OFF)

set(RESIZER_VERSION ${PROJECT_VERSION})
message(STATUS "Resizer version: ${RESIZER_VERSION}")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(GetGitRevisionDescription)
get_git_head_revision(GIT_REFSPEC RESIZER_GIT_SHA1)
message(STATUS "Resizer git sha: ${RESIZER_GIT_SHA1}")

# Default to bulding optimnized/release executable.
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RELEASE)
endif()

# Compiler specific options.
# Note -Wno-deprecated-register is to suppress bison errors.
if ((CMAKE_CXX_COMPILER_ID MATCHES AppleClang|Clang)
    OR (CMAKE_CXX_COMPILER_ID STREQUAL "GNU"))
  set(COMPILE_OPTIONS -Wall -Wextra -pedantic -Wcast-qual -Wredundant-decls -Wformat-security -Wno-deprecated-register -Wno-ignored-qualifiers)
endif()

message(STATUS "System name: ${CMAKE_SYSTEM_NAME}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Build CXX_FLAGS: ${CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE}}")
message(STATUS "CXX_FLAGS: ${COMPILE_OPTIONS}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")

################################################################
#
# Library dependencies
#
################################################################

# Zlib
include(FindZLIB)
# translate cmake bool to ifdef bool
if (ZLIB_FOUND)
  set(ZLIB 1)
else()
  set(ZLIB 0)
endif()

################################################################
#
# Locate CUDD bdd packagte
# -DCUDD=0 to not use CUDD.
# Look for library in CUDD/lib,     CUDD/cudd/lib
# Look for header  in CUDD/include, CUDD/cudd/include
#
if("${CUDD}" STREQUAL "" OR "${CUDD}" STREQUAL "0")
  set(CUDD_INCLUDE "")
  set(CUDD_LIB "")
  set(CUDD_FOUND FALSE)
  set(CUDD 0)
  message(STATUS "CUDD library: not found")
else()
  find_library(CUDD_LIB NAMES cudd PATHS ${CUDD}/lib ${CUDD}/lib/cudd)
  if (CUDD_LIB)
    message(STATUS "CUDD library: ${CUDD_LIB}")
    get_filename_component(CUDD_LIB_DIR "${CUDD_LIB}" PATH)
    get_filename_component(CUDD_LIB_PARENT1 "${CUDD_LIB_DIR}" PATH)
    get_filename_component(CUDD_LIB_PARENT2 "${CUDD_LIB_PARENT1}" PATH)
    find_file(CUDD_HEADER cudd.h
      PATHS ${CUDD}/include ${CUDD_LIB_PARENT2}/include/cudd)
    if (CUDD_HEADER)
      get_filename_component(CUDD_INCLUDE "${CUDD_HEADER}" PATH)
      message(STATUS "CUDD header: ${CUDD_HEADER}")
      # CUDD referenced by StaConfig.hh.cmake
      set(CUDD 1)
    else()
      message(STATUS "CUDD header: not found")
    endif()
  else()
    set(CUDD_INCLUDE "")
    set(CUDD_LIB "")
    set(CUDD_FOUND FALSE)
    set(CUDD 0)
    message(STATUS "CUDD library: not found")
  endif()
endif()

if("${SSTA}" STREQUAL "")
  set(SSTA 0)
endif()
message(STATUS "SSTA: ${SSTA}")

# configure a header file to pass some of the CMake settins
configure_file(${RESIZER_HOME}/ResizerConfig.hh.cmake
  ${RESIZER_HOME}/ResizerConfig.hh
  )

################################################################
#
# Locate TCL library.
#
# Note that the cmake findTcl module is hopeless for OSX
# because there doesn't appear to be a way to override
# searching OSX system directories before unix directories.

set(TCL_POSSIBLE_NAMES tcl87 tcl8.7
  tcl86 tcl8.6
  tcl85 tcl8.5
  tcl84 tcl8.4
  tcl83 tcl8.3
  tcl82 tcl8.2
  )

# tcl lib path guesses.
if (NOT TCL_LIB_PATHS)
  if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(TCL_LIB_PATHS /usr/local/lib)
    set(TCL_NO_DEFAULT_PATH TRUE)
  endif()
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(TCL_LIB_PATHS /usr/lib
    /usr/local/lib
    )
  set(TCL_NO_DEFAULT_PATH FALSE)
endif()

if (NOT TCL_LIB)
  # bagbiter cmake doesn't have a way to pass NO_DEFAULT_PATH as a parameter.
  if (TCL_NO_DEFAULT_PATH)
    find_library(TCL_LIB
      NAMES tcl ${TCL_POSSIBLE_NAMES}
      PATHS ${TCL_LIB_PATHS}
      NO_DEFAULT_PATH
      )
  else()
    find_library(TCL_LIB
      NAMES tcl ${TCL_POSSIBLE_NAMES}
      PATHS ${TCL_LIB_PATHS}
      )
  endif()
endif()
message(STATUS "TCL lib: ${TCL_LIB}")

get_filename_component(TCL_LIB_DIR "${TCL_LIB}" PATH)
get_filename_component(TCL_LIB_PARENT1 "${TCL_LIB_DIR}" PATH)
get_filename_component(TCL_LIB_PARENT2 "${TCL_LIB_PARENT1}" PATH)

# Locate tcl.h
if (NOT TCL_HEADER)
  find_file(TCL_HEADER tcl.h
    PATHS ${TCL_LIB_PARENT1} ${TCL_LIB_PARENT2}
    PATH_SUFFIXES include include/tcl
    NO_DEFAULT_PATH
    )
endif()
message(STATUS "TCL header: ${TCL_HEADER}")
get_filename_component(TCL_HEADER_DIR "${TCL_HEADER}" PATH)

################################################################

set(STA_INCLUDE_DIRS
  ${OPENSTA_HOME}/app
  ${OPENSTA_HOME}/dcalc
  ${OPENSTA_HOME}/graph
  ${OPENSTA_HOME}/liberty
  ${OPENSTA_HOME}/network
  ${OPENSTA_HOME}/parasitics
  ${OPENSTA_HOME}/sdc
  ${OPENSTA_HOME}/sdf
  ${OPENSTA_HOME}/search
  ${OPENSTA_HOME}/util
  ${OPENSTA_HOME}/verilog
  )

################################################################

set(RESIZER_SOURCE
  DefReader.cc
  DefWriter.cc
  LefReader.cc
  LefDefNetwork.cc
  LefDefSdcNetwork.cc
  Resizer.cc
  ResizerMain.cc
  Resizer_wrap.cc
  ResizerTclInitVar.cc
  SteinerTree.cc
  )

set(RESIZER_HEADERS
  DefReader.hh
  DefWriter.hh
  LefReader.hh
  LefDefNetwork.hh
  LefDefSdcNetwork.hh
  Resizer.hh
  SteinerTree.hh
  )

set(RESIZER_INCLUDE_DIRS
  ${STA_INCLUDE_DIRS}
  ${LEF_HOME}/lef
  ${DEF_HOME}/def
  ${FLUTE_HOME}
  )

set(RESIZER_TCL_FILES
  Resizer.tcl
  )

set(RESIZER_SWIG_FILES
  Resizer.i
  )

add_custom_command(OUTPUT ${RESIZER_HOME}/ResizerTclInitVar.cc
  COMMAND ${OPENSTA_HOME}/etc/TclEncode.tcl ${RESIZER_HOME}/ResizerTclInitVar.cc resizer_tcl_inits ${RESIZER_TCL_FILES}
  WORKING_DIRECTORY ${RESIZER_HOME}
  DEPENDS ${RESIZER_TCL_FILES} ${OPENSTA_HOME}/etc/TclEncode.tcl
  )

include(FindSWIG)
add_custom_command(OUTPUT ${RESIZER_HOME}/Resizer_wrap.cc
  COMMAND ${SWIG_EXECUTABLE} -tcl8 -c++ -namespace -prefix sta -I${OPENSTA_HOME}/tcl -I${OPENSTA_HOME}/sdf -I${OPENSTA_HOME}/dcalc -I${OPENSTA_HOME}/parasitics -I${OPENSTA_HOME}/verilog -o ${RESIZER_HOME}/Resizer_wrap.cc ${RESIZER_HOME}/Resizer.i
  COMMAND ${OPENSTA_HOME}/etc/SwigCleanup.tcl ${RESIZER_HOME}/Resizer_wrap.cc
  WORKING_DIRECTORY ${OPENSTA_HOME}
  DEPENDS ${RESIZER_SWIG_FILES}
  )

################################################################

add_subdirectory(${DEF_HOME})
add_subdirectory(${LEF_HOME})
add_subdirectory(${FLUTE_HOME})
add_subdirectory(${OPENSTA_HOME})

################################################################

add_executable(resizer
  ${RESIZER_SOURCE}
  )

target_include_directories(resizer PUBLIC ${RESIZER_INCLUDE_DIRS})

target_link_libraries(resizer
  OpenSTA
  ${TCL_LIB}
  ${CUDD_LIB}
  lef
  def
  flute
  )

if (ZLIB_FOUND)
  target_link_libraries(resizer ${ZLIB_LIBRARIES})
endif()

# g++ std::thread apparently still needs -pthreads.
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_link_libraries(resizer -pthread)
endif()

target_compile_options(resizer PUBLIC ${COMPILE_OPTIONS})

################################################################
#
# verilog2def
#
################################################################

add_executable(verilog2def
  VerilogToDef.cc
  LefDefNetwork.cc
  DefWriter.cc
  LefReader.cc
  )

target_include_directories(verilog2def PUBLIC ${RESIZER_INCLUDE_DIRS})

target_link_libraries(verilog2def
  OpenSTA
  ${TCL_LIB}
  ${CUDD_LIB}
  lef
  def
  )

if (ZLIB_FOUND)
  target_link_libraries(verilog2def ${ZLIB_LIBRARIES})
endif()

# g++ std::thread apparently still needs -pthreads.
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_link_libraries(verilog2def -pthread)
endif()

target_compile_options(verilog2def PUBLIC ${COMPILE_OPTIONS})

################################################################
# Install
# cmake .. -DCMAKE_INSTALL_PREFIX=<prefix_path>

# executable
install(TARGETS resizer DESTINATION bin)

################################################################

add_custom_target(resizer-tags etags -o TAGS ${RESIZER_SOURCE} ${RESIZER_HEADERS} ${RESIZER_TCL_FILES}
  WORKING_DIRECTORY ${RESIZER_HOME}
  DEPENDS ${RESIZER_SOURCE} ${RESIZER_HEADERS} ${RESIZER_TCL_FILES}
  )
