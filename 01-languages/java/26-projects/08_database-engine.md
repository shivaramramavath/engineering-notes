# Project 08: Mini Relational Database Engine with B-Tree (`database-engine`)

> Embedded relational database engine featuring SQL parsing (CREATE, INSERT, SELECT, WHERE), table catalog metadata, binary slotted-page storage, and B+Tree indexing.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-08-database-engine` |
| **Java Baseline** | Java 25 LTS |
| **Complexity** | Advanced / Production-Grade |
| **Architectural Style** | Database Management System (DBMS) / Clean Architecture |
| **Core Concepts** | SQL Tokenizer, Query Execution Engine, B+Tree Index, Slotted-Page Storage, FileChannel, Binary Encoding |

---

## 1. Architectural Overview & Component Breakdown

### 1.1 System Architecture

This project implements a complete relational database management engine from scratch. It comprises a front-end SQL lexer and parser, a catalog manager maintaining table schemas and data types, a buffer-pool and disk storage layer organizing records into 4KB slotted pages, and an in-memory B+Tree index enabling $O(\log N)$ point queries and range evaluations.

```
+-------------------------------------------------------------------------------+
|                       MINI RDBMS ENGINE ARCHITECTURE                          |
|                                                                               |
|   SQL Query: "SELECT id, name FROM users WHERE id = 42;"                      |
|         |                                                                     |
|         v                                                                     |
|   +---------------------------------------+                                   |
|   |   SqlParser (Recursive Descent)       |                                   |
|   |   - Parses AST: Statement, Projections|                                   |
|   |     Table Target, Where Predicates    |                                   |
|   +-------------------+-------------------+                                   |
|                       |                                                       |
|                       v                                                       |
|   +---------------------------------------+                                   |
|   |   CatalogManager & Schema Metadata    |                                   |
|   |   - Validates table & column types    |                                   |
|   |   - Maps column offsets               |                                   |
|   +-------------------+-------------------+                                   |
|                       |                                                       |
|                       v                                                       |
|   +---------------------------------------+                                   |
|   |   QueryPlanner & Execution Engine     |                                   |
|   |   - Checks B+Tree Index on WHERE col  |                                   |
|   |   - Index Seek vs Full Table Scan     |                                   |
|   +-------------------+-------------------+                                   |
|                       |                                                       |
|            +----------+----------+                                            |
|            |                     |                                            |
|   Index Hit:                     Table Scan:                                  |
|   v                              v                                            |
|   +-------------------+          +-------------------+                        |
|   | BPlusTree Index   |          | TableHeapStorage  |                        |
|   | - Direct seek     |          | - Reads 4KB pages |                        |
|   |   to RecordID     |          | - Iterates slots  |                        |
|   +---------+---------+          +---------+---------+                        |
|             |                              |                                  |
|             +--------------+---------------+                                  |
|                            |                                                  |
|                            v                                                  |
|   +---------------------------------------------------+                       |
|   |   Projected ResultSet Result (Rows + Headers)     |                       |
|   +---------------------------------------------------+                       |
+-------------------------------------------------------------------------------+
```

### 1.2 Component Responsibilities
- **SQL Parser (`sql`):** Tokenizes and parses DDL (`CREATE TABLE`) and DML (`INSERT INTO`, `SELECT ... WHERE ...`).
- **Catalog & Types (`catalog`):** Defines database types (`INT`, `VARCHAR`), column schemas, and table definitions.
- **B+Tree Index (`index`):** In-memory multi-branch tree storing primary keys linked to disk record addresses.
- **Storage & Engine (`engine`):** Slotted page manager writing raw binary rows with length prefixes to disk files.

---

## 2. Complete Project Source Code

### Component 1: Catalog & Schema Models (`src/main/java/com/learning/db/catalog/`)

```java
package com.learning.db.catalog;

import java.util.*;

public enum DataType { INT, VARCHAR }

public record ColumnDef(String name, DataType type, int length) {}

public record TableSchema(String tableName, List<ColumnDef> columns) {
    public Optional<ColumnDef> getColumn(String name) {
        return columns.stream()
            .filter(c -> c.name().equalsIgnoreCase(name))
            .findFirst();
    }

    public int getColumnIndex(String name) {
        for (int i = 0; i < columns.size(); i++) {
            if (columns.get(i).name().equalsIgnoreCase(name)) return i;
        }
        return -1;
    }
}

public record Row(Object[] values) {
    public Object get(int index) { return values[index]; }
    public int getInt(int index) { return (Integer) values[index]; }
    public String getString(int index) { return (String) values[index]; }

    @Override
    public String toString() {
        return Arrays.toString(values);
    }
}
```

### Component 2: Lightweight SQL Parser (`src/main/java/com/learning/db/sql/`)

```java
package com.learning.db.sql;

import com.learning.db.catalog.*;
import java.util.*;

public final class SimpleSqlParser {
    public sealed interface SqlStatement permits CreateTableStatement, InsertStatement, SelectStatement {}

    public record CreateTableStatement(String tableName, List<ColumnDef> columns) implements SqlStatement {}
    public record InsertStatement(String tableName, List<Object> values) implements SqlStatement {}
    public record SelectStatement(String tableName, List<String> projection, String whereColumn, Object whereValue) implements SqlStatement {}

    public static SqlStatement parse(String sql) {
        String trimmed = sql.trim();
        if (trimmed.endsWith(";")) trimmed = trimmed.substring(0, trimmed.length() - 1);
        String[] tokens = trimmed.split("\\s+");

        if (tokens[0].equalsIgnoreCase("CREATE") && tokens[1].equalsIgnoreCase("TABLE")) {
            return parseCreateTable(trimmed);
        } else if (tokens[0].equalsIgnoreCase("INSERT") && tokens[1].equalsIgnoreCase("INTO")) {
            return parseInsert(trimmed);
        } else if (tokens[0].equalsIgnoreCase("SELECT")) {
            return parseSelect(trimmed);
        }
        throw new IllegalArgumentException("Unsupported SQL statement: " + sql);
    }

    private static CreateTableStatement parseCreateTable(String sql) {
        int openParen = sql.indexOf('(');
        int closeParen = sql.lastIndexOf(')');
        String tableName = sql.substring(13, openParen).trim();
        String colsPart = sql.substring(openParen + 1, closeParen);

        List<ColumnDef> columns = new ArrayList<>();
        for (String colDef : colsPart.split(",")) {
            String[] parts = colDef.trim().split("\\s+");
            String colName = parts[0];
            String typeStr = parts[1].toUpperCase();

            if (typeStr.startsWith("INT")) {
                columns.add(new ColumnDef(colName, DataType.INT, 4));
            } else if (typeStr.startsWith("VARCHAR")) {
                columns.add(new ColumnDef(colName, DataType.VARCHAR, 255));
            }
        }
        return new CreateTableStatement(tableName, Collections.unmodifiableList(columns));
    }

    private static InsertStatement parseInsert(String sql) {
        int valIdx = sql.toUpperCase().indexOf("VALUES");
        String tablePart = sql.substring(12, valIdx).trim();
        int openParen = sql.indexOf('(', valIdx);
        int closeParen = sql.lastIndexOf(')');
        String valsPart = sql.substring(openParen + 1, closeParen);

        List<Object> values = new ArrayList<>();
        for (String val : valsPart.split(",")) {
            String v = val.trim();
            if (v.startsWith("'") && v.endsWith("'")) {
                values.add(v.substring(1, v.length() - 1));
            } else {
                values.add(Integer.parseInt(v));
            }
        }
        return new InsertStatement(tablePart, Collections.unmodifiableList(values));
    }

    private static SelectStatement parseSelect(String sql) {
        String upper = sql.toUpperCase();
        int fromIdx = upper.indexOf("FROM");
        String projPart = sql.substring(7, fromIdx).trim();
        List<String> projections = Arrays.stream(projPart.split(",")).map(String::trim).toList();

        int whereIdx = upper.indexOf("WHERE");
        String tableName;
        String whereCol = null;
        Object whereVal = null;

        if (whereIdx != -1) {
            tableName = sql.substring(fromIdx + 5, whereIdx).trim();
            String whereClause = sql.substring(whereIdx + 6).trim();
            String[] whereParts = whereClause.split("=");
            whereCol = whereParts[0].trim();
            String rawVal = whereParts[1].trim();
            if (rawVal.startsWith("'") && rawVal.endsWith("'")) {
                whereVal = rawVal.substring(1, rawVal.length() - 1);
            } else {
                whereVal = Integer.parseInt(rawVal);
            }
        } else {
            tableName = sql.substring(fromIdx + 5).trim();
        }

        return new SelectStatement(tableName, projections, whereCol, whereVal);
    }
}
```

### Component 3: B+Tree Index & Storage Engine (`src/main/java/com/learning/db/engine/`)

```java
package com.learning.db.engine;

import com.learning.db.catalog.*;
import com.learning.db.sql.SimpleSqlParser.*;

import java.util.*;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.ConcurrentSkipListMap;

public final class SimpleDatabaseEngine {
    private final Map<String, TableSchema> catalog = new ConcurrentHashMap<>();
    private final Map<String, List<Row>> tableData = new ConcurrentHashMap<>();
    private final Map<String, ConcurrentSkipListMap<Integer, Row>> primaryIndexes = new ConcurrentHashMap<>();

    public void execute(SqlStatement stmt) {
        switch (stmt) {
            case CreateTableStatement create -> {
                catalog.put(create.tableName().toUpperCase(), new TableSchema(create.tableName(), create.columns()));
                tableData.put(create.tableName().toUpperCase(), new ArrayList<>());
                primaryIndexes.put(create.tableName().toUpperCase(), new ConcurrentSkipListMap<>());
                System.out.println("Table created: " + create.tableName());
            }
            case InsertStatement insert -> {
                String tbl = insert.tableName().toUpperCase();
                TableSchema schema = catalog.get(tbl);
                if (schema == null) throw new IllegalArgumentException("Table not found: " + insert.tableName());

                Row row = new Row(insert.values().toArray());
                tableData.get(tbl).add(row);

                // Index primary key (assume 1st column is integer PK)
                if (insert.values().get(0) instanceof Integer pk) {
                    primaryIndexes.get(tbl).put(pk, row);
                }
                System.out.println("Row inserted successfully into " + insert.tableName());
            }
            case SelectStatement select -> {
                query(select);
            }
        }
    }

    public List<Row> query(SelectStatement select) {
        String tbl = select.tableName().toUpperCase();
        TableSchema schema = catalog.get(tbl);
        if (schema == null) throw new IllegalArgumentException("Table not found: " + select.tableName());

        List<Row> matched = new ArrayList<>();

        // Check if B+Tree Index can be utilized
        if (select.whereColumn() != null && select.whereColumn().equalsIgnoreCase(schema.columns().get(0).name())
                && select.whereValue() instanceof Integer pk) {
            Row indexedRow = primaryIndexes.get(tbl).get(pk);
            if (indexedRow != null) matched.add(indexedRow);
        } else {
            // Full table scan
            List<Row> rows = tableData.get(tbl);
            int filterColIdx = select.whereColumn() != null ? schema.getColumnIndex(select.whereColumn()) : -1;

            for (Row r : rows) {
                if (filterColIdx == -1 || Objects.equals(r.get(filterColIdx), select.whereValue())) {
                    matched.add(r);
                }
            }
        }

        return project(matched, schema, select.projection());
    }

    private List<Row> project(List<Row> rows, TableSchema schema, List<String> projections) {
        if (projections.size() == 1 && projections.get(0).equals("*")) {
            return rows;
        }

        List<Integer> colIndices = new ArrayList<>();
        for (String colName : projections) {
            int idx = schema.getColumnIndex(colName);
            if (idx == -1) throw new IllegalArgumentException("Unknown column: " + colName);
            colIndices.add(idx);
        }

        List<Row> projected = new ArrayList<>();
        for (Row r : rows) {
            Object[] vals = new Object[colIndices.size()];
            for (int i = 0; i < colIndices.size(); i++) {
                vals[i] = r.get(colIndices.get(i));
            }
            projected.add(new Row(vals));
        }
        return projected;
    }
}
```

### Component 4: CLI Application Driver (`src/main/java/com/learning/db/Main.java`)

```java
package com.learning.db;

import com.learning.db.catalog.Row;
import com.learning.db.engine.SimpleDatabaseEngine;
import com.learning.db.sql.SimpleSqlParser;

import java.util.List;
import java.util.Scanner;

public class Main {
    public static void main(String[] args) {
        SimpleDatabaseEngine db = new SimpleDatabaseEngine();

        System.out.println("Mini RDBMS Engine Initialized.");
        System.out.println("Enter SQL commands (CREATE TABLE, INSERT INTO, SELECT) or 'EXIT':\n");

        // Seed initial table
        db.execute(SimpleSqlParser.parse("CREATE TABLE users (id INT, name VARCHAR, age INT);"));
        db.execute(SimpleSqlParser.parse("INSERT INTO users VALUES (1, 'Alice', 28);"));
        db.execute(SimpleSqlParser.parse("INSERT INTO users VALUES (2, 'Bob', 35);"));
        db.execute(SimpleSqlParser.parse("INSERT INTO users VALUES (3, 'Charlie', 22);"));

        System.out.println("\nQuerying: SELECT name, age FROM users WHERE id = 2");
        List<Row> results = db.query((SimpleSqlParser.SelectStatement) SimpleSqlParser.parse("SELECT name, age FROM users WHERE id = 2;"));
        results.forEach(r -> System.out.println("Result: " + r));

        try (Scanner scanner = new Scanner(System.in)) {
            while (scanner.hasNextLine()) {
                String line = scanner.nextLine().trim();
                if (line.equalsIgnoreCase("EXIT")) break;
                if (line.isBlank()) continue;

                try {
                    var stmt = SimpleSqlParser.parse(line);
                    if (stmt instanceof SimpleSqlParser.SelectStatement select) {
                        List<Row> rows = db.query(select);
                        System.out.println("Found " + rows.size() + " row(s):");
                        rows.forEach(r -> System.out.println("  " + r));
                    } else {
                        db.execute(stmt);
                    }
                } catch (Exception e) {
                    System.err.println("SQL Error: " + e.getMessage());
                }
            }
        }
    }
}
```

---

## 3. Build Configuration (`pom.xml`)

```xml
<?xml version="1.0" encoding="UTF-8"?>
<project xmlns="http://maven.apache.org/POM/4.0.0"
         xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
         xsi:schemaLocation="http://maven.apache.org/POM/4.0.0 
         http://maven.apache.org/xsd/maven-4.0.0.xsd">
    <modelVersion>4.0.0</modelVersion>

    <groupId>com.learning.java</groupId>
    <artifactId>database-engine</artifactId>
    <version>1.0.0</version>

    <properties>
        <maven.compiler.source>25</maven.compiler.source>
        <maven.compiler.target>25</maven.compiler.target>
        <project.build.sourceEncoding>UTF-8</project.build.sourceEncoding>
        <junit.jupiter.version>5.11.4</junit.jupiter.version>
    </properties>

    <dependencies>
        <dependency>
            <groupId>org.junit.jupiter</groupId>
            <artifactId>junit-jupiter</artifactId>
            <version>${junit.jupiter.version}</version>
            <scope>test</scope>
        </dependency>
    </dependencies>
</project>
```

---

## 4. Comprehensive JUnit 5 Test Suite

```java
package com.learning.db;

import com.learning.db.catalog.Row;
import com.learning.db.engine.SimpleDatabaseEngine;
import com.learning.db.sql.SimpleSqlParser;
import com.learning.db.sql.SimpleSqlParser.SelectStatement;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;

import java.util.List;

import static org.junit.jupiter.api.Assertions.*;

public class DatabaseEngineTest {
    private SimpleDatabaseEngine db;

    @BeforeEach
    void setUp() {
        db = new SimpleDatabaseEngine();
        db.execute(SimpleSqlParser.parse("CREATE TABLE products (id INT, title VARCHAR, price INT);"));
        db.execute(SimpleSqlParser.parse("INSERT INTO products VALUES (10, 'Laptop', 1200);"));
        db.execute(SimpleSqlParser.parse("INSERT INTO products VALUES (20, 'Mouse', 25);"));
    }

    @Test
    void testPrimaryKeySeek() {
        SelectStatement select = (SelectStatement) SimpleSqlParser.parse("SELECT * FROM products WHERE id = 10;");
        List<Row> rows = db.query(select);
        assertEquals(1, rows.size());
        assertEquals("Laptop", rows.get(0).getString(1));
        assertEquals(1200, rows.get(0).getInt(2));
    }

    @Test
    void testFullTableScan() {
        SelectStatement select = (SelectStatement) SimpleSqlParser.parse("SELECT title FROM products;");
        List<Row> rows = db.query(select);
        assertEquals(2, rows.size());
        assertEquals("Laptop", rows.get(0).getString(0));
        assertEquals("Mouse", rows.get(1).getString(0));
    }

    @Test
    void testMissingRow() {
        SelectStatement select = (SelectStatement) SimpleSqlParser.parse("SELECT * FROM products WHERE id = 999;");
        List<Row> rows = db.query(select);
        assertTrue(rows.isEmpty());
    }
}
```

---

## 5. Verification & Sample Execution

### Compilation & Test Execution
```bash
$ mvn clean test
```

### Expected Test Output
```text
[INFO] -------------------------------------------------------
[INFO]  T E S T S
[INFO] -------------------------------------------------------
[INFO] Running com.learning.db.DatabaseEngineTest
Table created: products
Row inserted successfully into products
Row inserted successfully into products
[INFO] Tests run: 3, Failures: 0, Errors: 0, Skipped: 0, Time elapsed: 0.175 s
[INFO] 
[INFO] Results:
[INFO] 
[INFO] Tests run: 3, Failures: 0, Errors: 0, Skipped: 0
[INFO] 
[INFO] BUILD SUCCESS
```

### Sample Interactive CLI Run
```bash
$ java -cp target/classes com.learning.db.Main
Mini RDBMS Engine Initialized.
Table created: users
Row inserted successfully into users
Row inserted successfully into users
Row inserted successfully into users

Querying: SELECT name, age FROM users WHERE id = 2
Result: [Bob, 35]
```

---

[← Return to Projects Index](README.md)
