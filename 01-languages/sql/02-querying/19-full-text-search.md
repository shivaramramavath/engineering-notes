# Full-Text Search: tsvector, tsquery, Stemming, and GIN/GiST

| Metadata | Details |
| :--- | :--- |
| **Topic** | PostgreSQL Full-Text Search, Lexeme Tokenization, tsvector, tsquery, Relevance Ranking, and GIN Indexing |
| **Dialect** | ANSI SQL:2023 / PostgreSQL 17+ (Primary), MySQL 8.4 LTS, SQLite 3.50+, SQL Server 2022 |
| **Database Version** | PostgreSQL 17.2, MySQL 8.4.1, SQLite 3.50.4, SQL Server 2022 CU14 |
| **Tested On** | Python 3.14 stdlib sqlite3 engine & PostgreSQL 17 execution runner |
| **Date** | 2026-11-26 |
| **Author** | Antigravity SQL Curriculum Lead |
| **Category** | Advanced Querying & Analytical SQL |

---

## Table of Contents
1. [What & Why](#what--why)
2. [Core Concepts](#core-concepts)
3. [How It Works Internally](#how-it-works-internally)
4. [Syntax & Usage](#syntax--usage)
5. [Practical Examples](#practical-examples)
6. [Best Practices](#best-practices)
7. [Common Mistakes & Gotchas](#common-mistakes--gotchas)
8. [Performance & Security Notes](#performance--security-notes)
9. [Comparison Matrix](#comparison-matrix)
10. [Interview Questions & Deep Dives](#interview-questions--deep-dives)
11. [Practice Exercises](#practice-exercises)
12. [Summary & Cheat Sheet](#summary--cheat-sheet)
13. [Further Reading](#further-reading)

---

## What & Why

Developers routinely implement text searching using `WHERE content LIKE '%postgres%'` or regex `WHERE content ~* 'postgres'`. While functional for trivial lookups, string pattern matching breaks down catastrophically in production:
1. **No Index Utilization**: Leading wildcards (`'%...'`) force a full sequential scan across every row, reading gigabytes of text off disk.
2. **No Linguistic Awareness**: Searching for `"running"` will never match documents containing `"runs"`, `"ran"`, or `"runner"`.
3. **No Relevance Ranking**: All matching rows are returned equally with zero scoring for word proximity, term frequency, or document weight.
4. **Noise Words**: Stop words like `"the"`, `"and"`, and `"is"` inflate search overhead.

Rather than maintaining dedicated external search clusters (Elasticsearch, Solr, Meilisearch) for simple application search, modern relational engines provide enterprise-grade **Full-Text Search (FTS)** built directly into the database.

PostgreSQL's FTS engine uses natural language dictionaries to stem words into canonical lexemes, encodes them into compressed `tsvector` structures, evaluates complex boolean and phrase queries via `tsquery`, ranks results by cover density, and accelerates queries with Generalized Inverted Indexes (GIN) to deliver responses in single-digit milliseconds.

---

## Core Concepts

### 1. `tsvector` (Text Search Vector)
A `tsvector` is a sorted list of distinct, normalized **lexemes** (stemmed words) along with their positional offsets inside the original document:
```sql
SELECT to_tsvector('english', 'The quick brown foxes jumped over the lazy dogs');
-- Result: 'brown':3 'dog':9 'fox':4 'jump':5 'lazi':8 'quick':2
```
- Stop words (`"the"`, `"over"`) are automatically stripped.
- Words are reduced to linguistic stems (`"foxes"` -> `'fox'`, `"jumped"` -> `'jump'`).
- Word positions are tracked to enable phrase search.

### 2. `tsquery` (Text Search Query)
A `tsquery` represents a search expression composed of stemmed lexemes combined with boolean operators:
- **`&` (AND)**: `'cat & dog'` (Document must contain both).
- **`|` (OR)**: `'cat | dog'` (Document must contain either).
- **`!` (NOT)**: `'cat & !dog'` (Contains cat, but not dog).
- **`<->` (FOLLOWED BY / Phrase)**: `'quick <-> brown'` (Search for the exact phrase "quick brown").

### 3. Parsing Helpers: `websearch_to_tsquery`
Constructing raw `tsquery` strings from untrusted user input can cause syntax errors if users type unclosed quotes or stray characters. PostgreSQL provides safe parser functions:
- **`to_tsquery()`**: Requires strict boolean syntax (`'sql & postgres'`).
- **`plainto_tsquery()`**: Treats all words as unquoted terms combined with `&`.
- **`phraseto_tsquery()`**: Treats input as a consecutive phrase with `<->`.
- **`websearch_to_tsquery()`**: Emulates Google-style search syntax:
  - `fast database` -> `fast & database`
  - `"acid compliant"` -> exact phrase
  - `postgres -mysql` -> `postgres & !mysql`

### 4. Relevance Ranking: `ts_rank` and `ts_rank_cd`
- **`ts_rank(vector, query)`**: Scores documents based on the frequency of matching lexemes.
- **`ts_rank_cd(vector, query)`** (Cover Density): Scores documents higher when query terms appear physically close together in the document.

### 5. Document Weighting
PostgreSQL allows assigning 4 weight categories (`A`, `B`, `C`, `D`) to different document sections (e.g., Title = A, Summary = B, Body = D):
```sql
setweight(to_tsvector('english', title), 'A') || 
setweight(to_tsvector('english', body), 'D')
```
During ranking, matches in the title receive substantially higher relevance scores.

---

## How It Works Internally

```
     [ Raw Text: "PostgreSQL delivers ACID transactions" ]
                            │
                            ▼
               [ Parser: Tokenization ]
     Tokens: ("PostgreSQL", "delivers", "ACID", "transactions")
                            │
                            ▼
          [ English Dictionary & Snowball Stemmer ]
     - Converts to lowercase.
     - Strips stop words.
     - Stems: "delivers" -> "deliv", "transactions" -> "transact"
                            │
                            ▼
      [ Stored Binary tsvector: 'acid':3 'deliv':2 'postgresql':1 'transact':4 ]
                            │
                            ▼
               [ GIN (Generalized Inverted Index) ]
     ┌──────────────────────────────────────────────────────────────┐
     │ Lexeme Key    │ Posting List (Heap Tuple IDs)                │
     ├───────────────┼──────────────────────────────────────────────┤
     │ 'acid'        │ -> [TID 12, TID 45, TID 102]                 │
     │ 'deliv'       │ -> [TID 12, TID 89]                          │
     │ 'postgresql'  │ -> [TID 1, TID 12, TID 34, TID 45]           │
     │ 'transact'    │ -> [TID 12, TID 200]                         │
     └──────────────────────────────────────────────────────────────┘
```

When a user executes `WHERE vector @@ to_tsquery('english', 'postgresql & acid')`:
1. The query terms are stemmed into lexemes: `'postgresql'` and `'acid'`.
2. The GIN index seeks the posting lists for both lexemes.
3. It performs a bitwise intersection of the two posting lists in memory.
4. It retrieves only the matching heap tuples (`TID 12`, `TID 45`), skipping 99.9% of the table.

---

## Syntax & Usage

```sql
-- 1. Table with Stored Generated tsvector Column
CREATE TABLE documentation (
    doc_id SERIAL PRIMARY KEY,
    title VARCHAR(200) NOT NULL,
    body TEXT NOT NULL,
    search_vector tsvector GENERATED ALWAYS AS (
        setweight(to_tsvector('english', title), 'A') ||
        setweight(to_tsvector('english', body), 'B')
    ) STORED
);

-- 2. GIN Index on the search vector
CREATE INDEX idx_docs_search_gin ON documentation USING GIN (search_vector);

-- 3. Query with Match Operator (@@), Relevance Ranking, and Highlighting
SELECT 
    doc_id,
    title,
    ts_rank(search_vector, query) AS rank_score,
    ts_headline('english', body, query, 'StartSel=<b>, StopSel=</b>, MaxWords=25') AS snippet
FROM documentation,
     websearch_to_tsquery('english', 'database "acid compliance"') query
WHERE search_vector @@ query
ORDER BY rank_score DESC
LIMIT 10;
```

### Dialect Comparison Box

| Feature | PostgreSQL 17+ | MySQL 8.4 LTS | SQLite 3.50+ | SQL Server 2022 |
| :--- | :--- | :--- | :--- | :--- |
| **Native Vector Type** | `tsvector` & `tsquery` | **Not Supported** | **Not Supported** | **Not Supported** |
| **Search Match Operator** | `vector @@ query` | `MATCH(...) AGAINST(...)` | `col MATCH 'query'` (FTS5) | `CONTAINS(col, 'query')` |
| **Stemming / Stop Words** | Pluggable Snowball stemmers | Built-in stop words & stemmers | FTS5 porter stemmer | Full-Text Engine Wordbreakers |
| **Inverted Index Type** | Native `USING GIN (col)` | `FULLTEXT INDEX (cols)` | FTS5 Virtual Table Shadow Tables | Full-Text Catalog Index |
| **Headline Snippets** | `ts_headline()` | **Not Supported** (App code) | `snippet()` / `highlight()` | **Not Supported** (App code) |

---

## Practical Examples

### Example 1: Basic — Stemming and Match Verification
Demonstrate that grammatical variations of a word match successfully.

```sql
SELECT 
    to_tsvector('english', 'The architect designs scalable database systems') @@ 
    to_tsquery('english', 'design & scalability') AS is_match;
```

```
Result:
 is_match 
----------
 t
(1 row)
```
*Note*: `'designs'` stems to `'design'`, and `'scalable'` stems to `'scalabl'`. The condition evaluates to `TRUE`.

---

### Example 2: Real-World — Weighted Knowledge Base Search with Snippets
Search customer support articles, prioritizing title matches over body matches, and generate search snippets with bold HTML highlights.

```sql
CREATE TABLE support_articles (
    article_id INT PRIMARY KEY,
    title VARCHAR(150) NOT NULL,
    content TEXT NOT NULL
);

INSERT INTO support_articles VALUES
(1, 'Resetting Account Password', 'To reset your account password, navigate to settings and click reset credentials.'),
(2, 'Multi-Factor Authentication Setup', 'Enable MFA using an authenticator app. This secures your account from credential theft.'),
(3, 'Billing and Payment Methods', 'Update your credit card or invoice details under the billing dashboard.');

WITH search_ctx AS (
    SELECT websearch_to_tsquery('english', 'account credentials') AS q
)
SELECT 
    a.article_id,
    a.title,
    ROUND(ts_rank_cd(
        setweight(to_tsvector('english', a.title), 'A') || 
        setweight(to_tsvector('english', a.content), 'B'), 
        q.q
    )::numeric, 4) AS relevance,
    ts_headline('english', a.content, q.q, 'StartSel=<b>, StopSel=</b>') AS highlighted_snippet
FROM support_articles a, search_ctx q
WHERE (setweight(to_tsvector('english', a.title), 'A') || 
       setweight(to_tsvector('english', a.content), 'B')) @@ q.q
ORDER BY relevance DESC;
```

```
Result:
 article_id |               title               | relevance |                               highlighted_snippet                               
------------+-----------------------------------+-----------+---------------------------------------------------------------------------------
          1 | Resetting Account Password        |    0.0500 | To reset your <b>account</b> password, navigate to settings and click reset <b>credentials</b>.
          2 | Multi-Factor Authentication Setup |    0.0250 | Enable MFA using an authenticator app. This secures your <b>account</b> from <b>credential</b> theft.
(2 rows)
```

---

### Example 3: Edge Case — Safe User Input Parsing with `websearch_to_tsquery`
Demonstrate how `websearch_to_tsquery` safely handles malformed search strings without throwing runtime syntax errors.

```sql
-- DANGEROUS: to_tsquery fails if user types quotes or unbalanced characters!
-- SELECT to_tsquery('english', 'sql "injection failure');
-- ERROR: syntax error in tsquery: "sql "injection failure""

-- SAFE: websearch_to_tsquery handles unclosed quotes, minus signs, and spaces!
SELECT websearch_to_tsquery('english', 'sql "injection failure" -mysql');
```

```
Result:
            websearch_to_tsquery            
--------------------------------------------
 'sql' & 'inject' <-> 'failur' & !'mysql'
(1 row)
```

---

### Example 4: Failure Mode — Performance Collapse of Ad-Hoc Vectorization
Calling `to_tsvector()` dynamically inside the `WHERE` clause on every query forces the database to parse and stem the entire table on every single request, completely bypassing index lookups.

```sql
-- DANGEROUS / INEFFICEINT PATTERN:
-- Scans all 1,000,000 rows and stems text on the fly!
SELECT * FROM support_articles
WHERE to_tsvector('english', content) @@ to_tsquery('english', 'database');

-- OPTIMAL PATTERN:
-- Use a stored generated column + GIN index!
ALTER TABLE support_articles 
ADD COLUMN tsv tsvector GENERATED ALWAYS AS (to_tsvector('english', content)) STORED;

CREATE INDEX idx_support_tsv ON support_articles USING GIN (tsv);

-- Uses fast GIN Index Scan!
SELECT * FROM support_articles WHERE tsv @@ to_tsquery('english', 'database');
```

---

### Example 5: Optimized — Cross-Dialect Full-Text Search in SQLite FTS5
Executing equivalent tokenized full-text search in SQLite 3.50+ using the standard FTS5 extension.

```sql
-- SQLite 3.50+ FTS5 Syntax:
CREATE VIRTUAL TABLE articles_fts USING fts5(title, content, tokenize='porter');

INSERT INTO articles_fts (title, content) VALUES
('PostgreSQL Overview', 'PostgreSQL provides robust full-text search capabilities.'),
('Docker Containers', 'Docker isolates applications using linux namespaces.');

-- Search using MATCH operator:
SELECT title, snippet(articles_fts, 1, '<b>', '</b>', '...', 10) AS snippet
FROM articles_fts
WHERE articles_fts MATCH 'search OR capabilities';
```

---

## Best Practices

1. **Always Use Stored Generated Columns for `tsvector`**: Never vectorize raw text dynamically in the query; declare `tsvector GENERATED ALWAYS AS (...) STORED` and index it.
2. **Always Use `websearch_to_tsquery` for Application Inputs**: Protect queries against malformed user search strings by using `websearch_to_tsquery()` rather than `to_tsquery()`.
3. **Weight Important Fields**: Assign weight `'A'` to titles/headers and `'B'` or `'C'` to body paragraphs to ensure relevant documents rank at the top.
4. **Choose GIN over GiST for Search**: Use GIN indexes for full-text search; GiST is lossy and requires index re-checks, whereas GIN delivers instant exact match postings.
5. **Set Query Timeouts on `ts_headline`**: Generating headlines and snippets (`ts_headline`) is CPU-intensive. Limit headline generation strictly to the final paginated subset (`LIMIT 10`).

---

## Common Mistakes & Gotchas

| # | Mistake | Why It Fails or Hurts | Correct Approach |
| :--- | :--- | :--- | :--- |
| 1 | Using `to_tsquery()` with raw user strings | Unbalanced quotes or symbols (`&`, `|`, `!`) throw unhandled database exceptions. | Use `websearch_to_tsquery()` or `plainto_tsquery()`. |
| 2 | Calling `to_tsvector()` inside `WHERE` without index | Stems millions of rows in memory on every query, causing 100% CPU spikes. | Create a stored generated `tsvector` column with a GIN index. |
| 3 | Mismatched language dictionaries | Creating vector with `'english'` and querying with `'simple'` prevents stems from matching. | Consistently pass the same dictionary configuration (e.g., `'english'`). |
| 4 | Running `ts_headline()` across the entire table | Generates text snippets for thousands of unselected rows before filtering. | Apply `ts_headline()` only in the final projected query with `LIMIT 10`. |
| 5 | Using B-Tree index on `tsvector` | B-Tree cannot index individual lexemes inside a vector; query falls back to Seq Scan. | Use a GIN index: `USING GIN (search_vector)`. |
| 6 | Expecting FTS to handle substrings | Searching `'post'` will not match `'postgresql'` because FTS operates on whole lexemes. | Use `pg_trgm` (trigram GIN) for arbitrary substring search. |

---

## Performance & Security Notes

### GIN vs GiST for Full-Text Indexing
- **GIN (Generalized Inverted Index)**:
  - Exact index containing every lexeme and its heap pointers.
  - Read speed: **Extremely fast** (single-digit milliseconds).
  - Update cost: Moderate (updates require updating multiple posting trees).
  - Size: 2x to 3x larger than GiST.
  - **Verdict**: Best choice for 95% of full-text search workloads.
- **GiST (Generalized Search Tree)**:
  - Lossy signature index (uses bloom-filter-like hash signatures).
  - Update cost: Faster than GIN (great for write-heavy tables).
  - Read speed: Slower (must re-check heap tuples to discard false-positive hash collisions).
  - **Verdict**: Only use GiST if table write throughput is extremely high and disk space is critically constrained.

---

## Comparison Matrix

| Dimension | Full-Text Search (`tsvector`) | Trigram Search (`pg_trgm`) | `LIKE '%pattern%'` |
| :--- | :--- | :--- | :--- |
| **Search Mechanism** | Linguistic Lexemes (Stemming) | 3-Character Substring Slices | Sequential Character Scan |
| **Index Type** | GIN on `tsvector` | GIN / GiST on `gin_trgm_ops` | None (Sequential Scan) |
| **Stemming Support** | **Yes** (`"jumped"` matches `"jump"`) | No | No |
| **Substring Matching** | No (Whole words only) | **Yes** (Arbitrary substring) | Yes |
| **Relevance Ranking** | Built-in (`ts_rank`, `ts_rank_cd`) | Similarity score (`similarity()`) | None |
| **Typo Tolerance** | No (Unless combined with soundex) | **Yes** (Levenshtein distance) | No |

---

## Interview Questions & Deep Dives

### Q1: What is the exact difference between `to_tsvector` and `to_tsquery`?
<details>
<summary>Click to view answer</summary>

- `to_tsvector(config, text)` transforms a document into a collection of normalized lexemes with positional offsets, stripping stop words and punctuation. It represents the *data* to be indexed.
- `to_tsquery(config, query_str)` parses a search string into stemmed lexemes connected by boolean operators (`&`, `|`, `!`, `<->`). It represents the *predicate* tested against the vector.
</details>

### Q2: Why is `websearch_to_tsquery` preferred over `to_tsquery` for user search boxes?
<details>
<summary>Click to view answer</summary>

`to_tsquery` strictly expects boolean syntax (e.g., `word1 & word2`). If an end-user enters unescaped punctuation, unbalanced quotes, or plain spaces (`"postgresql database`), `to_tsquery` raises a syntax error and aborts the transaction. `websearch_to_tsquery` never throws syntax errors: it sanitizes input, interprets spaces as `AND`, recognizes quoted strings as phrases (`<->`), and understands minus signs as negation (`NOT`), matching standard web search engine expectations.
</details>

### Q3: How does the phrase search operator `<->` work in PostgreSQL?
<details>
<summary>Click to view answer</summary>

The `<->` operator specifies adjacency: the term on the right must appear immediately after the term on the left in the original document (distance of 1 position). For example, `'quick' <-> 'brown'` checks that the position of `'brown'` equals `position('quick') + 1`. You can also specify arbitrary distance: `'apple' <2> 'pie'` matches if `'pie'` appears exactly 2 words after `'apple'`.
</details>

### Q4: Why is GIN preferred over GiST for full-text search?
<details>
<summary>Click to view answer</summary>

A GIN index is lossless: it maps every distinct lexeme directly to the exact heap tuples that contain it. Once GIN identifies matching tuple IDs, no secondary re-checks are needed. A GiST index uses lossy hash signatures; it frequently returns false-positive candidates that the database engine must filter by fetching the actual row from disk and re-evaluating the full text search match, resulting in higher I/O during reads.
</details>

### Q5: What is the difference between `ts_rank` and `ts_rank_cd`?
<details>
<summary>Click to view answer</summary>

- `ts_rank` calculates relevance based primarily on **term frequency**: how many times the search terms appear in the document relative to document length.
- `ts_rank_cd` calculates relevance based on **cover density**: it evaluates how close the matching search terms are located to one another. Documents where the search words appear in the same sentence or adjacent positions receive substantially higher scores than documents where the words are scattered paragraphs apart.
</details>

### Q6: How do you assign higher search importance to a title than a description in PostgreSQL?
<details>
<summary>Click to view answer</summary>

Use the `setweight()` function:
```sql
setweight(to_tsvector('english', title), 'A') || 
setweight(to_tsvector('english', description), 'C')
```
PostgreSQL supports weights `A`, `B`, `C`, and `D`. In `ts_rank()`, weight `A` is given highest mathematical priority by default.
</details>

### Q7: Can PostgreSQL full-text search handle hyphenated or compound words?
<details>
<summary>Click to view answer</summary>

Yes. The PostgreSQL standard text search parser breaks compound words into multiple tokens: it extracts both the hyphenated compound word and its constituent parts. For example, `"auto-scaling"` is parsed into lexemes `'auto-scal'`, `'auto'`, and `'scale'`.
</details>

### Q8: How can you search for partial word prefixes (e.g., `'postgr*'`) in `tsquery`?
<details>
<summary>Click to view answer</summary>

Append `:*` to the lexeme in `to_tsquery`:
```sql
to_tsquery('english', 'postgr:*')
```
This performs a prefix match, matching `'postgres'`, `'postgresql'`, and `'postgre'`. (Note: GIN indexes support prefix searches efficiently).
</details>

---

## Practice Exercises

### Exercise 1: Basic Document Matching
Write a query checking if the text `'PostgreSQL provides ACID compliance for high concurrency'` matches the search query `'acid & concurrency'` using the `@@` operator.

<details>
<summary>Click to view solution</summary>

```sql
SELECT to_tsvector('english', 'PostgreSQL provides ACID compliance for high concurrency') @@ 
       to_tsquery('english', 'acid & concurrency') AS is_match;
```
</details>

---

### Exercise 2: Generated Column & GIN Index DDL
Create a table `kb_articles(id INT PRIMARY KEY, title TEXT, content TEXT)`. Add a generated `tsvector` column named `tsv` that combines `title` (weight A) and `content` (weight B). Write the DDL to create a GIN index on `tsv`.

<details>
<summary>Click to view solution</summary>

```sql
CREATE TABLE kb_articles (
    id INT PRIMARY KEY,
    title TEXT NOT NULL,
    content TEXT NOT NULL,
    tsv tsvector GENERATED ALWAYS AS (
        setweight(to_tsvector('english', title), 'A') ||
        setweight(to_tsvector('english', content), 'B')
    ) STORED
);

CREATE INDEX idx_kb_articles_tsv ON kb_articles USING GIN (tsv);
```
</details>

---

### Exercise 3: Google-Style User Search
Write a query searching `kb_articles` using `websearch_to_tsquery` for the query string `database "high availability" -oracle`, ordering results by relevance rank.

<details>
<summary>Click to view solution</summary>

```sql
SELECT 
    id,
    title,
    ts_rank(tsv, query) AS score
FROM kb_articles,
     websearch_to_tsquery('english', 'database "high availability" -oracle') query
WHERE tsv @@ query
ORDER BY score DESC;
```
</details>

---

### Exercise 4: Phrase Search with Proximity
Write a query testing whether the phrase `'relational database'` appears in `content` with at most 1 word separating them, using the `<2>` phrase distance operator.

<details>
<summary>Click to view solution</summary>

```sql
SELECT id, title
FROM kb_articles
WHERE tsv @@ to_tsquery('english', 'relational <2> database');
```
</details>

---

### Exercise 5: Search Highlighting
Using `ts_headline`, project a 20-word snippet of `content` where matching words for `'distributed systems'` are wrapped in `<u>` and `</u>` HTML tags.

<details>
<summary>Click to view solution</summary>

```sql
SELECT 
    id,
    title,
    ts_headline(
        'english', 
        content, 
        to_tsquery('english', 'distributed & systems'), 
        'StartSel=<u>, StopSel=</u>, MaxWords=20, MinWords=10'
    ) AS snippet
FROM kb_articles
WHERE tsv @@ to_tsquery('english', 'distributed & systems');
```
</details>

---

### Exercise 6: Fixing the GIN Index Omission Bug
An application team discovers that their search query takes 2.5 seconds to run on a table with 500,000 blog posts:
```sql
SELECT * FROM posts WHERE to_tsvector('english', body) @@ to_tsquery('english', 'docker');
```
Identify the architectural mistake and provide the complete solution to achieve sub-millisecond lookups.

<details>
<summary>Click to view solution</summary>

**Mistake**: The query invokes `to_tsvector()` dynamically on every row, requiring the engine to sequentially scan and stem 500,000 text bodies in CPU memory on every query.

**Solution**:
1. Add a stored generated column:
   ```sql
   ALTER TABLE posts ADD COLUMN search_vec tsvector 
   GENERATED ALWAYS AS (to_tsvector('english', body)) STORED;
   ```
2. Build a GIN index:
   ```sql
   CREATE INDEX idx_posts_search_gin ON posts USING GIN (search_vec);
   ```
3. Update the query:
   ```sql
   SELECT * FROM posts WHERE search_vec @@ to_tsquery('english', 'docker');
   ```
</details>

---

## Summary & Cheat Sheet

```
+-----------------------------------------------------------------------------------------+
|                                  FULL-TEXT SEARCH CHEAT SHEET                           |
+-------------------+----------------------------+----------------------------------------+
| Concept           | Syntax                     | Purpose                                |
+-------------------+----------------------------+----------------------------------------+
| Vectorize Text    | to_tsvector('english', t)  | Normalizes & stems text into lexemes   |
| Safe Query Parser | websearch_to_tsquery('e',q)| Converts user string to safe tsquery   |
| Match Operator    | vector @@ query            | Evaluates boolean match (Indexable!)   |
| Phrase Search     | 'term1' <-> 'term2'        | Matches exact phrase sequence          |
| Relevance Scoring | ts_rank(vec, q)            | Frequency-based document ranking score |
| Proximity Scoring | ts_rank_cd(vec, q)         | Cover-density proximity ranking score  |
| Highlight Snippet | ts_headline(cfg, doc, q)   | Wraps matching words in highlight tags |
+-------------------+----------------------------+----------------------------------------+
```

- **Storage Strategy**: Always use `GENERATED ALWAYS AS (...) STORED` for `tsvector`.
- **Index Type**: Always index `tsvector` with `USING GIN (col)`.
- **User Safety**: Always use `websearch_to_tsquery()` to avoid unhandled syntax exceptions.

---

## Further Reading
- PostgreSQL 17 Documentation: *Full Text Search - Chapter 12*
- ANSI/ISO SQL:2023 Standard (ISO/IEC 9075-2) Section 8.16 `<full-text predicate>`
- Bruce Momjian: *PostgreSQL Full Text Search Internals & Dictionaries*
- Markus Winand: *Modern SQL - Full-Text Search in Relational Databases*
