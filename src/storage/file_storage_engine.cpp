#include "storage/file_storage_engine.h"

#include "db/errors.h"
#include "storage/serialization.h"

#include <algorithm>
#include <fstream>
#include <limits>
#include <unordered_set>
#include <utility>

namespace vrdb {
namespace {

constexpr const char* rowIdHeader = "__vrdb_row_id";

RowId parseRowId(const std::string& field, const std::string& context) {
    if (field.empty() || !std::all_of(field.begin(), field.end(), [](char character) {
            return character >= '0' && character <= '9';
        })) {
        throw StorageError("invalid RowId in " + context + ": " + field);
    }
    try {
        std::size_t parsed = 0;
        const auto id = std::stoull(field, &parsed);
        if (parsed != field.size() || id == 0 || id > std::numeric_limits<RowId>::max()) {
            throw StorageError("invalid RowId in " + context + ": " + field);
        }
        return static_cast<RowId>(id);
    } catch (const StorageError&) {
        throw;
    } catch (const std::exception&) {
        throw StorageError("invalid RowId in " + context + ": " + field);
    }
}

std::vector<std::string> tableHeader(const Schema& schema) {
    std::vector<std::string> header{rowIdHeader};
    header.reserve(schema.size() + 1);
    for (const auto& column : schema.columns()) {
        header.push_back(column.name);
    }
    return header;
}

std::vector<std::string> storedRowFields(const StoredRow& stored) {
    auto fields = serializeRowForCsv(stored.row);
    fields.insert(fields.begin(), std::to_string(stored.id));
    return fields;
}

} // namespace

FileStorageEngine::FileStorageEngine(std::filesystem::path rootDirectory)
    : rootDirectory_(std::move(rootDirectory)) {
    std::filesystem::create_directories(rootDirectory_);
}

void FileStorageEngine::createTable(const std::string& tableName, const Schema& schema) {
    std::filesystem::create_directories(rootDirectory_);
    std::ofstream file(tablePath(tableName), std::ios::trunc);
    if (!file) {
        throw StorageError("failed to create table storage for " + tableName);
    }
    writeCsvRecord(file, tableHeader(schema));
    file.close();
    if (!file) {
        throw StorageError("failed to write table header for " + tableName);
    }
    try {
        writeNextId(tableName, 1);
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(tablePath(tableName), ignored);
        throw;
    }
}

RowId FileStorageEngine::appendRow(const std::string& tableName, const Schema& schema, const Row& row) {
    schema.validateRow(row);
    auto data = readTable(tableName, schema);
    const RowId next = data.legacy ? static_cast<RowId>(data.rows.size()) + 1 : readNextId(tableName);
    if (next == std::numeric_limits<RowId>::max()) {
        throw StorageError("RowId space exhausted for " + tableName);
    }

    // Reserve the identity before changing the table. A failed write may leave a gap,
    // but a committed RowId is never reused after a later deletion.
    writeNextId(tableName, next + 1);
    data.rows.push_back(StoredRow{next, row});
    writeTable(tableName, schema, data.rows);
    return next;
}

bool FileStorageEngine::updateRow(const std::string& tableName, const Schema& schema, RowId id, const Row& row) {
    schema.validateRow(row);
    auto data = readTable(tableName, schema);
    const auto found = std::find_if(data.rows.begin(), data.rows.end(), [id](const StoredRow& stored) {
        return stored.id == id;
    });
    if (found == data.rows.end()) {
        return false;
    }
    found->row = row;
    if (data.legacy) {
        writeNextId(tableName, static_cast<RowId>(data.rows.size()) + 1);
    }
    writeTable(tableName, schema, data.rows);
    return true;
}

bool FileStorageEngine::deleteRow(const std::string& tableName, const Schema& schema, RowId id) {
    auto data = readTable(tableName, schema);
    const auto found = std::find_if(data.rows.begin(), data.rows.end(), [id](const StoredRow& stored) {
        return stored.id == id;
    });
    if (found == data.rows.end()) {
        return false;
    }
    if (data.legacy) {
        writeNextId(tableName, static_cast<RowId>(data.rows.size()) + 1);
    }
    data.rows.erase(found);
    writeTable(tableName, schema, data.rows);
    return true;
}

std::vector<StoredRow> FileStorageEngine::readRows(const std::string& tableName, const Schema& schema) const {
    return readTable(tableName, schema).rows;
}

FileStorageEngine::TableData FileStorageEngine::readTable(
    const std::string& tableName, const Schema& schema) const {
    std::ifstream file(tablePath(tableName));
    if (!file) {
        throw StorageError("failed to read table storage for " + tableName);
    }
    std::vector<std::string> fields;
    if (!readCsvRecord(file, fields)) {
        throw StorageError("table CSV is missing its header: " + tableName);
    }
    const bool legacy = fields.size() == schema.size();
    const std::size_t expectedWidth = schema.size() + (legacy ? 0 : 1);
    if (fields.size() != expectedWidth || (!legacy && fields[0] != rowIdHeader)) {
        throw StorageError("table CSV header width or RowId column does not match schema: " + tableName);
    }
    for (std::size_t index = 0; index < schema.size(); ++index) {
        if (fields[index + (legacy ? 0 : 1)] != schema.column(static_cast<ColumnId>(index)).name) {
            throw StorageError("table CSV header does not match schema: " + tableName);
        }
    }

    TableData data{{}, legacy};
    std::unordered_set<RowId> seen;
    while (readCsvRecord(file, fields)) {
        if (fields.size() != expectedWidth) {
            throw StorageError("stored row width does not match schema: " + tableName);
        }
        RowId id = 0;
        if (legacy) {
            if (data.rows.size() >= std::numeric_limits<RowId>::max() - 1) {
                throw StorageError("too many legacy rows in " + tableName);
            }
            id = static_cast<RowId>(data.rows.size()) + 1;
        } else {
            id = parseRowId(fields.front(), tableName);
            fields.erase(fields.begin());
            if (!seen.insert(id).second) {
                throw StorageError("duplicate RowId " + std::to_string(id) + " in " + tableName);
            }
        }
        data.rows.push_back(StoredRow{id, deserializeRowFromCsv(fields, schema)});
    }
    if (!legacy) {
        const auto next = readNextId(tableName);
        for (const auto& stored : data.rows) {
            if (stored.id >= next) {
                throw StorageError("RowId counter does not exceed stored IDs for " + tableName);
            }
        }
    }
    return data;
}

void FileStorageEngine::writeTable(
    const std::string& tableName, const Schema& schema, const std::vector<StoredRow>& rows) const {
    const auto temporary = tablePath(tableName).string() + ".tmp";
    std::ofstream file(temporary, std::ios::trunc);
    if (!file) {
        throw StorageError("failed to rewrite table " + tableName);
    }
    writeCsvRecord(file, tableHeader(schema));
    for (const auto& stored : rows) {
        writeCsvRecord(file, storedRowFields(stored));
    }
    file.close();
    if (!file) {
        throw StorageError("failed to write updated table " + tableName);
    }
    std::error_code error;
    std::filesystem::rename(temporary, tablePath(tableName), error);
    if (error) {
        throw StorageError("failed to replace table " + tableName + ": " + error.message());
    }
}

RowId FileStorageEngine::readNextId(const std::string& tableName) const {
    std::ifstream file(nextIdPath(tableName));
    std::string value;
    if (!file || !std::getline(file, value)) {
        throw StorageError("missing RowId counter for " + tableName);
    }
    return parseRowId(value, tableName + " RowId counter");
}

void FileStorageEngine::writeNextId(const std::string& tableName, RowId id) const {
    const auto temporary = nextIdPath(tableName).string() + ".tmp";
    std::ofstream file(temporary, std::ios::trunc);
    if (!file) {
        throw StorageError("failed to write RowId counter for " + tableName);
    }
    file << id << '\n';
    file.close();
    if (!file) {
        throw StorageError("failed to write RowId counter for " + tableName);
    }
    std::error_code error;
    std::filesystem::rename(temporary, nextIdPath(tableName), error);
    if (error) {
        throw StorageError("failed to replace RowId counter for " + tableName + ": " + error.message());
    }
}

void FileStorageEngine::dropTable(const std::string& tableName) {
    std::error_code error;
    const bool removed = std::filesystem::remove(tablePath(tableName), error);
    if (error || !removed) {
        throw StorageError("failed to remove table storage for " + tableName);
    }
    std::filesystem::remove(nextIdPath(tableName), error);
    if (error) {
        throw StorageError("failed to remove RowId counter for " + tableName);
    }
}

std::filesystem::path FileStorageEngine::tablePath(const std::string& tableName) const {
    return rootDirectory_ / (tableName + ".csv");
}

std::filesystem::path FileStorageEngine::nextIdPath(const std::string& tableName) const {
    return rootDirectory_ / (tableName + ".nextid");
}

} // namespace vrdb
