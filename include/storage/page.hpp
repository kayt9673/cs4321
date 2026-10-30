#pragma once

#include "types/row.hpp"

#include <cstddef>
#include <mutex>
#include <vector>

namespace vrdb {

// A capacity-bounded, thread-safe owner of one ordered group of stored rows.
class Page {
public:
    Page(std::size_t sequence, std::size_t capacity);
    Page(std::size_t sequence, std::size_t capacity, std::vector<StoredRow> rows);

    Page(const Page&) = delete;
    Page& operator=(const Page&) = delete;
    Page(Page&&) = delete;
    Page& operator=(Page&&) = delete;

    std::size_t sequence() const noexcept;
    std::size_t capacity() const noexcept;
    std::size_t size() const;
    bool empty() const;
    bool full() const;

    // Add a row if capacity remains; return false without modifying the page otherwise.
    bool addRow(StoredRow row);
    // Copy rows while retaining ownership in this page.
    std::vector<StoredRow> snapshot() const;
    // Transfer all rows out of this page, leaving it empty and reusable.
    std::vector<StoredRow> takeRows();

private:
    const std::size_t sequence_;
    const std::size_t capacity_;
    mutable std::mutex mutex_;
    std::vector<StoredRow> rows_;
};

} // namespace vrdb
