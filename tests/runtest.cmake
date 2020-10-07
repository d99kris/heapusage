macro(run_test EXECUTABLE EXPECTED_RESULT)
    # Read expected result
    file(READ ${EXPECTED_RESULT} EXPECTED)
    STRING(REGEX REPLACE "\n" ";" EXPECTED "${EXPECTED}")
    string(REGEX REPLACE ";$" ""  EXPECTED "${EXPECTED}")

    # Run executable
    execute_process(
        COMMAND
            ${CMAKE_BINARY_DIR}/heapusage -o result.res ${EXECUTABLE}
        RESULT_VARIABLE executable_ok
        ERROR_VARIABLE  errors
    )

    if(executable_ok)
        message(FATAL_ERROR "Test failed (return value = ${executable_ok})")
    endif()

    if(errors)
        message(FATAL_ERROR "Error(s) occured: ${errors}")
    endif()

    # Read results
    file(READ result.res RESULTS)
    STRING(REGEX REPLACE "\n" ";" RESULTS "${RESULTS}")
    string(REGEX REPLACE ";$" ""  RESULTS "${RESULTS}")

    # Compare length
    list(LENGTH EXPECTED length_expected)
    list(LENGTH RESULTS  length_results)
    if(NOT length_results EQUAL length_expected)
        message(FATAL_ERROR "Not the same length !")
    endif()

    # Compare lists
    math(EXPR length "${length_expected} - 1")
    foreach(val RANGE ${length})
        list(GET EXPECTED ${val} val1)
        list(GET RESULTS  ${val} val2)
        string(REGEX MATCH "^==[0-9]*==" PREFIX_1 "${val1}")
        string(REGEX MATCH "^==[0-9]*==" PREFIX_2 "${val2}")
        string(REPLACE "${PREFIX_1}" "" val1 ${val1})
        string(REPLACE "${PREFIX_2}" "" val2 ${val2})
        if(NOT val1 STREQUAL val2)
            message(FATAL_ERROR "${val1}\nnot the same as\n${val2}")
        endif()
    endforeach()
endmacro()

run_test(${EXECUTABLE} ${EXPECTED_RESULT})