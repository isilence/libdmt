cmake_minimum_required(VERSION 2.6)
project(test C)

include_directories(${DIR_INC})

set(SOURCE_FILES
        main.c
)

add_executable(sample ${SOURCE_FILES})
target_link_libraries(sample dlm)