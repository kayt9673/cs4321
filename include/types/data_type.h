#pragma once

#include <cstddef>
#include <string_view>
#include <variant>

namespace vrdb {

struct Int64Type {};

struct TextType {};

class VectorType {
public:
    explicit VectorType(std::size_t dimension);

    std::size_t dimension() const noexcept;

private:
    std::size_t dimension_;
};

using DataType = std::variant<Int64Type, TextType, VectorType>;

bool isInteger(const DataType& type) noexcept;
bool isText(const DataType& type) noexcept;
bool isVector(const DataType& type) noexcept;
std::string_view dataTypeName(const DataType& type) noexcept;

} // namespace vrdb
