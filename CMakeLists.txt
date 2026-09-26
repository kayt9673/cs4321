cmake_minimum_required(VERSION 3.16)

project(vector_relational_db VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(vrdb
    src/db/database_manager.cpp
    src/db/catalog.cpp
    src/db/errors.cpp
    src/query/predicate.cpp
    src/types/data_type.cpp
    src/types/cell.cpp
    src/types/column.cpp
    src/types/schema.cpp
    src/types/row.cpp
    src/storage/serialization.cpp
    src/storage/file_storage_engine.cpp
    src/query/executor.cpp
    src/vector/distance.cpp
)

target_include_directories(vrdb
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

add_executable(vrdb_demo src/main.cpp)
target_link_libraries(vrdb_demo PRIVATE vrdb)

add_executable(vrdb_cli src/cli.cpp)
target_link_libraries(vrdb_cli PRIVATE vrdb)

enable_testing()

find_package(Python3 QUIET COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
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

add_test(
    NAME cli
    COMMAND ${CMAKE_COMMAND}
        -DCLI=$<TARGET_FILE:vrdb_cli>
        -DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/cli_test_database
        -P ${PROJECT_SOURCE_DIR}/tests/test_cli.cmake
)
