#pragma once

#include <stdexcept>
#include <string>

namespace vrdb {

class DatabaseError : public std::runtime_error {
public:
    explicit DatabaseError(const std::string& message);
};

class SchemaError : public DatabaseError {
public:
    explicit SchemaError(const std::string& message);
};

class StorageError : public DatabaseError {
public:
    explicit StorageError(const std::string& message);
};

class QueryError : public DatabaseError {
public:
    explicit QueryError(const std::string& message);
};

} // namespace vrdb
