project(tests)
find_library(CMOCKA_LIBRARY cmocka)

include_directories(
    ${DIR_INC}
    ${DIR_SRC})

function(dlm_add_test name vlg)
    set(files ${ARGN})

    add_executable(${name} ${files})

    add_test("${name}" test_vms)
    if (${vlg})
        add_test("${name}_valgrind" valgrind
                --error-exitcode=1 --read-var-info=yes
                --leak-check=full --show-leak-kinds=all
                ./${name})
    endif()

    target_link_libraries("${name}" cmocka dlm)
endfunction()

dlm_add_test(test_vms 1 mem/test_vms.c test_base.h mem/test_mem.h)
dlm_add_test(test_cl 0 mem/test_cl.c test_base.h mem/test_mem.h)
