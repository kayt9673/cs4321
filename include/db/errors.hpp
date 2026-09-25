#pragma once

#include <stdexcept>
#include <string>

namespace vrdb {

class DatabaseError : public std::runtime_error {
public:
    // Construct a database error with the supplied message.
    explicit DatabaseError(const std::string& message);
};

class SchemaError : public DatabaseError {
public:
    // Construct a schema-validation error with the supplied message.
    explicit SchemaError(const std::string& message);
};

class StorageError : public DatabaseError {
public:
    // Construct a storage error with the supplied message.
    explicit StorageError(const std::string& message);
};

class QueryError : public DatabaseError {
public:
    // Construct a query error with the supplied message.
    explicit QueryError(const std::string& message);
};

} // namespace vrdb
