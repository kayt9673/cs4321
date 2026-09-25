if(NOT DEFINED CLI OR NOT DEFINED TEST_ROOT)
    message(FATAL_ERROR "CLI and TEST_ROOT must be provided")
endif()

file(REMOVE_RECURSE "${TEST_ROOT}")

function(run_cli)
    execute_process(
        COMMAND "${CLI}" "${TEST_ROOT}" ${ARGN}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "CLI command failed: ${ARGN}\n${error}")
    endif()
    set(CLI_OUTPUT "${output}" PARENT_SCOPE)
endfunction()

run_cli(init)
if(NOT EXISTS "${TEST_ROOT}/catalog.csv")
    message(FATAL_ERROR "init did not create catalog.csv")
endif()

run_cli(create documents id:INTEGER title:TEXT "embedding:VECTOR(3)")
run_cli(insert documents 1 "hello, CSV" "[0.1,-0.2,0.3]")
run_cli(list)
if(NOT CLI_OUTPUT MATCHES "documents")
    message(FATAL_ERROR "reopened database did not list documents")
endif()

run_cli(describe documents)
if(NOT CLI_OUTPUT MATCHES "embedding +\\| VECTOR\\(3\\)")
    message(FATAL_ERROR "reopened database did not restore vector schema")
endif()

run_cli(select documents)
if(NOT CLI_OUTPUT MATCHES "row_id" OR NOT CLI_OUTPUT MATCHES "hello, CSV")
    message(FATAL_ERROR "reopened database did not restore inserted row")
endif()

run_cli(select documents --csv)
if(NOT CLI_OUTPUT MATCHES "\"row_id\",\"id\",\"title\",\"embedding\"" OR
   NOT CLI_OUTPUT MATCHES "\"1\",\"1\",\"hello, CSV\"")
    message(FATAL_ERROR "CSV output does not include the persistent RowId")
endif()

run_cli(update documents 1 7 "updated text" "[-1,0,1]")
run_cli(select documents --csv)
if(NOT CLI_OUTPUT MATCHES "\"1\",\"7\",\"updated text\"")
    message(FATAL_ERROR "update did not persist across CLI processes")
endif()

run_cli(delete documents 1)
run_cli(select documents --csv)
if(CLI_OUTPUT MATCHES "updated text")
    message(FATAL_ERROR "delete did not persist across CLI processes")
endif()
run_cli(insert documents 8 "new row" "[0,0,0]")
if(NOT CLI_OUTPUT MATCHES "row 2")
    message(FATAL_ERROR "deleted RowId was reused")
endif()

execute_process(
    COMMAND "${CLI}" "${TEST_ROOT}" delete documents 1
    RESULT_VARIABLE missing_result
    ERROR_VARIABLE missing_error
)
if(missing_result EQUAL 0 OR NOT missing_error MATCHES "unknown RowId 1")
    message(FATAL_ERROR "deleting an unknown RowId should fail clearly")
endif()

if(NOT EXISTS "${TEST_ROOT}/tables/documents.csv")
    message(FATAL_ERROR "create did not create the table CSV")
endif()
if(NOT EXISTS "${TEST_ROOT}/tables/documents.nextid")
    message(FATAL_ERROR "create did not create the persistent RowId counter")
endif()

file(REMOVE_RECURSE "${TEST_ROOT}")
