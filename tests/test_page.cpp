#include "storage/page.hpp"

#include <atomic>
#include <cassert>
#include <cstdint>
#include <stdexcept>
#include <thread>
#include <vector>

int main() {
    using namespace vrdb;

    bool rejected{false};
    try {
        Page invalid{0, 0};
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);

    constexpr std::size_t capacity{100};
    Page page{7, capacity};
    std::atomic<std::size_t> appended{0};
    std::vector<std::jthread> writers{};
    for (std::size_t worker{0}; worker < 4; ++worker) {
        writers.emplace_back([&, worker] {
            for (std::size_t index{0}; index < capacity; ++index) {
                const auto id{static_cast<RowId>(worker * capacity + index + 1)};
                if (page.addRow(StoredRow{id, Row{{static_cast<std::int64_t>(id)}}})) {
                    ++appended;
                }
            }
        });
    }
    writers.clear(); // jthread destruction joins all writers.

    assert(page.sequence() == 7);
    assert(page.capacity() == capacity);
    assert(page.size() == capacity);
    assert(page.full());
    assert(appended == capacity);
    assert(page.snapshot().size() == capacity);

    auto rows{page.takeRows()};
    assert(rows.size() == capacity);
    assert(page.empty());
    assert(page.addRow(StoredRow{999, Row{{std::int64_t{999}}}}));
}
