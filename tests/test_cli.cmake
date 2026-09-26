if(NOT DEFINED CLI OR NOT DEFINED TEST_ROOT)
    message(FATAL_ERROR "CLI and TEST_ROOT must be provided")
endif()

file(REMOVE_RECURSE "${TEST_ROOT}")

# Run a CLI command, fail on errors, and expose its output to the caller.
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
if(NOT IS_DIRECTORY "${TEST_ROOT}/catalogs")
    message(FATAL_ERROR "init did not create catalogs directory")
endif()

run_cli(create documents id:INTEGER title:TEXT "embedding:VECTOR(3)")
if(NOT EXISTS "${TEST_ROOT}/catalogs/documents.csv")
    message(FATAL_ERROR "create did not write the table catalog")
endif()

run_cli(insert documents 1 "hello_CSV" "[0.1,-0.2,0.3]")
run_cli(list)
if(NOT CLI_OUTPUT MATCHES "documents")
    message(FATAL_ERROR "reopened database did not list documents")
endif()

run_cli(describe documents)
if(NOT CLI_OUTPUT MATCHES "embedding +\\| VECTOR\\(3\\)")
    message(FATAL_ERROR "reopened database did not restore vector schema")
endif()

run_cli(select documents)
if(NOT CLI_OUTPUT MATCHES "hello_CSV")
    message(FATAL_ERROR "reopened database did not restore inserted row")
endif()

run_cli(select documents --csv)
if(NOT CLI_OUTPUT MATCHES "\"id\",\"title\",\"embedding\"" OR
   NOT CLI_OUTPUT MATCHES "\"1\",\"hello_CSV\"")
    message(FATAL_ERROR "CSV output does not match logical table columns")
endif()

if(NOT EXISTS "${TEST_ROOT}/tables/documents.csv")
    message(FATAL_ERROR "create did not create the table CSV")
endif()

run_cli(insert documents 2 "float32" "[16777216,1.00000011920928955078125,1e30]")
run_cli(select documents)
if(NOT CLI_OUTPUT MATCHES "16777216,1[.]00000012")
    message(FATAL_ERROR "CLI did not preserve float32 vector precision")
endif()

file(REMOVE_RECURSE "${TEST_ROOT}")
