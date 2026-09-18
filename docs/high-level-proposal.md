# **C++ Vector-Relational Database**

## **Summary & Motivation**

This project implements a small vector-relational database management system in C++.

Traditional relational databases organize data into typed tables and support queries over relational attributes. Vector databases additionally store high-dimensional vectors and support similarity search using distance metrics.

The goal of this project is to support both models within the same database. Users can define arbitrary tables containing traditional relational columns alongside vector columns and execute queries combining relational predicates with vector similarity predicates.

The initial implementation prioritizes correctness and sequential execution. Indexing and query optimization are subsequent extensions.

# **Behavioral Description**

At completion, the database should support the following end-to-end workflow.

1. A user creates or opens a database at a specified location.  
2. The database loads its existing table schemas and metadata.  
3. The user may create arbitrary tables.  
4. Each table consists of a user-defined ordered collection of typed columns.  
5. Columns initially support `INTEGER`, `TEXT`, and `VECTOR(n)`.  
6. The user inserts rows conforming to the table's schema.  
7. Rows are persisted to disk.  
8. The user constructs a query against a table.  
9. Queries may combine relational predicates and vector-distance predicates.  
10. The database executes the query and returns matching rows.  
11. Database metadata and rows remain available after the database process is restarted.  
12. Initial query execution uses sequential scans.  
13. Later versions may introduce relational and vector indexes without changing the logical table model.

# 

# **Data Model**

## **Database**

A `Database` contains zero or more user-created tables.

| Database │ ├── Table ├── Table ├── Table └── ... |
| :---- |

**Requirements:**

- Table names must be unique within a database.  
- The database maintains enough persistent metadata to reconstruct its tables and schemas after restart.

## **Table**

A `Table` represents a named relation.

| Table │ ├── Name ├── Schema └── Rows |
| :---- |

**Requirements:**

- A table may contain any combination of supported column types.   
- Vector columns are **not** stored in separate vector tables.  
- A table can have zero, one, or multiple vector columns.

## 

## **Schema**

A `Schema` is the ordered definition of the columns belonging to a table.

| Schema ┌──────────────┬─────────────┐ │ Column Name  │ Column Type │ ├──────────────┼─────────────┤ │ id           │ INTEGER     │ │ title        │ TEXT        │ │ body         │ TEXT        │ │ embedding    │ VECTOR(384) │ └──────────────┴─────────────┘ |
| :---- |

**Requirements:**

- Column order is significant because each position in a row corresponds to the column at the same position in the schema.  
- A schema must contain at least one column.   
- Column names must be unique within the schema.

## **Column**

**Requirements:** A column has:

| Field | Definition |
| ----- | ----- |
| `name` | User-defined name identifying the column |
| `type` | Data type of values stored in the column |
| `type parameters` | Additional information required by the selected type |

Supported column types for Milestone 1:

| Column Type │ ├── INTEGER ├── TEXT └── VECTOR(n) |
| :---- |

## **Supported Types**

The following is written in order of implementation order (i.e. `Integer` and `Vector(n)` functionality should be implemented first, then Text).

### **`INTEGER`**

| Property | Definition |
| ----- | ----- |
| Representation | Signed 64-bit integer |
| Equality | `=`, `!=` |
| Ordering | `<`, `<=`, `>`, `>=` |
| Example | `42` |

### **`VECTOR(n)`**

A parameterized type where `n` is the dimension. Different vector columns may have different dimensions.

| Property | Definition |
| ----- | ----- |
| Representation | Fixed-dimensional sequence of 32-bit floats |
| Dimension | Defined by schema |
| Equality/ordering | Not initially supported |
| Distance | Euclidean, Cosine |
| Example | `[0.14, -0.22, 0.81]` |

### **`TEXT`**

Text must support empty strings and characters that have special meaning in the underlying storage format.

| Property | Definition |
| ----- | ----- |
| Representation | Variable-length string |
| Equality | `=`, `!=` |
| Ordering | Not initially supported |
| Example | `"database systems"` |

## **Row**

A `Row` is an ordered collection of values conforming to a table's schema.

**Example:** Valid and invalid rows 

Schema:

| products id          INTEGER name        TEXT embedding   VECTOR(3) |
| :---- |

Valid Row: `42 | "keyboard" | [0.12, -0.41, 0.83]`

Invalid Rows:

| Row | Reason |
| ----- | ----- |
| `42 | "keyboard"` | The value for the vector column is missing |
| `"42" | "keyboard" | [0.12, -0.41, 0.83]` | The first value has the wrong type |
| `42 | "keyboard" | [0.12, -0.41]` | The vector dimension does not match the schema |

# 

# **Query Model**

A query operates against one table and contains:

| Query │ ├── Table ├── Projection ├── Predicates ├── Limit └── Offset |
| :---- |

## **Relational Predicates**

| Column Type | Supported Predicates |
| ----- | ----- |
| `INTEGER` | `=, !=, <, <=, >, >=` |
| `TEXT` | `=, !=` |

## **Vector Predicates**

A vector predicate consists of:

| Vector Predicate │ ├── Vector Column ├── Reference Vector ├── Distance Metric ├── Comparison Operator └── Threshold |
| :---- |

Supported distance metrics for Milestone 1:

- Euclidean distance  
- Cosine distance 

**Example:** Query for a vector predicate 

| DISTANCE(     embedding,     \[0.12, 0.43, ..., \-0.17\],     COSINE ) \< 0.25 |
| :---- |

## **Combined Predicates**

A query can express both vector and relational predicates:

| year \>= 2020 AND category \= "science" AND COSINE\_DISTANCE(embedding, query\_vector) \< 0.2 |
| :---- |

# 

# **Persistence Model**

The final system should persist both:

| Database │ ├── Metadata │   ├── Table names │   └── Table schemas │ └── Data     └── Table rows |
| :---- |

**Example:** 

Sequence of queries:

1) Create database  
2) Create table  
3) Insert 1,000 rows  
4) Close program  
5) Restart program

After these queries, the database should:

- Know that the table exists  
- Know its schema  
- Be able to retrieve/query its rows

# 

# **Future Goals (Milestone 2\)**

## **Indexing**

| Table   │   ├── Relational Index   └── Vector Index |
| :---- |

## **Joins**

TBD

## **SQL Parser**

C++ SQL Parser: Highrise Project   
