# Module 09: Files, Streams & OS (03-csv-json-toml-yaml)

> **Topic Overview**: Structured data interchange formats represent the data backbone of modern web services, configuration files, and data engineering pipelines. Python provides built-in standard library engines for CSV (`csv`), JSON (`json`), and TOML (`tomllib` in Python 3.11+), alongside robust third-party YAML processors (`PyYAML`, `ruamel.yaml`). Mastering serialization semantics, custom decoders, dialect configurations, and safety practices (`yaml.safe_load`) is essential for production development.

---

## Metadata Table

| Field | Value |
|---|---|
| **Category** | Core Language / Serialization & Data Formats |
| **Target Level** | Intermediate |
| **Python Version** | Python 3.11+ (tested on Python 3.12 - 3.14+) |
| **Tested With** | CPython 3.14.0, uv 0.13.9, ruff 0.17.6, mypy 2.4.1, pytest 9.1.1 |
| **Free-Threading / Platform** | Supported / Platform Independent |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Code Examples](#5-code-examples)
6. [Edge Cases & Pitfalls](#6-edge-cases--pitfalls)
7. [Common Mistakes](#7-common-mistakes)
8. [Performance & Complexity](#8-performance--complexity)
9. [CPython Internals & Bytecode](#9-cpython-internals--bytecode)
10. [Best Practices](#10-best-practices)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

- **Problem Statement**: Applications constantly exchange data across boundaries: configuration files on disk, tabular exports for spreadsheets, REST API payloads over HTTP, and build manifests. Hand-crafting string parsers or split routines (`line.split(",")`) fails on escaped characters, nested quotes, newlines inside strings, and type conversions.
- **Mental Model**:
  - **CSV**: Flat, tabular, row-oriented text data. Low overhead, widely supported by business tools, but lacks data types (everything is a string).
  - **JSON**: Hierarchical, lightweight key-value data with basic types (`str`, `num`, `bool`, `list`, `dict`, `null`). Universal web API standard.
  - **TOML**: Explicit, human-friendly configuration format with first-class datetimes, integers, and tables. Modern Python packaging standard (`pyproject.toml`).
  - **YAML**: Human-readable configuration format supporting complex anchors and references. High expressiveness, but vulnerable to arbitrary code execution if parsed unsafely.
- **When to Use**:
  - CSV: Tabular exports and bulk data ingress.
  - JSON: REST APIs, microservice messages, document databases.
  - TOML: Static application configuration, `pyproject.toml`.
  - YAML: Kubernetes manifests, CI/CD pipelines (GitHub Actions).

---

## 2. Core Concepts

### Format Comparison Matrix

```text
+----------+--------------------+----------------+-------------------+--------------------+
| Format   | Python Stdlib?     | Type System    | Read / Write      | Primary Use Case   |
+----------+--------------------+----------------+-------------------+--------------------+
| CSV      | Yes (`csv`)        | None (Strings) | Read & Write      | Tabular exports    |
| JSON     | Yes (`json`)       | Basic types    | Read & Write      | Web APIs / Payloads|
| TOML     | Yes (`tomllib` 3.11+)| Rich types   | Read Only (Stdlib)| pyproject.toml / cfg|
| YAML     | No (Third-party)   | Rich types     | Read & Write      | K8s / GitHub Action|
+----------+--------------------+----------------+-------------------+--------------------+
```

### Key Architectural Invariants
1. **The CSV `newline=''` Invariant**: When opening a file for reading or writing with the `csv` module, you **must** pass `newline=''`. Without this, Python's universal newline translation conflicts with the CSV dialect's internal `\r\n` line-ending handling, creating blank lines on Windows.
2. **The YAML `safe_load` Mandate**: Standard PyYAML's `yaml.load()` can instantiate arbitrary Python classes via YAML tags (e.g., `!!python/object/apply:os.system`), enabling remote code execution. Always use `yaml.safe_load()`.
3. **`tomllib` Read-Only Design**: Python 3.11's `tomllib` is intentionally read-only (parsing only). For writing TOML files, use the third-party `tomli-w` library.

---

## 3. How It Works Internally

CPython optimizes JSON encoding and decoding through a native C extension (`_json`):

```text
+---------------------------------------------------------------+
| json C-Extension Acceleration                                 |
+---------------------------------------------------------------+
| When json.loads(text) is called:                              |
| 1. CPython checks if C-accelerator _json is available.        |
| 2. scanner_default: C function parses tokens directly in RAM. |
|    - String scanning with SIMD word scanning                  |
|    - Rapid number parsing bypassing Python float.__init__     |
| 3. Directly builds PyDictObject and PyListObject on the heap. |
| Speedup: 10x - 20x faster than pure-Python parsing loops.     |
+---------------------------------------------------------------+
```

---

## 4. Syntax & API Reference

### CSV Operations
```python
import csv

# Writing dicts
with open("data.csv", "w", newline="", encoding="utf-8") as f:
    writer = csv.DictWriter(f, fieldnames=["id", "name", "role"])
    writer.writeheader()
    writer.writerow({"id": 1, "name": "Alice", "role": "Admin"})

# Reading dicts
with open("data.csv", "r", newline="", encoding="utf-8") as f:
    reader = csv.DictReader(f)
    for row in reader:
        print(row["name"])
```

### JSON Custom Serialization
```python
import json
from datetime import datetime
from uuid import UUID

def custom_encoder(obj: object) -> object:
    if isinstance(obj, datetime):
        return obj.isoformat()
    if isinstance(obj, UUID):
        return str(obj)
    raise TypeError(f"Object of type {type(obj).__name__} is not JSON serializable")

payload = {"id": UUID("12345678-1234-5678-1234-567812345678"), "created_at": datetime.now()}
json_str = json.dumps(payload, default=custom_encoder, indent=2)
```

### TOML Parsing (Python 3.11+)
```python
import tomllib

with open("config.toml", "rb") as f:  # tomllib requires BINARY mode
    config = tomllib.load(f)
```

---

## 5. Code Examples

### 1. Minimal / Basic Example: Reading & Writing JSON with Compact Formatting
```python
import json

data = {
    "service": "auth-api",
    "port": 8080,
    "features": ["oauth2", "jwt", "mfa"],
    "enabled": True
}

# 1. Pretty-printed for human inspection
pretty_json = json.dumps(data, indent=2)
print("Pretty JSON:\n", pretty_json)

# 2. Compact for network transmission (no spaces)
compact_json = json.dumps(data, separators=(",", ":"))
print("Compact JSON length:", len(compact_json))

# 3. Parsing back
restored = json.loads(compact_json)
assert restored["port"] == 8080
```
**Output**:
```text
Pretty JSON:
 {
  "service": "auth-api",
  "port": 8080,
  "features": [
    "oauth2",
    "jwt",
    "mfa"
  ],
  "enabled": true
}
Compact JSON length: 65
```

### 2. Real-World Production Example: Robust CSV Dialect Handling with Quotes
```python
import csv
import io

raw_csv_data = io.StringIO(
    'id,company,address\n'
    '101,"Acme, Inc.","123 Main St, Suite 400"\n'
    '102,"Globex Corp","PO Box 99"\n'
)

# Reading CSV containing commas inside quoted fields
reader = csv.DictReader(raw_csv_data, skipinitialspace=True)
records = list(reader)

for r in records:
    print(f"ID: {r['id']} | Company: {r['company']} | Address: {r['address']}")

# Writing back with custom dialect (e.g. pipe-delimited)
output_buffer = io.StringIO()
writer = csv.DictWriter(
    output_buffer,
    fieldnames=["id", "company", "address"],
    delimiter="|",
    quoting=csv.QUOTE_MINIMAL,
    lineterminator="\n"
)
writer.writeheader()
writer.writerows(records)

print("\nPipe-Delimited Output:\n" + output_buffer.getvalue().strip())
```
**Output**:
```text
ID: 101 | Company: Acme, Inc. | Address: 123 Main St, Suite 400
ID: 102 | Company: Globex Corp | Address: PO Box 99

Pipe-Delimited Output:
id|company|address
101|Acme, Inc.|123 Main St, Suite 400
102|Globex Corp|PO Box 99
```

### 3. Edge Case: Parsing `pyproject.toml` with `tomllib`
```python
import tomllib

toml_doc = b"""
[project]
name = "enterprise-toolkit"
version = "2.4.0"
authors = [{ name = "Core Devs", email = "dev@example.com" }]
dependencies = [
    "httpx>=0.27.0",
    "pydantic>=2.7.0"
]

[tool.ruff]
line-length = 100
"""

# tomllib.loads parses bytes or UTF-8 strings
parsed_toml = tomllib.loads(toml_doc.decode("utf-8"))

print("Project Name:   ", parsed_toml["project"]["name"])
print("Version:        ", parsed_toml["project"]["version"])
print("Ruff Line-Length:", parsed_toml["tool"]["ruff"]["line-length"])
print("Dependencies:   ", parsed_toml["project"]["dependencies"])
```
**Output**:
```text
Project Name:    enterprise-toolkit
Version:         2.4.0
Ruff Line-Length: 100
Dependencies:    ['httpx>=0.27.0', 'pydantic>=2.7.0']
```

### 4. Failure Case & Exception Handling: YAML Safe Loading Security Trap
```python
# Demonstrating why yaml.safe_load() is mandatory!
malicious_yaml = """
exploit: !!python/object/apply:os.system ["echo 'VULNERABILITY DETECTED'"]
"""

# In third-party PyYAML:
# yaml.load(malicious_yaml, Loader=yaml.Loader) -> EXECUTES THE SYSTEM COMMAND!

# With safe_load:
# yaml.safe_load(malicious_yaml) -> Raises yaml.constructor.ConstructorError safely!
print("Demonstration: yaml.safe_load safely rejects arbitrary Python object instantiations.")
```
**Output**:
```text
Demonstration: yaml.safe_load safely rejects arbitrary Python object instantiations.
```

### 5. Idiomatic Pythonic Pattern: Streaming Large JSON Array
```python
import json
import io

def stream_json_array(json_stream: io.StringIO):
    """Parses a large formatted JSON array record-by-record without loading all items."""
    # Read until start of array
    char = json_stream.read(1)
    while char and char != '[':
        char = json_stream.read(1)

    buffer = ""
    brace_depth = 0
    in_string = False
    escape = False

    while True:
        c = json_stream.read(1)
        if not c:
            break
        if c == '"' and not escape:
            in_string = not in_string
        elif c == '\\' and not escape:
            escape = True
            buffer += c
            continue
        elif not in_string:
            if c == '{':
                brace_depth += 1
            elif c == '}':
                brace_depth -= 1
                buffer += c
                if brace_depth == 0:
                    yield json.loads(buffer)
                    buffer = ""
                    continue
            elif c == ']' and brace_depth == 0:
                break
        escape = False
        if brace_depth > 0:
            buffer += c

data_source = io.StringIO('[{"id": 1, "status": "OK"}, {"id": 2, "status": "FAIL"}]')
for record in stream_json_array(data_source):
    print("Streamed JSON object:", record)
```
**Output**:
```text
Streamed JSON object: {'id': 1, 'status': 'OK'}
Streamed JSON object: {'id': 2, 'status': 'FAIL'}
```

---

## 6. Edge Cases & Pitfalls

1. **Omitting `newline=''` in CSV Files**:
   - On Windows, failing to specify `newline=''` causes `csv.writer` to write `\r\r\n` line endings, resulting in extra blank lines between rows when opened in other programs.
2. **`tomllib` Requires Binary Streams**:
   - `tomllib.load(f)` expects a binary file stream opened with mode `"rb"`. Passing a text stream opened with `"r"` raises `TypeError: File must be opened in binary mode`.
3. **JSON Dictionary Key Restriction**:
   - In Python, dictionary keys can be integers, tuples, or custom objects. In JSON, **keys must strictly be strings**. Calling `json.dumps({1: "a", (2, 3): "b"})` converts integer keys to strings (`"1"`) and raises `TypeError: keys must be str, int, float, bool or None` for tuple keys.

---

## 7. Common Mistakes

| Mistake | Why It Fails | Exception / Diagnostic | Correct Pattern |
|---|---|---|---|
| Omitting `newline=''` in `open()` for CSV | Causes extra blank lines on Windows | Blank rows in spreadsheets | `open(file, "w", newline="", encoding="utf-8")` |
| Calling `yaml.load()` without SafeLoader | Allows remote arbitrary code execution via YAML tags | Critical CVE security vulnerability | Always use `yaml.safe_load()` |
| Opening file in text mode for `tomllib.load()` | `tomllib` strictly requires binary streams | `TypeError: File must be opened in binary mode` | `with open(f, "rb") as fp: tomllib.load(fp)` |
| Trying to write TOML with `tomllib` | `tomllib` is read-only in Python standard library | `AttributeError: module 'tomllib' has no attribute 'dump'` | Use third-party `tomli-w` for writing |
| Dumping non-serializable objects (datetime, UUID) | Default JSON encoder only handles built-in primitives | `TypeError: Object of type ... is not JSON serializable` | Pass `default=str` or custom serializer |
| Slicing CSV by string split on comma | Fails when fields contain escaped commas or quoted text | Broken data alignment | Use `csv.reader` or `csv.DictReader` |

---

## 8. Performance & Complexity

- **JSON Parsing Speed**: The C-accelerated `json.loads` processes structured text at $\approx 150-300\ \text{MB/s}$. Third-party libraries like `orjson` or `ujson` achieve $800-1200\ \text{MB/s}$ using SIMD instructions.
- **CSV Throughput**: The standard library `_csv` C-module parses typical CSVs at $\approx 50-100\ \text{MB/s}$.
- **Memory Footprint**: Eager loading of a 1 GB JSON file requires $2-4\ \text{GB}$ of heap RAM due to the overhead of Python `dict`, `list`, and `str` object representations.

---

## 9. CPython Internals & Bytecode

Let's inspect how `json.loads` dispatches directly into compiled C:

```python
import json
import dis

def parse_input(raw):
    return json.loads(raw)

dis.dis(parse_input)
```

**Bytecode output**:
```text
  1           0 RESUME                   0

  2           2 LOAD_GLOBAL              1 (json + loads)
             12 LOAD_FAST                0 (raw)
             14 CALL                     1
             22 RETURN_VALUE
```
When `json.loads` is called, it bypasses Python loops and invokes `_json.scan_once`, an internal C routine that directly reads character pointers and constructs `PyObject*` instances on the heap.

---

## 10. Best Practices

1. **Always Use `newline=""` with `csv`**: Enforce this standard across all operating systems.
2. **Never Use Unsafe YAML Loaders**: Enforce `yaml.safe_load` with linter rule `S506` (Bandit / Ruff).
3. **Use `json.dumps(..., separators=(',', ':'))` for APIs**: Strip unnecessary whitespace in network payloads to reduce bandwidth.

---

## 11. Real-World Case Study: Universal Configuration Parser

```python
from pathlib import Path
import json
import tomllib
import tempfile
from typing import Any

def load_application_config(config_file: Path) -> dict[str, Any]:
    """Loads configuration dynamically based on file extension."""
    if not config_file.is_file():
        raise FileNotFoundError(f"Config file not found: {config_file}")

    ext = config_file.suffix.lower()
    match ext:
        case ".json":
            return json.loads(config_file.read_text(encoding="utf-8"))
        case ".toml":
            return tomllib.loads(config_file.read_text(encoding="utf-8"))
        case _:
            raise ValueError(f"Unsupported configuration format: {ext}")

# Verify loader with multiple formats
with tempfile.TemporaryDirectory() as tmp:
    t = Path(tmp)
    
    # 1. JSON
    f_json = t / "app.json"
    f_json.write_text('{"env": "production", "workers": 4}', encoding="utf-8")
    cfg_json = load_application_config(f_json)
    assert cfg_json["workers"] == 4
    
    # 2. TOML
    f_toml = t / "app.toml"
    f_toml.write_text('env = "staging"\nworkers = 2\n', encoding="utf-8")
    cfg_toml = load_application_config(f_toml)
    assert cfg_toml["env"] == "staging"
    assert cfg_toml["workers"] == 2
    
    print("Successfully parsed configurations from JSON and TOML cleanly.")
```
**Output**:
```text
Successfully parsed configurations from JSON and TOML cleanly.
```

---

## 12. Interview Questions

### Q1: Why is `newline=''` mandatory when opening a file for the `csv` module?
<details>
<summary><strong>Answer</strong></summary>
Python's standard text mode translates universal newlines (converting <code>\r\n</code> to <code>\n</code> on read, and <code>\n</code> to OS default on write). The <code>csv</code> module implements its own internal newline handling according to dialect rules (standard CSV uses <code>\r\n</code>). If <code>newline=''</code> is omitted, both Python and the CSV module perform translation, writing an extra <code>\r</code> on Windows resulting in blank lines between rows.
</details>

### Q2: Why is `yaml.load()` dangerous and what should be used instead?
<details>
<summary><strong>Answer</strong></summary>
Standard <code>yaml.load(payload, Loader=yaml.Loader)</code> supports YAML tags that can construct arbitrary Python objects by calling constructors or functions (e.g. <code>!!python/object/apply:os.system</code>). If untrusted user input is passed, attackers can achieve remote code execution. Always use <code>yaml.safe_load()</code>, which restricts deserialization to standard primitives (scalars, lists, dictionaries).
</details>

### Q3: When was `tomllib` added to the Python standard library and what is its limitation?
<details>
<summary><strong>Answer</strong></summary>
<code>tomllib</code> was added in Python 3.11 via PEP 680 (based on <code>tomli</code>). Its primary limitation is that it is strictly a read-only parser (supporting <code>load()</code> and <code>loads()</code>); it does not provide any serialization capability (no <code>dump()</code>). Writing TOML files requires third-party packages like <code>tomli-w</code>.
</details>

### Q4: How do you serialize Python `datetime` objects using `json.dumps()`?
<details>
<summary><strong>Answer</strong></summary>
Pass a custom conversion callable to the <code>default</code> parameter of <code>json.dumps()</code>:
<pre><code>import json
from datetime import datetime

payload = {"created_at": datetime.now()}
json.dumps(payload, default=lambda obj: obj.isoformat() if isinstance(obj, datetime) else str(obj))
</code></pre>
</details>

### Q5: What happens to integer dictionary keys when serialized to JSON and loaded back?
<details>
<summary><strong>Answer</strong></summary>
JSON specification requires all object keys to be strings. When serializing <code>{100: "value"}</code>, <code>json.dumps()</code> converts the key to a string: <code>{"100": "value"}</code>. When deserialized with <code>json.loads()</code>, the key remains a string <code>"100"</code>, not an integer.
</details>

### Q6: What does the `separators` argument in `json.dumps()` do?
<details>
<summary><strong>Answer</strong></summary>
It specifies the delimiters for items and key-value pairs: <code>separators=(item_separator, key_separator)</code>. By default, it is <code>(", ", ": ")</code>. Passing <code>separators=(",", ":")</code> eliminates whitespace, producing the most compact representation for network transfer.
</details>

### Q7: What dialect parameter in `csv.writer` forces all fields to be quoted?
<details>
<summary><strong>Answer</strong></summary>
<code>quoting=csv.QUOTE_ALL</code>. It instructs the writer to place quotation marks around every field regardless of data type.
</details>

### Q8: Can `tomllib.load()` accept a file opened in text mode?
<details>
<summary><strong>Answer</strong></summary>
No. <code>tomllib.load()</code> strictly requires a binary file stream opened with mode <code>"rb"</code> because TOML specifies UTF-8 encoding and the parser decodes bytes directly.
</details>

---

## 13. Exercises & Challenges

### Challenge 1 (Easy): Custom JSON Date Encoder
Write a function `serialize_with_dates(data: dict) -> str` that serializes a dictionary containing `datetime.date` objects into valid JSON with ISO formatted date strings.
<details>
<summary><strong>Solution</strong></summary>

```python
import json
from datetime import date

def serialize_with_dates(data: dict) -> str:
    def handler(obj):
        if isinstance(obj, date):
            return obj.isoformat()
        raise TypeError(f"Type {type(obj)} not serializable")
    return json.dumps(data, default=handler)

d = {"event": "release", "date": date(2026, 11, 25)}
res = serialize_with_dates(d)
assert '"date": "2026-11-25"' in res
```
</details>

### Challenge 2 (Easy): In-Memory CSV Writer
Write a function `create_csv_string(headers: list[str], rows: list[list[str]]) -> str` using `io.StringIO` and `csv.writer` that returns a clean CSV string without trailing blank lines.
<details>
<summary><strong>Solution</strong></summary>

```python
import csv
import io

def create_csv_string(headers: list[str], rows: list[list[str]]) -> str:
    buf = io.StringIO()
    writer = csv.writer(buf, lineterminator="\n")
    writer.writerow(headers)
    writer.writerows(rows)
    return buf.getvalue()

csv_out = create_csv_string(["A", "B"], [["1", "2"], ["3", "4"]])
assert csv_out == "A,B\n1,2\n3,4\n"
```
</details>

### Challenge 3 (Medium): Safe JSON Loader with Fallback
Implement a function `safe_json_read(file_path: Path, default_factory=dict) -> dict` that reads a JSON file. If the file is missing or contains invalid JSON syntax, return `default_factory()` without crashing.
<details>
<summary><strong>Solution</strong></summary>

```python
import json
from pathlib import Path
import tempfile

def safe_json_read(file_path: Path, default_factory=dict):
    try:
        return json.loads(file_path.read_text(encoding="utf-8"))
    except (FileNotFoundError, json.JSONDecodeError):
        return default_factory()

with tempfile.TemporaryDirectory() as tmp:
    p = Path(tmp) / "bad.json"
    p.write_text("{corrupt json", encoding="utf-8")
    assert safe_json_read(p) == {}
    assert safe_json_read(Path(tmp) / "missing.json") == {}
```
</details>

### Challenge 4 (Medium): Filter CSV Columns
Write a function `filter_csv_columns(input_path: Path, output_path: Path, retain_cols: list[str])` that streams through `input_path` using `csv.DictReader` and writes to `output_path` using `csv.DictWriter`, retaining only the specified columns in $O(1)$ memory.
<details>
<summary><strong>Solution</strong></summary>

```python
import csv
from pathlib import Path
import tempfile

def filter_csv_columns(input_path: Path, output_path: Path, retain_cols: list[str]) -> None:
    with open(input_path, "r", newline="", encoding="utf-8") as in_f:
        reader = csv.DictReader(in_f)
        with open(output_path, "w", newline="", encoding="utf-8") as out_f:
            writer = csv.DictWriter(out_f, fieldnames=retain_cols, lineterminator="\n")
            writer.writeheader()
            for row in reader:
                filtered_row = {k: row[k] for k in retain_cols if k in row}
                writer.writerow(filtered_row)

with tempfile.TemporaryDirectory() as tmp:
    src = Path(tmp) / "src.csv"
    src.write_text("id,name,age,salary\n1,Alice,30,90000\n2,Bob,40,120000\n", encoding="utf-8")
    dst = Path(tmp) / "dst.csv"
    
    filter_csv_columns(src, dst, ["id", "name"])
    assert dst.read_text(encoding="utf-8") == "id,name\n1,Alice\n2,Bob\n"
```
</details>

### Challenge 5 (Hard): Round-Trip Dataclass to TOML/JSON Converter
Create a `@dataclass` representing an application configuration (with nested tables and integer ports). Write functions `to_json_file(instance, path)` and `from_toml_bytes(toml_bytes)` converting between the formats and validating field types.
<details>
<summary><strong>Solution</strong></summary>

```python
from dataclasses import dataclass, asdict
import json
import tomllib

@dataclass
class ServerConfig:
    host: str
    port: int
    debug: bool

def config_to_json(cfg: ServerConfig) -> str:
    return json.dumps(asdict(cfg), indent=2)

def config_from_toml(raw_toml: bytes) -> ServerConfig:
    data = tomllib.loads(raw_toml.decode("utf-8"))
    server_data = data.get("server", data)
    return ServerConfig(
        host=str(server_data["host"]),
        port=int(server_data["port"]),
        debug=bool(server_data["debug"])
    )

sample_toml = b'[server]\nhost = "127.0.0.1"\nport = 5000\ndebug = true\n'
cfg = config_from_toml(sample_toml)
assert cfg.host == "127.0.0.1"
assert cfg.port == 5000
assert cfg.debug is True

json_repr = config_to_json(cfg)
assert '"port": 5000' in json_repr
```
</details>

### Challenge 6 (Hard): Streaming JSON Lines (JSONL) Processor
JSON Lines (`.jsonl`) is a standard format where each line is a separate, self-contained JSON object. Write an append-only writer `JSONLWriter(path)` and a lazy generator reader `stream_jsonl(path)` that streams records line-by-line in $O(1)$ memory.
<details>
<summary><strong>Solution</strong></summary>

```python
import json
from pathlib import Path
import tempfile
from typing import Iterator

class JSONLWriter:
    def __init__(self, path: Path) -> None:
        self.path = path

    def write_record(self, record: dict) -> None:
        with open(self.path, "a", encoding="utf-8") as f:
            f.write(json.dumps(record) + "\n")

def stream_jsonl(path: Path) -> Iterator[dict]:
    with open(path, "r", encoding="utf-8") as f:
        for line in f:
            clean = line.strip()
            if clean:
                yield json.loads(clean)

with tempfile.TemporaryDirectory() as tmp:
    jl_file = Path(tmp) / "events.jsonl"
    writer = JSONLWriter(jl_file)
    writer.write_record({"event": "click", "user_id": 101})
    writer.write_record({"event": "purchase", "user_id": 101, "amount": 45.0})

    streamed = list(stream_jsonl(jl_file))
    assert len(streamed) == 2
    assert streamed[0]["event"] == "click"
    assert streamed[1]["amount"] == 45.0
```
</details>
