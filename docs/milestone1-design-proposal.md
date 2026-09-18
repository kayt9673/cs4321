# **Milestone 1 Design**

## **Codebase Structure**

The database is organized into the following modules:

| include/ ├── db/ │   ├── database.h │   └── catalog.h │ ├── types/ │   ├── value.h │   ├── column.h │   ├── schema.h │   └── row.h │ ├── storage/ │   ├── storage\_engine.h │   ├── file\_storage\_engine.h │   └── serialization.h │ ├── query/ │   ├── query.h │   ├── predicate.h │   ├── query\_result.h │   └── executor.h │ └── vector/     └── distance.h  |
| :---- |

| src/ ├── db/ │   ├── database.cpp │   └── catalog.cpp │ ├── types/ │   ├── schema.cpp │   └── row.cpp │ ├── storage/ │   ├── file\_storage\_engine.cpp │   └── serialization.cpp │ ├── query/ │   ├── predicate.cpp │   └── executor.cpp │ └── vector/     └── distance.cpp |
| :---- |

# **Types**

## **ColumnType / Value**

`include/types/value.h`

| /\*\* Describes the type declared in a schema. \*/ enum class ColumnType {     INTEGER,     TEXT,     VECTOR }; using Vector \= std::vector\<float\>; /\*\* Represents an actual value stored in a row. \*/ using Value \= std::variant\<     int64\_t,      std::string,     Vector \>; |
| :---- |

Adding a database type requires extending both `ColumnType` and `Value`.

**Supported Representations:**

| Column Type  | C++ Representation |
| ----- | ----- |
| ColumnType::INTEGER | int64\_t |
| ColumnType::TEXT  | std::string |
| ColumnType::VECTOR | std::vector\<float\> |

**Column**

`include/types/schema.h`

| struct Column {     std::string name;     ColumnType type;     // Only populated for VECTOR columns; must be \>= 1\.     std::optional\<std::size\_t\> vectorDimension;  }; |
| :---- |

**Examples:** Initializing columns

| Column Type | Code |
| ----- | ----- |
| `INTEGER` | Column{"id", ColumnType::INTEGER, std::nullopt} |
| `TEXT` | Column{"title", ColumnType::TEXT, std::nullopt} |
| `VECTOR` | Column{"embedding", ColumnType::VECTOR, 384} |

**Examples:** Invalid columns

| Property | Reasoning |
| ----- | ----- |
| name.empty() | Column name cannot be empty |
| type \== VECTOR && \!vectorDimension.has\_value() | Vector data must have a specified dimension |
| type \== VECTOR &&  vectorDimension \== 0 | Vector dimension must be positive |
| type \!= VECTOR && vectorDimension.has\_value() | Vector dimension must have a specified value |

# 

# **Schema**

`include/types/schema.h`

| /\*\*  \* Represents the ordered column structure of a table.  \*/ class Schema { public:     // Creates a schema from an ordered list of columns.     explicit Schema(std::vector\<Column\> columns);      // Returns all columns in schema order.     const std::vector\<Column\>& columns() const;          // Returns all columns in schema order.     std::size\_t size() const;     // Returns whether a column with the given name exists.     bool hasColumn(const std::string& name) const;     // Returns the index of the named column (see below).     std::size\_t columnIndex(         const std::string& name     ) const;     // Returns the named column.     const Column& column(         const std::string& name     ) const;     // Returns the column at the given index.     const Column& column(         std::size\_t index     ) const;     // Verifies that the schema and its columns are valid (see below).     void validate() const; private:    // Columns in their defined schema order.     std::vector\<Column\> columns\_; }; |
| :---- |

**Example:** Initializing a `Schema`

| Schema({     {"id", ColumnType::INTEGER, std::nullopt},     {"title", ColumnType::TEXT, std::nullopt},     {"embedding", ColumnType::VECTOR, 384} }); |
| :---- |

**Additional Specifications:** 

`columnIndex()` is the common lookup mechanism used by predicate execution and projection.

`Schema::validate()` enforces:

- columns.size() \> 0  
- For every column, column.name \!= “” and the column is valid   
- For every pair of columns, column names are unique   
- Column order is preserved

Unknown columns result in an error rather than returning an invalid index.

# 

# **Row**

`include/types/row.h`

| /\*\*  \* Represents a single row of values in a table.  \*/ class Row { public:     // Creates a row from an ordered list of values.     explicit Row(std::vector\<Value\> values);     // Returns all values in the row.     const std::vector\<Value\>& values() const;     // Returns the value at the given index.     const Value& value(         std::size\_t index     ) const;     // Returns the number of values in the row.     std::size\_t size() const;     // Verifies that the row matches the given schema (see below).      void validateAgainst(         const Schema& schema     ) const; private:     // Values stored in schema order.     std::vector\<Value\> values\_; }; |
| :---- |

**Example:** 

| Schema  | `Row` Representation |
| ----- | ----- |
| id          INTEGER title       TEXT embedding   VECTOR(3) | {     int64\_t{42},     std::string{"Example"},     Vector{0.1f, 0.2f, 0.3f} } |

**Additional Specifications:** 

`validateAgainst(schema)` checks:

- row.size \== schema.size  
- All column types are correct, i.e.  
  - INTEGER → int64\_t  
  - TEXT    → std::string  
  - VECTOR  → Vector  
- For vector columns: value.size \== column.vectorDimension

# 

# **Catalog**

`include/db/catalog.h`

| /\*\*   \* Stores metadata describing a table.   \*/  struct TableMetadata {     std::string name;     Schema schema; }; /\*\*   \* Manages metadata for all tables in the database.   \*   \* Provides table creation, deletion, lookup, and persistent   \* loading of table schemas.   \*/  class Catalog { public:     // Creates a catalog for the database at the given path.     explicit Catalog(         std::filesystem::path databasePath     );     // Loads existing table metadata from persistent storage.     void load();     // Adds a new table and its schema to the catalog.     void createTable(         const std::string& name,         const Schema& schema     );     // Removes a table from the catalog.     void dropTable(         const std::string& name     );     // Returns whether the named table exists.     bool hasTable(        const std::string& name     ) const;     // Returns the schema for the named table.     const Schema& getSchema(         const std::string& name     ) const;     // Returns the names of all tables in the catalog.     std::vector\<std::string\> listTables() const; private:     // Path used to persist catalog metadata.     std::filesystem::path path\_;     // In-memory metadata indexed by table name.     std::unordered\_map\<         std::string,          TableMetadata     \> tables\_; }; |
| :---- |

**Example:** Intended behavior

Database: 

| my\_database/ ├── catalog.vrdb └── tables/     ├── products.vrdb     └── users.vrdb |
| :---- |

`catalog.vrdb` should contain sufficient information to reconstruct both the `products` and `users` schemas.

**Additional Specifications:** 

`Catalog::load()` is called when the database opens.

- Invariant: catalog contains table ⇔ database recognizes table  
- The existence of `products.vrdb` alone does not define a table.

**StorageEngine**

`include/storage/storage_engine.h`

The storage layer is responsible for persistent data, not schema validation or query execution.

| /\*\*  \* Defines the interface for persistent table storage.  \*/ class StorageEngine { public:     // Virtual destructor for storage engine implementations.     virtual \~StorageEngine() \= default;      // Creates persistent storage for a new table.     virtual void createTable(         const std::string& tableName     ) \= 0;      // Appends a validated row to the table.     virtual void appendRow(         const std::string& tableName,         const Schema& schema,         const Row& row     ) \= 0;      // Reads and returns all rows from the table.     virtual std::vector\<Row\> readRows(         const std::string& tableName,         const Schema& schema     ) \= 0;      // Deletes the table's persistent storage.     virtual void dropTable(         const std::string& tableName     ) \= 0; }; |
| :---- |

`FileStorageEngine` implements this interface.

| /\*\*  \* File-based implementation of the StorageEngine.  \* Stores table data as files within the database directory.  \*/ class FileStorageEngine : public StorageEngine { public:     // Creates a file storage engine using the given root directory.     explicit FileStorageEngine(         std::filesystem::path root     );     // Creates the file used to store a table.     void createTable(...) override;     // Appends a row to the table's file.     void appendRow(...) override;     // Reads all rows from the table's file.     std::vector\<Row\> readRows(...) override;     // Deletes the table's file.     void dropTable(...) override; private:     // Root directory containing table files.     std::filesystem::path root\_; }; |
| :---- |

# 

# **Row Serialization**

`include/storage/serialization.h`

Serialization is isolated from `StorageEngine`, so that file I/O and value encoding are separate responsibilities.

A malformed or schema-incompatible row is considered an error.

| // Converts a database value into its persistent string representation. std::string serializeValue(     const Value& value ); // Converts a serialized value back into its expected column type. Value deserializeValue(     const std::string& encoded,     const Column& column );  // Converts a complete row into its persistent string representation. std::string serializeRow(     const Row& row ); // Reconstructs a row using its serialized data and table schema. Row deserializeRow(     const std::string& encoded,     const Schema& schema ); |
| :---- |

# 

# **Query**

`include/query/query.h`

| /\*\*  \* Represents a query to execute against a table.  \*/ struct Query {     // Name of the table being queried.     std::string table;     // Columns to return. Empty means all columns (see below).     std::vector\<std::string\> projection;     // Conditions that rows must satisfy.     std::vector\<Predicate\> predicates;     // Maximum number of rows to return.     std::optional\<std::size\_t\> limit;     // Number of matching rows to skip before returning results.     std::size\_t offset \= 0; }; |
| :---- |

**Additional Specifications:** 

Projections: 

| Description | Code | Equivalent Query |
| ----- | ----- | ----- |
| Empty projection | projection \= {} | SELECT \* |
| Non-empty projection | projection \= {     "title",     "year" }; | SELECT title, year |

Predicates are evaluated with `AND` semantics.

# 

# **Predicate**

`include/query/predicate.h`

## **ComparisonOperator**

| enum class ComparisonOperator {     EQUAL,     NOT\_EQUAL,     LESS\_THAN,     LESS\_THAN\_OR\_EQUAL,     GREATER\_THAN,     GREATER\_THAN\_OR\_EQUAL }; |
| :---- |

## **IntegerPredicate**

| struct IntegerPredicate {     std::string column;     ComparisonOperator op;     int64\_t value; }; |
| :---- |

**Example:** Initializing an `IntegerPredicate`

| IntegerPredicate{     "year",     ComparisonOperator::GREATER\_THAN\_OR\_EQUAL,     2020 }; |
| :---- |

## **TextPredicate**

| struct TextPredicate {     std::string column;     ComparisonOperator op; // Only EQUAL and NOT\_EQUAL are valid     std::string value; }; |
| :---- |

## 

## **DistanceMetric**

| enum class DistanceMetric {     EUCLIDEAN,     COSINE }; |
| :---- |

## **VectorPredicate**

| struct VectorPredicate {     std::string column;     DistanceMetric metric;     std::vector\<float\> referenceVector;     ComparisonOperator op; // Must be LESS\_THAN or LESS\_THAN\_OR\_EQUAL     float threshold; }; |
| :---- |

**Example:** Initializing a `VectorPredicate`

| Code | Equivalent Query |
| ----- | ----- |
| VectorPredicate{     "embedding",     DistanceMetric::COSINE,     queryEmbedding,     ComparisonOperator::LESS\_THAN,     0.2f }; | COSINE\_DISTANCE(     embedding,     queryEmbedding ) \< 0.2  |

## **Predicate**

Execution can dispatch based on the predicate type without manually storing a predicate “type” field.

| using Predicate \= std::variant\<     IntegerPredicate,     TextPredicate,     VectorPredicate \>; |
| :---- |

# **QueryResult**

`include/query/query_result.h`

| /\*\*  \* Represents the result returned from a query.  \*/ struct QueryResult {     // Schema describing the columns in the returned rows.     Schema schema;     // Rows that matched the query.     std::vector\<Row\> rows; }; |
| :---- |

**Example:** 

| Schema | Projection | `QueryResult` |
| ----- | ----- | ----- |
| id          INTEGER title       TEXT year        INTEGER embedding   VECTOR(384) | projection \= \["title", "year"\] | QueryResult │ ├── Schema │   ├── title TEXT │   └── year INTEGER │ └── Rows     ├── \[...\]     ├── \[...\]     └── \[...\] |

# 

# **Distance Functions**

`include/vector/distance.h`

| float euclideanDistance(     const std::vector\<float\>& a,     const std::vector\<float\>& b ); float cosineDistance(     const std::vector\<float\>& a,     const std::vector\<float\>& b ); float distance(     const std::vector\<float\>& a,     const std::vector\<float\>& b,     DistanceMetric metric ); |
| :---- |

**Requirements:**

- Both operands must have equal dimensions.  
- For cosine distance, a zero vector is invalid   
  - However, a zero vector remains valid **stored data**. The error occurs only when an operation requiring a nonzero norm is performed.

# 

# **QueryExecutor**

`include/query/executor.h`

| /\*\*  \* Executes queries against rows using the provided table schema.  \*/ class QueryExecutor { public:     // Executes a query and returns the matching projected rows.     QueryResult execute(         const Query& query,         const Schema& schema,         const std::vector\<Row\>& rows     ) const; private:     // Checks whether a row satisfies a predicate.     bool evaluatePredicate(         const Predicate& predicate,         const Schema& schema,         const Row& row     ) const;     // Returns a row containing only the requested columns.     Row projectRow(         const Row& row,         const Schema& schema,         const std::vector\<std::string\>& projection     ) const;     // Creates the schema corresponding to the requested columns.     Schema projectSchema(         const Schema& schema,         const std::vector\<std::string\>& projection     ) const; }; |
| :---- |

**Query Execution Order:**

1) Validate the query against the table's schema.  
2) Scan each row sequentially.  
3) Evaluate all predicates against the current row.  
- If any predicate is false, skip the row.  
- If all predicates are true, continue processing the row.  
4) Apply the query offset.  
- Skip matching rows until the offset has been reached.  
5) Apply the projection to select only the requested columns.  
6) Add the projected row to the query result.  
7) Check the query limit.  
- If the limit has been reached, return the result.  
- Otherwise, continue scanning rows.  
8) Return the result after all rows have been scanned.

# 

# **Database**

`include/db/database.h`

| /\*\*  \* Main interface for interacting with the database.  \*/ class Database { public:     // Opens or creates a database at the given path.     explicit Database(         const std::filesystem::path& path     );     // Creates a new table with the given schema.     void createTable(         const std::string& name,         const Schema& schema     );     // Removes a table from the database.     void dropTable(         const std::string& name     );     // Inserts a row into the specified table.     void insert(         const std::string& tableName,         const Row& row     );     // Executes a query and returns its results.     QueryResult select(         const Query& query     );     // Returns whether the named table exists.     bool hasTable(         const std::string& name     ) const;     // Returns the names of all tables in the database.     std::vector\<std::string\> listTables() const;     // Returns the schema for the specified table.     const Schema& getSchema(         const std::string& tableName     ) const;     // Returns the number of rows in the specified table.     std::size\_t rowCount(         const std::string& tableName     ) const; private:     // Stores table names and schemas.     Catalog catalog\_;     // Handles persistent table and row storage.     std::unique\_ptr\<StorageEngine\> storage\_;     // Executes queries against stored rows.     QueryExecutor executor\_; }; |
| :---- |

**Example:** Using `Database`

| // Open or create the database at the given directory. Database db("./my\_database"); // Create a table with integer, text, and vector columns. db.createTable(     "documents",     Schema({         {"id", ColumnType::INTEGER},         {"text", ColumnType::TEXT},         {"embedding", ColumnType::VECTOR, 384}     }) ); // Insert a row that matches the \`documents\` schema. db.insert(     "documents",     Row({         int64\_t{1},         std::string{"vector databases"},         embedding     }) ); // Create a query against the \`documents\` table. Query query; query.table \= "documents"; // Only return rows where id \> 0 AND // cosine distance from the reference vector is \< 0.3. query.predicates \= {     IntegerPredicate{         "id",         ComparisonOperator::GREATER\_THAN,         0     },     VectorPredicate{         "embedding",         DistanceMetric::COSINE,         reference,         ComparisonOperator::LESS\_THAN,         0.3f     } }; // Execute the query and store the matching rows. QueryResult result \= db.select(query); |
| :---- |

# 

# **Database Startup**

Creating the `Database` instance below performs the following initialization steps:

| Database db("./my\_database"); |
| :---- |

1) **Verify the database directory:** Create `./my_database` if it does not already exist.  
2) **Initialize the storage engine:** Set up the `FileStorageEngine` for reading and writing table data.  
3) **Initialize the catalog:** Set up the `Catalog` used to manage table metadata.  
4) **Load existing metadata:** Call `Catalog::load()` to restore previously created table names and schemas.

**Example:** Valid flow 

| // First session: create the database, table, and row. {     Database db("./data");     db.createTable("documents", schema);     db.insert("documents", row); } // Database closes here. // Second session: reopen the same database. Database db("./data");  db.hasTable("documents");   // true db.getSchema("documents");  // returns the original schema db.rowCount("documents");   // returns 1 |
| :---- |

# 

# **Insert Execution**

`Database::insert()` handles the entire process of inserting a row:

1) Check that the table exists using `Catalog::hasTable()`.  
2) Retrieve the table's schema using `Catalog::getSchema()`.  
3) Validate the row against the schema using `Row::validateAgainst()`.  
4) Persist the row using `StorageEngine::appendRow()`.

Rows are written directly to persistent storage rather than being maintained in both memory and storage. This avoids inconsistencies between in-memory and persisted data and establishes the storage engine as the single source of truth for table rows.

# **Select Execution**

`Database::select()` handles the complete query execution process:

1) Retrieve the table's schema using `Catalog::getSchema()`.  
2) Read the table's persisted rows using `StorageEngine::readRows()`.  
3) Pass the query, schema, and rows to `QueryExecutor::execute()`.  
4) Return the resulting `QueryResult`.

This ensures that queries are executed against the persisted table data, rather than a separate in-memory copy.

# 

# **Error Handling**

The repository uses exceptions using the following hierarchy: 

| /\*\*  \* Base exception type for database-related errors.  \*/ class DatabaseError     : public std::runtime\_error {}; /\*\*  \* Thrown when a schema or row violates schema requirements.  \*/ class SchemaError     : public DatabaseError {}; /\*\*  \* Thrown when an error occurs while reading or writing    \* persistent data.  \*/ class StorageError     : public DatabaseError {}; /\*\*  \* Thrown when a query is invalid or cannot be executed.  \*/ class QueryError     : public DatabaseError {}; |
| :---- |

**Examples:** `SchemaError`

| SchemaError:     duplicate column 'id' SchemaError:     VECTOR column 'embedding' requires a dimension SchemaError:     expected VECTOR(384) for column 'embedding',     received VECTOR(768) |
| :---- |

**Example:** `StorageError`

| StorageError:     unable to open table file 'documents.vrdb' |
| :---- |

**Examples:** `QueryError`

| QueryError:     unknown column 'rating' QueryError:     operator '\<' is not supported for TEXT |
| :---- |

# 

# **Component Dependencies**

The codebase follows this dependency hierarchy:

| Database ├── Catalog │   └── Schema ├── StorageEngine │   └── Row └── QueryExecutor     ├── Predicate     └── Vector Distance Schema, Row, and Predicate └── Types |
| :---- |

The `Database` acts as the top-level component and coordinates the catalog, storage engine, and query executor.

**Dependency Rules:**

- `types/` is the lowest-level module and should not depend on `database/`, `storage/`, or `query/`.  
- `vector/` contains standalone vector operations and should not depend on `Database` or `StorageEngine`.  
- `Catalog` manages schemas and table metadata.  
- `StorageEngine` manages persisted rows.  
- `QueryExecutor` handles predicates and uses the vector distance functions when evaluating vector conditions.

This structure keeps the core components independent and helps prevent circular dependencies.

# **Out of Scope for Milestone 1**

SQL parser  
JOIN  
GROUP BY  
aggregation  
ORDER BY  
transactions  
WAL  
concurrency control  
NULL  
foreign keys  
primary keys  
full-text search  
vector ANN indexing  
relational indexing

