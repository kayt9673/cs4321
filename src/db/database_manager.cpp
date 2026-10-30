#include "db/database_manager.hpp"

#include "db/errors.hpp"

#include <algorithm>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <type_traits>
#include <utility>

namespace vrdb {
namespace {

class ThreadPool {
public:
    explicit ThreadPool(std::size_t workerCount) {
        workers_.reserve(workerCount);
        for (std::size_t index{0}; index < workerCount; ++index) {
            workers_.emplace_back([this] {
                while (true) {
                    std::function<void()> task{};
                    {
                        std::unique_lock lock{mutex_};
                        ready_.wait(lock, [this] { return stopping_ || !tasks_.empty(); });
                        if (stopping_ && tasks_.empty()) return;
                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }
                    task();
                }
            });
        }
    }

    ~ThreadPool() {
        {
            std::lock_guard lock{mutex_};
            stopping_ = true;
        }
        ready_.notify_all();
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    template <typename Function>
    auto submit(Function&& function) {
        using Result = std::invoke_result_t<Function>;
        auto task{std::make_shared<std::packaged_task<Result()>>(
            std::forward<Function>(function))};
        auto future{task->get_future()};
        {
            std::lock_guard lock{mutex_};
            if (stopping_) throw std::runtime_error{"cannot submit to a stopped thread pool"};
            tasks_.push([task] { (*task)(); });
        }
        ready_.notify_one();
        return future;
    }

private:
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable ready_;
    bool stopping_{false};
    // Declared last so jthreads join before synchronization members are destroyed.
    std::vector<std::jthread> workers_;
};

std::size_t defaultWorkerCount() {
    const auto hardware{std::thread::hardware_concurrency()};
    return std::max<std::size_t>(1, hardware == 0 ? 1 : hardware - 1);
}

void collectBatch(QueryResult batch, const Query& query,
                  QueryExecutor::State& state) {
    for (std::size_t index{0}; index < batch.rows.size(); ++index) {
        if (state.skipped < query.offset) {
            ++state.skipped;
            continue;
        }
        state.result.rows.push_back(std::move(batch.rows[index]));
        state.result.rowIds.push_back(batch.rowIds[index]);
        if (query.limit && state.result.rows.size() >= *query.limit) {
            state.complete = true;
            return;
        }
    }
}

} // namespace

// Open database storage and load the persisted catalog.
DatabaseManager::DatabaseManager(const std::filesystem::path& storagePath)
    : storage_{storagePath} {
    catalog_.load(storage_);
}

// Create a table and persist its schema and storage.
void DatabaseManager::createTable(const std::string& name, const Schema& schema) {
    Catalog::validateString(name);
    if (catalog_.hasTable(name)) {
        throw DatabaseError{"table already exists: " + name};
    }
    storage_.createTable(name, schema);
    try {
        catalog_.createTable(name, schema, storage_);
    } catch (...) {
        storage_.dropTable(name);
        throw;
    }
}

// Remove a table from storage and the catalog.
void DatabaseManager::dropTable(const std::string& name) {
    if (!catalog_.hasTable(name)) {
        throw DatabaseError{"unknown table: " + name};
    }
    storage_.dropTable(name);
    catalog_.dropTable(name, storage_);
}

// Validate and append a row to the named table.
RowId DatabaseManager::insert(const std::string& tableName, const Row& row) {
    const auto& schema{catalog_.getSchema(tableName)};
    return storage_.appendRow(tableName, schema, row);
}

void DatabaseManager::update(const std::string& tableName, RowId id, const Row& row) {
    const auto& schema{catalog_.getSchema(tableName)};
    if (!storage_.updateRow(tableName, schema, id, row)) {
        throw DatabaseError{"unknown RowId " + std::to_string(id) + " in table " + tableName};
    }
}

void DatabaseManager::erase(const std::string& tableName, RowId id) {
    const auto& schema{catalog_.getSchema(tableName)};
    if (!storage_.deleteRow(tableName, schema, id)) {
        throw DatabaseError{"unknown RowId " + std::to_string(id) + " in table " + tableName};
    }
}

// Read a table and execute the requested query against its rows.
QueryResult DatabaseManager::select(const Query& query, QueryExecutionOptions options) {
    const auto& schema{catalog_.getSchema(query.table)};
    if (options.batchSize == 0) {
        throw QueryError{"query batch size must be greater than zero"};
    }
    const std::size_t workerCount{options.workerCount == 0 ? defaultWorkerCount() :
                                                            options.workerCount};
    if (workerCount == 0) {
        throw QueryError{"query worker count must be greater than zero"};
    }

    auto cursor{storage_.openTableCursor(query.table, schema)};
    auto state{executor_.begin(query, schema)};
    if (state.complete) return std::move(state.result);

    if (workerCount == 1) {
        while (!cursor.finished() && !state.complete) {
            auto page{cursor.readNextPage(options.batchSize)};
            executor_.executeBatch(query, schema, page->takeRows(), state);
        }
        return std::move(state.result);
    }

    // Avoid constructing a worker pool when the table fits in one batch.
    auto firstPage{cursor.readNextPage(options.batchSize)};
    if (cursor.finished()) {
        executor_.executeBatch(query, schema, firstPage->takeRows(), state);
        return std::move(state.result);
    }

    Query workerQuery{query};
    workerQuery.offset = 0;
    workerQuery.limit.reset();
    const std::size_t maxInFlight{options.maxInFlightBatches == 0 ?
        workerCount * 2 : options.maxInFlightBatches};
    if (maxInFlight == 0) {
        throw QueryError{"maximum in-flight query batches must be greater than zero"};
    }

    ThreadPool pool{workerCount};
    std::deque<std::future<QueryResult>> pending{};
    pending.push_back(pool.submit(
        [this, workerQuery, &schema, page = std::move(firstPage)]() mutable {
            return executor_.execute(workerQuery, schema, page->takeRows());
        }));
    while ((!cursor.finished() || !pending.empty()) && !state.complete) {
        while (!cursor.finished() && pending.size() < maxInFlight) {
            auto page{cursor.readNextPage(options.batchSize)};
            if (page->empty()) break;
            pending.push_back(pool.submit(
                [this, workerQuery, &schema, page = std::move(page)]() mutable {
                    return executor_.execute(workerQuery, schema, page->takeRows());
                }));
        }
        if (!pending.empty()) {
            collectBatch(pending.front().get(), query, state);
            pending.pop_front();
        }
    }
    return std::move(state.result);
}

// Return whether the catalog contains the named table.
bool DatabaseManager::hasTable(const std::string& name) const {
    return catalog_.hasTable(name);
}

// Return table names in sorted order.
std::vector<std::string> DatabaseManager::listTables() const {
    return catalog_.listTables();
}

// Return the named table schema or report an unknown table.
const Schema& DatabaseManager::getSchema(const std::string& tableName) const {
    return catalog_.getSchema(tableName);
}

// Read the table and return its number of stored rows.
std::size_t DatabaseManager::rowCount(const std::string& tableName) const {
    const auto& schema{catalog_.getSchema(tableName)};
    auto cursor{storage_.openTableCursor(tableName, schema)};
    std::size_t count{0};
    constexpr std::size_t batchSize{1024};
    while (!cursor.finished()) {
        count += cursor.readNextPage(batchSize)->size();
    }
    return count;
}

} // namespace vrdb
