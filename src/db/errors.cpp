#include "db/errors.hpp"

#include <utility>

namespace vrdb {

DatabaseError::DatabaseError(const std::string& message)
    : std::runtime_error(message) {}

SchemaError::SchemaError(const std::string& message)
    : DatabaseError(message) {}

StorageError::StorageError(const std::string& message)
    : DatabaseError(message) {}

QueryError::QueryError(const std::string& message)
    : DatabaseError(message) {}

} // namespace vrdb
