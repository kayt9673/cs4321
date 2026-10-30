#include "storage/page.hpp"

#include <stdexcept>
#include <utility>

namespace vrdb {

Page::Page(std::size_t sequence, std::size_t capacity)
    : Page{sequence, capacity, {}} {}

Page::Page(std::size_t sequence, std::size_t capacity, std::vector<StoredRow> rows)
    : sequence_{sequence}, capacity_{capacity}, rows_{std::move(rows)} {
    if (capacity_ == 0) {
        throw std::invalid_argument{"page capacity must be greater than zero"};
    }
    if (rows_.size() > capacity_) {
        throw std::invalid_argument{"initial rows exceed page capacity"};
    }
    rows_.reserve(capacity_);
}

std::size_t Page::sequence() const noexcept { return sequence_; }
std::size_t Page::capacity() const noexcept { return capacity_; }

std::size_t Page::size() const {
    std::lock_guard lock{mutex_};
    return rows_.size();
}

bool Page::empty() const {
    std::lock_guard lock{mutex_};
    return rows_.empty();
}

bool Page::full() const {
    std::lock_guard lock{mutex_};
    return rows_.size() == capacity_;
}

bool Page::addRow(StoredRow row) {
    std::lock_guard lock{mutex_};
    if (rows_.size() == capacity_) return false;
    rows_.push_back(std::move(row));
    return true;
}

std::vector<StoredRow> Page::snapshot() const {
    std::lock_guard lock{mutex_};
    return rows_;
}

std::vector<StoredRow> Page::takeRows() {
    std::lock_guard lock{mutex_};
    std::vector<StoredRow> rows{};
    rows.swap(rows_);
    rows_.reserve(capacity_);
    return rows;
}

} // namespace vrdb
