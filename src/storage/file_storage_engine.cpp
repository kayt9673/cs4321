#include "storage/file_storage_engine.hpp"

#include "db/errors.hpp"

#include <algorithm>
#include <fstream>
#include <limits>
#include <memory>
#include <optional>
#include <utility>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace vrdb {
namespace {

// Publish only complete files. Never remove the old destination before replacing it.
void replaceFile(const std::filesystem::path& temporary, const std::filesystem::path& destination) {
#if defined(_WIN32)
    const bool exists{std::filesystem::exists(destination)};
    const BOOL replaced{exists ? ReplaceFileW(destination.c_str(), temporary.c_str(), nullptr, 0, nullptr, nullptr)
                               : MoveFileExW(temporary.c_str(), destination.c_str(), 0)};
    if (!replaced) {
        const auto error{GetLastError()};
        throw StorageError{"failed to publish " + destination.string() +
                           " (Windows error " + std::to_string(error) + ")"};
    }
#else
    std::error_code error;
    std::filesystem::rename(temporary, destination, error);
    if (error) throw StorageError{"failed to publish " + destination.string() + ": " + error.message()};
#endif
}

// Older files can retain a reservation from a previous, interrupted upgrade.
RowId legacyHighWater(const std::filesystem::path& path, const CsvFormat& format, std::size_t rowCount) {
    if (rowCount >= std::numeric_limits<RowId>::max() - 1) throw StorageError{"RowId space exhausted"};
    const RowId minimum{static_cast<RowId>(rowCount) + 1};
    if (!std::filesystem::exists(path)) return minimum;
    std::ifstream file{path};
    if (!file) throw StorageError{"failed to read RowId counter"};
    return std::max(minimum, format.readNextId(file));
}

} // namespace

struct TableCursor::Impl {
    std::ifstream file;
    CsvFormat::TableReader reader;
    std::optional<RowId> nextId;
    std::string tableName;

    Impl(const std::filesystem::path& path, std::string name, const Schema& schema,
         std::optional<RowId> storedNextId)
        : file{path}, reader{file, name, schema}, nextId{storedNextId},
          tableName{std::move(name)} {
        if (!file) {
            throw StorageError{"failed to read table storage for " + tableName};
        }
    }
};

TableCursor::TableCursor(std::unique_ptr<Impl> impl) : impl_{std::move(impl)} {}
TableCursor::~TableCursor() = default;
TableCursor::TableCursor(TableCursor&&) noexcept = default;
TableCursor& TableCursor::operator=(TableCursor&&) noexcept = default;

std::unique_ptr<Page> TableCursor::readNextPage(std::size_t maxRows) {
    auto rows{impl_->reader.readNext(maxRows)};
    if (impl_->file.bad()) {
        throw StorageError{"failed to read table storage for " + impl_->tableName};
    }
    if (impl_->nextId && std::any_of(rows.begin(), rows.end(), [this](const StoredRow& row) {
            return row.id >= *impl_->nextId;
        })) {
        throw StorageError{"RowId counter does not exceed stored IDs for " + impl_->tableName};
    }
    // An exhausted cursor no longer needs its handle. In particular, ordinary
    // Windows input streams deny delete-sharing and would block table replacement.
    if (impl_->reader.finished() && impl_->file.is_open()) impl_->file.close();
    auto page{std::make_unique<Page>(nextPageSequence_, maxRows)};
    for (auto& row : rows) {
        if (!page->addRow(std::move(row))) {
            throw StorageError{"cursor produced more rows than the page capacity"};
        }
    }
    if (!page->empty()) ++nextPageSequence_;
    return page;
}

bool TableCursor::finished() const { return impl_->reader.finished(); }

// Set the database directory and create its table and catalog directories if needed.
FileStorageEngine::FileStorageEngine(std::filesystem::path rootDirectory)
    : rootDirectory_{std::move(rootDirectory)} {
    std::filesystem::create_directories(rootDirectory_ / "tables");
    std::filesystem::create_directories(rootDirectory_ / "catalogs");
}

// Create a table CSV file with its schema header.
void FileStorageEngine::createTable(const std::string& tableName, const Schema& schema) {
    for (const auto& column : schema.columns()) {
        if (column.name == "__vrdb_row_id" || column.name == "__vrdb_null_bitmap") {
            throw SchemaError{"reserved physical column name: " + column.name};
        }
    }
    if (std::filesystem::exists(tablePath(tableName)) || std::filesystem::exists(nextIdPath(tableName))) {
        throw StorageError{"table storage already exists for " + tableName};
    }
    std::ofstream file{tablePath(tableName)};
    if (!file) {
        throw StorageError{"failed to create table storage for " + tableName};
    }
    format_.writeTable(file, schema, {});
    file.close();
    if (!file) {
        throw StorageError{"failed to write table header for " + tableName};
    }
    try {
        writeNextId(tableName, 1);
    } catch (...) {
        std::error_code ignored{};
        std::filesystem::remove(tablePath(tableName), ignored);
        throw;
    }
}

// Validate a row and append its CSV record to the table file.
RowId FileStorageEngine::appendRow(const std::string& tableName, const Schema& schema, const Row& row) {
    schema.validateRow(row);
    auto data{readTable(tableName, schema)};
    const RowId id{data.legacy ? legacyHighWater(nextIdPath(tableName), format_, data.rows.size()) : readNextId(tableName)};
    if (id == std::numeric_limits<RowId>::max()) {
        throw StorageError{"RowId space exhausted for " + tableName};
    }
    // Reserve the ID before writing the row; failed writes may skip IDs, never reuse them.
    writeNextId(tableName, id + 1);
    if (data.needsUpgrade) {
        data.rows.push_back(StoredRow{id, row});
        writeTable(tableName, schema, data.rows);
        return id;
    }
    std::ofstream file{tablePath(tableName), std::ios::app};
    if (!file) {
        throw StorageError{"failed to append row to " + tableName};
    }
    Page page{0, 1};
    page.addRow(StoredRow{id, row});
    format_.writePage(file, page);
    file.close();
    if (!file) {
        throw StorageError{"failed to write row to " + tableName};
    }
    return id;
}

bool FileStorageEngine::updateRow(const std::string& tableName, const Schema& schema,
                                  RowId id, const Row& row) {
    schema.validateRow(row);
    auto data{readTable(tableName, schema)};
    const auto found{std::find_if(data.rows.begin(), data.rows.end(), [id](const StoredRow& stored) {
        return stored.id == id;
    })};
    if (found == data.rows.end()) {
        return false;
    }
    if (data.legacy) {
        writeNextId(tableName, legacyHighWater(nextIdPath(tableName), format_, data.rows.size()));
    }
    found->row = row;
    writeTable(tableName, schema, data.rows);
    return true;
}

bool FileStorageEngine::deleteRow(const std::string& tableName, const Schema& schema, RowId id) {
    auto data{readTable(tableName, schema)};
    const auto found{std::find_if(data.rows.begin(), data.rows.end(), [id](const StoredRow& stored) {
        return stored.id == id;
    })};
    if (found == data.rows.end()) {
        return false;
    }
    if (data.legacy) {
        writeNextId(tableName, legacyHighWater(nextIdPath(tableName), format_, data.rows.size()));
    }
    data.rows.erase(found);
    writeTable(tableName, schema, data.rows);
    return true;
}

// Check the table header and deserialize all stored rows.
std::vector<StoredRow> FileStorageEngine::readRows(const std::string& tableName,
                                                   const Schema& schema) const {
    auto cursor{openTableCursor(tableName, schema)};
    std::vector<StoredRow> rows{};
    constexpr std::size_t batchSize{1024};
    while (!cursor.finished()) {
        auto batch{cursor.readNextPage(batchSize)->takeRows()};
        rows.insert(rows.end(), std::make_move_iterator(batch.begin()),
                    std::make_move_iterator(batch.end()));
    }
    return rows;
}

TableCursor FileStorageEngine::openTableCursor(const std::string& tableName,
                                               const Schema& schema) const {
    const auto path{tablePath(tableName)};
    std::ifstream probe{path};
    if (!probe) {
        throw StorageError{"failed to read table storage for " + tableName};
    }
    CsvFormat::TableReader header{probe, tableName, schema};
    const auto nextId{header.legacy() ? std::optional<RowId>{} :
                                      std::optional<RowId>{readNextId(tableName)}};
    return TableCursor{std::make_unique<TableCursor::Impl>(path, tableName, schema, nextId)};
}

CsvFormat::TableData FileStorageEngine::readTable(const std::string& tableName,
                                                  const Schema& schema) const {
    std::ifstream file{tablePath(tableName)};
    if (!file) {
        throw StorageError{"failed to read table storage for " + tableName};
    }

    auto data{format_.readTable(file, tableName, schema)};
    if (file.bad()) {
        throw StorageError{"failed to read table storage for " + tableName};
    }
    if (!data.legacy) {
        const auto nextId{readNextId(tableName)};
        if (std::any_of(data.rows.begin(), data.rows.end(), [nextId](const StoredRow& row) {
                return row.id >= nextId;
            })) {
            throw StorageError{"RowId counter does not exceed stored IDs for " + tableName};
        }
    }
    return data;
}

void FileStorageEngine::writeTable(const std::string& tableName, const Schema& schema,
                                   const std::vector<StoredRow>& rows) const {
    const auto temporary{tablePath(tableName).string() + ".tmp"};
    std::ofstream file{temporary, std::ios::trunc};
    if (!file) {
        throw StorageError{"failed to rewrite table " + tableName};
    }
    format_.writeTable(file, schema, rows);
    file.close();
    if (!file) {
        throw StorageError{"failed to write updated table " + tableName};
    }
    replaceFile(temporary, tablePath(tableName));
}

RowId FileStorageEngine::readNextId(const std::string& tableName) const {
    std::ifstream file{nextIdPath(tableName)};
    if (!file) {
        throw StorageError{"missing RowId counter for " + tableName};
    }
    return format_.readNextId(file);
}

void FileStorageEngine::writeNextId(const std::string& tableName, RowId id) const {
    if (std::filesystem::exists(nextIdPath(tableName))) {
        id = std::max(id, readNextId(tableName));
    }
    const auto temporary{nextIdPath(tableName).string() + ".tmp"};
    std::ofstream file{temporary, std::ios::trunc};
    if (!file) {
        throw StorageError{"failed to write RowId counter for " + tableName};
    }
    format_.writeNextId(file, id);
    file.close();
    if (!file) {
        throw StorageError{"failed to write RowId counter for " + tableName};
    }
    replaceFile(temporary, nextIdPath(tableName));
}

// Delete the table storage file or report a failure.
void FileStorageEngine::dropTable(const std::string& tableName) {
    std::error_code error{};
    const bool removed{std::filesystem::remove(tablePath(tableName), error)};
    if (error || !removed) {
        throw StorageError{"failed to remove table storage for " + tableName};
    }
    std::filesystem::remove(nextIdPath(tableName), error);
    if (error) {
        throw StorageError{"failed to remove RowId counter for " + tableName};
    }
}

// Build the CSV file path for a table name.
std::filesystem::path FileStorageEngine::tablePath(const std::string& tableName) const {
    return rootDirectory_ / "tables" / (tableName + format_.extension);
}

std::filesystem::path FileStorageEngine::nextIdPath(const std::string& tableName) const {
    return rootDirectory_ / "tables" / (tableName + ".nextid");
}

// Build the catalog file path for a validated table name.
std::filesystem::path FileStorageEngine::catalogPath(const std::string& tableName) const {
    return rootDirectory_ / "catalogs" / (tableName + format_.extension);
}

std::vector<std::string> FileStorageEngine::listTables() const {
    std::vector<std::string> names{};
    for (const auto& entry : std::filesystem::directory_iterator{rootDirectory_ / "catalogs"}) {
        if (entry.is_regular_file() && entry.path().extension() == format_.extension) {
            names.push_back(entry.path().stem().string());
        }
    }
    return names;
}

Schema FileStorageEngine::readSchema(const std::string& tableName) const {
    std::ifstream file{catalogPath(tableName)};
    if (!file) {
        throw StorageError{"unable to read catalog for table: " + tableName};
    }
    auto schema{format_.readSchema(file, tableName)};
    if (file.bad()) {
        throw StorageError{"failed to read catalog for " + tableName};
    }
    return schema;
}

void FileStorageEngine::writeSchema(const std::string& tableName, const Schema& schema) {
    const auto path{catalogPath(tableName)};
    if (std::filesystem::exists(path)) {
        throw StorageError{"catalog already exists for " + tableName};
    }
    const auto temporary{path.string() + ".tmp"};
    std::ofstream file{temporary, std::ios::trunc};
    if (!file) {
        throw StorageError{"failed to write catalog for " + tableName};
    }
    format_.writeSchema(file, tableName, schema);
    file.close();
    if (!file) {
        throw StorageError{"failed to finish catalog for " + tableName};
    }
    replaceFile(temporary, path);
}

void FileStorageEngine::removeSchema(const std::string& tableName) {
    const auto path{catalogPath(tableName)};
    std::error_code error{};
    if (!std::filesystem::remove(path, error) || error) {
        throw StorageError{"failed to remove catalog " + path.string()};
    }
}

} // namespace vrdb
