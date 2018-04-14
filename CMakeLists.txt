cmake_minimum_required(VERSION 2.6)
project(dlm)
include(preferences.cmake)

set(DIR_ROOT ${PROJECT_SOURCE_DIR})
set(DIR_SRC ${DIR_ROOT}/src)
set(DIR_INC ${DIR_ROOT}/include)
set(DIR_BIN ${DIR_ROOT}/bin)
set(DIR_SAMPLE ${DIR_ROOT}/samples)
set(DIR_TESTS ${DIR_ROOT}/tests)

add_subdirectory(${DIR_SRC})
enable_testing()
add_subdirectory(${DIR_TESTS})
