#----------------------------------------------------------------------------
# Setup the project
cmake_minimum_required(VERSION 2.6 FATAL_ERROR)
project(digi)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
add_definitions("-std=c++11 -march=native -mtune=native")

#---Locate the ROOT package and defines a number of variables (e.g. ROOT_INCLUDE_DIRS)
list(APPEND CMAKE_PREFIX_PATH $ENV{ROOTSYS})
# set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "$ENV{ROOTSYS}/ect/cmake")
find_package(ROOT REQUIRED COMPONENTS RIO Net)
include(${ROOT_USE_FILE})

#----------------------------------------------------------------------------
# Locate sources and headers for this project
# NB: headers are included so they will show up in IDEs
#
include_directories(${CMAKE_CURRENT_SOURCE_DIR}
                    ${CMAKE_CURRENT_SOURCE_DIR}/include
                    ${ROOT_INCLUDE_DIR})
link_directories(${ROOT_LIBRARY_DIR})
file(GLOB headers ${PROJECT_SOURCE_DIR}/include/*.hpp ${PROJECT_SOURCE_DIR}/include/*.h)
file(GLOB sources ${PROJECT_SOURCE_DIR}/src/*.cpp)


#----------------------------------------------------------------------------
# Add the executable, and link it to the Geant4 libraries
#
add_executable(digi main.cpp ${sources} ${headers})
target_link_libraries(digi ${ROOT_LIBRARIES} CAENDigitizer RHTTP)
