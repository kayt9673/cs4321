#include "db/errors.hpp"

#include <utility>

namespace vrdb {

// Construct a database error with the supplied message.
DatabaseError::DatabaseError(const std::string& message)
    : std::runtime_error{message} {}

// Construct a schema-validation error with the supplied message.
SchemaError::SchemaError(const std::string& message)
    : DatabaseError{message} {}

// Construct a storage error with the supplied message.
StorageError::StorageError(const std::string& message)
    : DatabaseError{message} {}

// Construct a query error with the supplied message.
QueryError::QueryError(const std::string& message)
    : DatabaseError{message} {}

} // namespace vrdb
