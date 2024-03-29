cmake_minimum_required(VERSION 3.5)

project(hcana VERSION 0.96 LANGUAGES CXX)

option(HCANA_BUILTIN_PODD "Use built-in Podd submodule (default: YES)" OFF)

include(cmake/os.cmake)
include(FindThreads)

find_package(ROOT 6.0 REQUIRED CONFIG)
include(${ROOT_USE_FILE})

#https://github.com/fmtlib/fmt.git
find_package(fmt REQUIRED)
get_target_property(FMT_INCLUDE_DIR fmt::fmt INTERFACE_INCLUDE_DIRECTORIES)
set(SPDLOG_FMT_EXTERNAL TRUE)
#add_definitions(-DSPDLOG_FMT_EXTERNAL )
find_package(spdlog REQUIRED)
if(NOT spdlog_FOUND) 
  message("spdlog not found.")
  message(FATAL_ERROR "spdlog should be installed with analyzer if it is not available on the system.")
  #set(SPDLOG_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/src/3rd_party/spdlog/include)
else()
  get_target_property(SPDLOG_INCLUDE_DIR spdlog::spdlog INTERFACE_INCLUDE_DIRECTORIES)
endif()

find_package(coda_et REQUIRED)
get_target_property(CODA_ET_INCLUDE_DIR coda_et::coda_et INTERFACE_INCLUDE_DIRECTORIES)

find_package(EVIO REQUIRED)
get_target_property(EVIO_INCLUDE_DIR EVIO::EVIO INTERFACE_INCLUDE_DIRECTORIES)

#----------------------------------------------------------------------------
# Set up Podd and ROOT dependencies
if(HCANA_BUILTIN_PODD)
  set(CMAKE_MODULE_PATH
    ${PROJECT_SOURCE_DIR}/podd/cmake/Modules
    ${CMAKE_MODULE_PATH}
    )
  include(PoddCMakeEnv)
else()
  # Find Podd and register it as a dependency
  # This will also automatically set up ROOT
  find_package(Podd 1.6 REQUIRED)
  include(PoddCMakeEnv)
  config_add_dependency(Podd 1.6)
endif()

# Register ROOT dependency - it's in our public interface
#config_add_dependency(ROOT 5.10)

#----------------------------------------------------------------------------
# Set up the compiler flags
#set_compiler_flags("${ROOT_CXX_FLAGS}")
#set_diagnostic_flags(WALL WEXTRA)
#report_build_info()



#----------------------------------------------------------------------------
# Install in GNU-style directory layout
include(GNUInstallDirs)
set(INSTALL_CONFIGDIR ${CMAKE_INSTALL_LIBDIR}/${MAIN_PROJECT_NAME_LC})

#----------------------------------------------------------------------------
# Directories in which we build things
if(HCANA_BUILTIN_PODD)
  add_subdirectory(podd)
endif()
add_subdirectory(src)
add_subdirectory(cmake)
add_subdirectory(tools)
