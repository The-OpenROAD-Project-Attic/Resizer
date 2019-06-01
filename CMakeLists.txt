# OpenSTA, Static Timing Analyzer
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

project(sizer VERSION 2.0.15)

set(RESIZER_HOME ${PROJECT_SOURCE_DIR})
set(STA_HOME ${PROJECT_SOURCE_DIR}/../opensta)
set(LEF_HOME ${PROJECT_SOURCE_DIR}/../lef/5.8-p027)
set(DEF_HOME ${PROJECT_SOURCE_DIR}/../def/5.8-p027)

set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Disable compiler specific extensions like gnu++11.
set(CMAKE_CXX_EXTENSIONS OFF)

message(STATUS "STA version: ${PROJECT_VERSION}")

# Default to bulding optimnized/release executable.
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RELEASE)
endif()

# Compiler specific options.
if (CMAKE_CXX_COMPILER_ID MATCHES AppleClang|Clang)
  set(COMPILE_OPTIONS -Wall -Wcast-qual -Wunused-parameter -Wredundant-decls -Wno-deprecated-register)
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  set(COMPILE_OPTIONS -Wall -Wcast-qual -Wunused-parameter -Wredundant-decls)
endif()

message(STATUS "System name: ${CMAKE_SYSTEM_NAME}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Build CXX_FLAGS: ${CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE}}")
message(STATUS "CXX_FLAGS: ${COMPILE_OPTIONS}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")

################################################################
#
# Source files.
#
################################################################

# Source files.
set(STA_TCL_FILES
  ${STA_HOME}/tcl/Util.tcl
  ${STA_HOME}/tcl/Graph.tcl
  ${STA_HOME}/tcl/Liberty.tcl
  ${STA_HOME}/tcl/Link.tcl
  ${STA_HOME}/tcl/Network.tcl
  ${STA_HOME}/tcl/NetworkEdit.tcl
  ${STA_HOME}/tcl/Sdc.tcl
  ${STA_HOME}/tcl/Search.tcl
  ${STA_HOME}/tcl/Cmds.tcl
  ${STA_HOME}/tcl/Variables.tcl
  ${STA_HOME}/tcl/Sta.tcl
  ${STA_HOME}/tcl/Power.tcl
  ${STA_HOME}/tcl/Splash.tcl
  ${STA_HOME}/dcalc/DelayCalc.tcl
  ${STA_HOME}/parasitics/Parasitics.tcl
  ${STA_HOME}/sdf/Sdf.tcl
  )

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
configure_file(${STA_HOME}/util/StaConfig.hh.cmake
  ${STA_HOME}/util/StaConfig.hh
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
  ${STA_HOME}/app
  ${STA_HOME}/dcalc
  ${STA_HOME}/graph
  ${STA_HOME}/liberty
  ${STA_HOME}/network
  ${STA_HOME}/parasitics
  ${STA_HOME}/sdc
  ${STA_HOME}/sdf
  ${STA_HOME}/search
  ${STA_HOME}/util
  ${STA_HOME}/verilog
  )

################################################################

set(RESIZER_SOURCE
  DefReader.cc
  DefWriter.cc
  LefReader.cc
  LefDefNetwork.cc
  Resizer.cc
  ResizerMain.cc
  Resizer_wrap.cc
  ResizerTclInitVar.cc
  )

set(RESIZER_HEADERS
  DefReader.hh
  DefWriter.hh
  LefReader.hh
  LefDefNetwork.hh
  Resizer.hh
  )

set(RESIZER_INCLUDE_DIRS
  ${STA_INCLUDE_DIRS}
  ${LEF_HOME}/lef
  ${DEF_HOME}/def
  )

set(RESIZER_TCL_FILES
  ${STA_TCL_FILES}
  Resizer.tcl
  )

set(RESIZER_SWIG_FILES
  Resizer.i
  )

# Dependencies for Resizer.i
set(STA_SWIG_FILES
  ${STA_HOME}/tcl/NetworkEdit.i
  ${STA_HOME}/tcl/StaException.i
  ${STA_HOME}/dcalc/DelayCalc.i
  ${STA_HOME}/parasitics/Parasitics.i
  ${STA_HOME}/verilog/Verilog.i
  ${STA_HOME}/tcl/StaTcl.i
  )

add_custom_command(OUTPUT ${RESIZER_HOME}/ResizerTclInitVar.cc
  COMMAND ${STA_HOME}/etc/TclEncode.tcl ${RESIZER_HOME}/ResizerTclInitVar.cc resizer_tcl_inits ${RESIZER_TCL_FILES}
  WORKING_DIRECTORY ${RESIZER_HOME}
  DEPENDS ${RESIZER_TCL_FILES} ${STA_HOME}/etc/TclEncode.tcl
  )

include(FindSWIG)
add_custom_command(OUTPUT ${RESIZER_HOME}/Resizer_wrap.cc
  COMMAND ${SWIG_EXECUTABLE} -tcl8 -c++ -namespace -prefix sta -I${STA_HOME}/tcl -I${STA_HOME}/sdf -I${STA_HOME}/dcalc -I${STA_HOME}/parasitics -I${STA_HOME}/verilog -o ${RESIZER_HOME}/Resizer_wrap.cc ${RESIZER_HOME}/Resizer.i
  COMMAND ${STA_HOME}/etc/SwigCleanup.tcl ${RESIZER_HOME}/Resizer_wrap.cc
  WORKING_DIRECTORY ${STA_HOME}
  DEPENDS ${RESIZER_SWIG_FILES} ${STA_SWIG_FILES}
  )

################################################################

add_executable(resizer ${RESIZER_SOURCE})

target_include_directories(resizer PUBLIC ${RESIZER_INCLUDE_DIRS})

target_link_libraries(resizer
  ${STA_HOME}/app/libOpenSTAnoCmds.a
  ${TCL_LIB}
  ${CUDD_LIB}
  ${LEF_HOME}/lef/liblef.a
  ${DEF_HOME}/def/libdef.a
  )

if (ZLIB_FOUND)
  target_link_libraries(resizer ${ZLIB_LIBRARIES})
endif()
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
  target_link_libraries(resizer -pthread)
endif()

################################################################
# Install
# cmake .. -DCMAKE_INSTALL_PREFIX=<prefix_path>

# executable
install(TARGETS resizer DESTINATION bin)

################################################################

add_custom_target(tags etags -o TAGS ${RESIZER_SOURCE} ${RESIZER_HEADERS} ${RESIZER_TCL_FILES}
  WORKING_DIRECTORY ${RESIZER_HOME}
  DEPENDS ${RESIZER_SOURCE} ${RESIZER_HEADERS} ${RESIZER_TCL_FILES}
  )
