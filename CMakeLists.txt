cmake_minimum_required(VERSION 3.16)

project(vector_relational_db VERSION 0.1.0 LANGUAGES CXX)

find_package(Threads REQUIRED)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(hnswlib_headers INTERFACE)
target_include_directories(hnswlib_headers SYSTEM INTERFACE
    ${PROJECT_SOURCE_DIR}/third_party/hnswlib)

add_library(vrdb
    src/db/database_manager.cpp
    src/db/catalog.cpp
    src/db/index_manager.cpp
    src/db/errors.cpp
    src/query/predicate.cpp
    src/types/data_type.cpp
    src/types/cell.cpp
    src/types/column.cpp
    src/types/schema.cpp
    src/types/row.cpp
    src/storage/serialization.cpp
    src/storage/csv_format.cpp
    src/storage/index_format.cpp
    src/storage/page.cpp
    src/storage/file_storage_engine.cpp
    src/query/executor.cpp
    src/query/nearest_neighbor.cpp
    src/vector/distance.cpp
    src/vector/index.cpp
)

target_include_directories(vrdb
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)
target_link_libraries(vrdb PUBLIC Threads::Threads)
target_link_libraries(vrdb PRIVATE hnswlib_headers)

add_executable(vrdb_demo src/main.cpp)
target_link_libraries(vrdb_demo PRIVATE vrdb)

add_executable(vrdb_cli src/cli.cpp)
target_link_libraries(vrdb_cli PRIVATE vrdb)

add_executable(vrdb_query_benchmark benchmarks/query_benchmark.cpp)
target_link_libraries(vrdb_query_benchmark PRIVATE vrdb)

add_executable(vrdb_hnsw_benchmark benchmarks/hnsw_benchmark.cpp)
target_link_libraries(vrdb_hnsw_benchmark PRIVATE vrdb)
if(WIN32)
    target_link_libraries(vrdb_hnsw_benchmark PRIVATE psapi)
endif()

enable_testing()

foreach(test_name vector_index nearest_neighbor hnsw_recall)
    add_executable(test_${test_name} tests/test_${test_name}.cpp)
    target_link_libraries(test_${test_name} PRIVATE vrdb)
    add_test(NAME ${test_name} COMMAND test_${test_name})
endforeach()
set_tests_properties(hnsw_recall PROPERTIES TIMEOUT 300)

add_executable(test_null tests/test_null.cpp)
target_link_libraries(test_null PRIVATE vrdb)
add_test(NAME null COMMAND test_null)

find_package(Python3 QUIET COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_test(
        NAME null_cli
        COMMAND ${Python3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/tests/test_null_cli.py
            $<TARGET_FILE:vrdb_cli>
    )
    add_test(
        NAME generator
        COMMAND ${Python3_EXECUTABLE}
            ${PROJECT_SOURCE_DIR}/tests/test_generate_database.py
            $<TARGET_FILE:vrdb_cli>
    )
endif()

add_executable(test_storage tests/test_storage.cpp)
target_link_libraries(test_storage PRIVATE vrdb)
add_test(NAME storage COMMAND test_storage)

add_executable(test_catalog tests/test_catalog.cpp)
target_link_libraries(test_catalog PRIVATE vrdb)
add_test(NAME catalog COMMAND test_catalog)

add_executable(test_csv_format tests/test_csv_format.cpp)
target_link_libraries(test_csv_format PRIVATE vrdb)
add_test(NAME csv_format COMMAND test_csv_format)

add_executable(test_database_ownership tests/test_database_ownership.cpp)
target_link_libraries(test_database_ownership PRIVATE vrdb)
add_test(NAME database_ownership COMMAND test_database_ownership)

add_executable(test_predicates tests/test_predicates.cpp)
target_link_libraries(test_predicates PRIVATE vrdb)
add_test(NAME predicates COMMAND test_predicates)

add_executable(test_varied_tables tests/test_varied_tables.cpp)
target_link_libraries(test_varied_tables PRIVATE vrdb)
add_test(NAME varied_tables COMMAND test_varied_tables)

add_executable(test_distance tests/test_distance.cpp)
target_link_libraries(test_distance PRIVATE vrdb)
add_test(NAME distance COMMAND test_distance)

add_executable(test_schema tests/test_schema.cpp)
target_link_libraries(test_schema PRIVATE vrdb)
add_test(NAME schema COMMAND test_schema)

add_executable(test_cell tests/test_cell.cpp)
target_link_libraries(test_cell PRIVATE vrdb)
add_test(NAME cell COMMAND test_cell)

add_executable(test_row tests/test_row.cpp)
target_link_libraries(test_row PRIVATE vrdb)
add_test(NAME row COMMAND test_row)

add_executable(test_page tests/test_page.cpp)
target_link_libraries(test_page PRIVATE vrdb)
add_test(NAME page COMMAND test_page)

add_test(
    NAME cli
    COMMAND ${CMAKE_COMMAND}
        -DCLI=$<TARGET_FILE:vrdb_cli>
        -DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/cli_test_database
        -P ${PROJECT_SOURCE_DIR}/tests/test_cli.cmake
)
