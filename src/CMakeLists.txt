cmake_minimum_required(VERSION 2.6)
project(dlm C)

set(CMAKE_C_STANDARD 99)
find_package(OpenCL REQUIRED)
if (${OpenCL_VERSION_STRING} VERSION_LESS "1.2")
    message(FATAL_ERROR "OpenCl v1.2 or greater required, current is v${OpenCL_VERSION_STRING}")
endif()
find_package(Threads REQUIRED)

include_directories(.)
include_directories(${DIR_INC})
include_directories(${OpenCL_INCLUDE_DIRS})

set(SOURCE_FILES
        # extra
        ${DIR_INC}/dlm/compiler.h
        ${DIR_INC}/dlm/list.h
        ${DIR_INC}/dlm/poison.h
        ${DIR_INC}/dlm/stddef.h
        ${DIR_INC}/dlm/object.h
        ${DIR_INC}/dlm/providers/magic.h

        # memory
        ${DIR_INC}/dlm/memory.h
        generic.h
        generic.c
        event_pthread_mtx.c

        # virtual memory storage
        ${DIR_INC}/dlm/providers/vms.h
        providers/vms.c

        # opencl devices
        ${DIR_INC}/dlm/providers/opencl.h
        providers/opencl.c

        # infiniband verbs devices
        ${DIR_INC}/dlm/providers/ib.h
        providers/ib.c
)

add_library(dlm ${SOURCE_FILES})
target_link_libraries(dlm
    OpenCL::OpenCL
    ${CMAKE_THREAD_LIBS_INIT})