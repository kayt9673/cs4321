cmake_minimum_required(VERSION 3.16)

project(vector_relational_db VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(vrdb
    src/db/database.cpp
    src/db/catalog.cpp
    src/db/errors.cpp
    src/query/predicate.cpp
    src/types/value.cpp
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

enable_testing()

add_executable(test_storage tests/test_storage.cpp)
target_link_libraries(test_storage PRIVATE vrdb)
add_test(NAME storage COMMAND test_storage)

add_executable(test_predicates tests/test_predicates.cpp)
target_link_libraries(test_predicates PRIVATE vrdb)
add_test(NAME predicates COMMAND test_predicates)

add_executable(test_distance tests/test_distance.cpp)
target_link_libraries(test_distance PRIVATE vrdb)
add_test(NAME distance COMMAND test_distance)
