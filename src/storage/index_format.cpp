#include "storage/index_format.hpp"

#include "db/errors.hpp"
#include "storage/serialization.hpp"

#include <charconv>

namespace vrdb {
namespace {

std::size_t number(const std::string& text) {
    std::size_t value{};
    const auto [end, error]{std::from_chars(text.data(), text.data() + text.size(), value)};
    if (error != std::errc{} || end != text.data() + text.size()) {
        throw StorageError{"invalid numeric field in vector index definition"};
    }
    return value;
}

} // namespace

void IndexFormat::write(std::ostream& output, const VectorIndexDefinition& definition) {
    writeCsvRecord(output, {"vrdb_vector_index", "1", definition.name, definition.table,
        definition.column, definition.metric == DistanceMetric::EUCLIDEAN ? "EUCLIDEAN" : "COSINE",
        std::to_string(definition.dimension), std::to_string(definition.options.M),
        std::to_string(definition.options.efConstruction), std::to_string(definition.options.efSearch),
        std::to_string(definition.options.seed)});
}

VectorIndexDefinition IndexFormat::read(std::istream& input) {
    std::vector<std::string> fields;
    if (!readCsvRecord(input, fields) || fields.size() != 11 ||
        fields[0] != "vrdb_vector_index" || fields[1] != "1") {
        throw StorageError{"invalid or unsupported vector index definition"};
    }
    VectorIndexDefinition definition;
    definition.name = fields[2];
    definition.table = fields[3];
    definition.column = fields[4];
    if (fields[5] == "EUCLIDEAN") definition.metric = DistanceMetric::EUCLIDEAN;
    else if (fields[5] == "COSINE") definition.metric = DistanceMetric::COSINE;
    else throw StorageError{"unknown vector index distance metric"};
    definition.dimension = number(fields[6]);
    definition.options = {number(fields[7]), number(fields[8]), number(fields[9]), number(fields[10])};
    if (definition.dimension == 0) throw StorageError{"zero vector index dimension"};
    try {
        validateHnswOptions(definition.options);
    } catch (const QueryError& error) {
        throw StorageError{error.what()};
    }
    if (readCsvRecord(input, fields) || input.bad()) {
        throw StorageError{"unexpected trailing data in vector index definition"};
    }
    return definition;
}

} // namespace vrdb
