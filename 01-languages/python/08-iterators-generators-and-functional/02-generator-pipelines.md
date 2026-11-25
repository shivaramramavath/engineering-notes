# Module 08: Iterators, Generators & Functional Programming (02-generator-pipelines)

> **Topic Overview**: Generator pipelines are Python's idiomatic realization of Unix-style streaming architectures. By chaining independent, single-responsibility generator stages together, developers can process multi-gigabyte log files, database streams, or network feeds with constant $O(1)$ memory overhead and zero intermediate disk or memory buffers.

---

## Metadata Table

| Field | Value |
|---|---|
| **Category** | Core Language / Data Pipelines & Streaming |
| **Target Level** | Intermediate to Advanced |
| **Python Version** | Python 3.10+ (tested on Python 3.12 - 3.14+) |
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

- **Problem Statement**: Standard data processing patterns load full files into memory via `.readlines()` or list comprehensions (`[transform(x) for x in data if filter(x)]`). When data files reach tens of gigabytes, memory spikes trigger Out-Of-Memory (OOM) operating system kills.
- **Mental Model**: In a generator pipeline, computation is pull-driven. The final consumer (e.g., `sum()`, a `for` loop, or a file writer) requests a single item from the last stage. That stage requests an item from its predecessor, and so on, cascading back to the data source. Only one item travels through the pipeline at any given moment.
- **When to Use**:
  - Processing large log files, CSVs, or Parquet exports.
  - Real-time event and telemetry processing.
  - Extract-Transform-Load (ETL) data pipelines.
- **When NOT to Use**: Do not use pipelines if intermediate operations require global state or random access (e.g., global sorting with `sort()`, which requires loading the entire dataset into memory anyway).

---

## 2. Core Concepts

### Architectural Pipeline Topology
A generator pipeline consists of four distinct roles chained sequentially:

```text
+-------------------------------------------------------------------------------+
| Pipeline Architecture: Unix Pipes in Python                                   |
+-------------------------------------------------------------------------------+
| [1. Producer]    --> read_lines(file)        (Emits raw strings lazily)       |
|                             │                                                 |
|                             v                                                 |
| [2. Filter]      --> filter_errors(lines)    (Discards non-matching items)    |
|                             │                                                 |
|                             v                                                 |
| [3. Transformer] --> parse_json(lines)       (Converts strings to dicts)      |
|                             │                                                 |
|                             v                                                 |
| [4. Consumer]    --> sum() / for-loop / write (Pulls items one at a time)     |
+-------------------------------------------------------------------------------+
```

### The Pull vs Push Paradigm
1. **Pull Model (Generators)**: The consumer pulls values down the chain via `next()`. If the consumer stops iterating, the upstream producers pause immediately with zero wasted work.
2. **Push Model (Callbacks / Rx)**: Upstream producers emit values as fast as possible and push them downstream, requiring backpressure management if consumers are slow.

---

## 3. How It Works Internally

Memory consumption in generator pipelines is strictly bounded:

```text
Standard List Approach (Eager Allocation):
[File 10 GB] ──> [RAM: 10 GB List] ──> [RAM: 2 GB Filtered] ──> [RAM: 2 GB Parsed]
Total RAM Peak: 14 GB (OOM Risk!)

Generator Pipeline Approach (Lazy Streaming):
[File 10 GB] ──> [1 line: 120 bytes] ──> [Filter] ──> [Dict: 240 bytes] ──> [Consumer]
Total RAM Peak: < 1 KB (Constant O(1) Memory!)
```

---

## 4. Syntax & API Reference

### Pipeline Composition Syntax
```python
# Composing stages cleanly
raw_stream = log_reader(filepath)
filtered_stream = (line for line in raw_stream if "ERROR" in line)
parsed_stream = (json.loads(line) for line in filtered_stream)
metric_stream = (record["latency_ms"] for record in parsed_stream)

# Consumer drives execution
total_latency = sum(metric_stream)
```

---

## 5. Code Examples

### 1. Minimal / Basic Example: Log Stream Pipeline
```python
from typing import Iterator

def generate_logs() -> Iterator[str]:
    logs = [
        "2026-11-25 10:00:01 INFO User login: alice",
        "2026-11-25 10:00:02 ERROR Database connection timeout",
        "2026-11-25 10:00:03 INFO User login: bob",
        "2026-11-25 10:00:04 ERROR Payment gateway unreachable",
        "2026-11-25 10:00:05 DEBUG Cache hit: session_99"
    ]
    for line in logs:
        yield line

# Stage 1: Filter
def filter_by_level(lines: Iterator[str], level: str) -> Iterator[str]:
    for line in lines:
        if f" {level} " in line:
            yield line

# Stage 2: Transform
def extract_message(lines: Iterator[str]) -> Iterator[str]:
    for line in lines:
        parts = line.split(" ", 3)
        yield parts[3]

# Chained Pipeline
pipeline = extract_message(filter_by_level(generate_logs(), "ERROR"))

for msg in pipeline:
    print("Extracted Error:", msg)
```
**Output**:
```text
Extracted Error: Database connection timeout
Extracted Error: Payment gateway unreachable
```

### 2. Real-World Production Example: Web Server Access Log Analyzer
```python
import re
from typing import Iterator, NamedTuple

class LogEntry(NamedTuple):
    ip: str
    status: int
    bytes_sent: int
    path: str

LOG_REGEX = re.compile(r'^(\S+) \S+ \S+ \[.*?\] "\S+ (\S+) \S+" (\d{3}) (\d+)')

def simulate_access_log() -> Iterator[str]:
    yield '192.168.1.1 - - [25/Nov/2026:10:00:00 +0000] "GET /index.html HTTP/1.1" 200 1024'
    yield '10.0.0.5 - - [25/Nov/2026:10:00:01 +0000] "POST /api/login HTTP/1.1" 401 256'
    yield '192.168.1.1 - - [25/Nov/2026:10:00:02 +0000] "GET /dashboard HTTP/1.1" 200 4096'
    yield '172.16.0.2 - - [25/Nov/2026:10:00:03 +0000] "GET /static/app.js HTTP/1.1" 200 8192'

def parse_log_entries(lines: Iterator[str]) -> Iterator[LogEntry]:
    for line in lines:
        match = LOG_REGEX.match(line)
        if match:
            ip, path, status, bytes_str = match.groups()
            yield LogEntry(ip=ip, path=path, status=int(status), bytes_sent=int(bytes_str))

def calculate_bandwidth_for_status(entries: Iterator[LogEntry], target_status: int) -> int:
    matching_bytes = (entry.bytes_sent for entry in entries if entry.status == target_status)
    return sum(matching_bytes)

pipeline_entries = parse_log_entries(simulate_access_log())
total_200_bytes = calculate_bandwidth_for_status(pipeline_entries, target_status=200)
print(f"Total Bandwidth for HTTP 200: {total_200_bytes} bytes")
```
**Output**:
```text
Total Bandwidth for HTTP 200: 13312 bytes
```

### 3. Edge Case: Comparing Memory Footprints
```python
import sys

# Case A: Eager list comprehension
eager_list = [x * 2 for x in range(1_000_000)]
list_bytes = sys.getsizeof(eager_list)

# Case B: Lazy generator expression
lazy_gen = (x * 2 for x in range(1_000_000))
gen_bytes = sys.getsizeof(lazy_gen)

print(f"Memory used by 1,000,000-item List:      {list_bytes:>10,} bytes ({list_bytes / (1024*1024):.2f} MB)")
print(f"Memory used by 1,000,000-item Generator: {gen_bytes:>10,} bytes")
print(f"Memory Ratio: List consumes {list_bytes // gen_bytes:,}x more RAM!")
```
**Output**:
```text
Memory used by 1,000,000-item List:       8,448,728 bytes (8.06 MB)
Memory used by 1,000,000-item Generator:        200 bytes
Memory Ratio: List consumes 42,243x more RAM!
```

### 4. Failure Case & Exception Handling: Pipeline Exception Propagation
```python
def producer():
    yield "record_1"
    yield "record_bad"
    yield "record_3"

def transformer(stream):
    for item in stream:
        if item == "record_bad":
            raise ValueError("Corrupted record format!")
        yield item.upper()

def consumer():
    pipeline = transformer(producer())
    try:
        for val in pipeline:
            print("Received:", val)
    except ValueError as err:
        print(f"[RECOVERED] Pipeline halted with error: {err}")

consumer()
```
**Output**:
```text
Received: RECORD_1
[RECOVERED] Pipeline halted with error: Corrupted record format!
```

### 5. Idiomatic Pythonic Pattern: Generator Pipeline with Early Termination
```python
def infinite_integers():
    n = 0
    while True:
        yield n
        n += 1

def square_numbers(nums):
    for x in nums:
        yield x * x

def take_first_n(stream, n):
    for _ in range(n):
        yield next(stream)

# Compose infinite pipeline with finite take consumer
pipeline = take_first_n(square_numbers(infinite_integers()), 5)
print("First 5 squares:", list(pipeline))
```
**Output**:
```text
First 5 squares: [0, 1, 4, 9, 16]
```

---

## 6. Edge Cases & Pitfalls

1. **Premature Materialization**:
   - Calling `list()` or `len()` in the middle of a pipeline materializes all upstream items into memory at that point, destroying the $O(1)$ memory guarantee. Keep items lazy until the final consumer.
2. **Operations Requiring Global Sorting**:
   - Sorting (`sorted()`) or finding medians cannot be done lazily because verifying the smallest element requires seeing all inputs. If sorting is needed, sort in external storage (e.g., SQLite or Unix `sort`) before streaming into Python.
3. **Closing Upstream Resources**:
   - When streaming from files or sockets, ensure the file context manager encloses the consumption loop, or use generator `finally` blocks to guarantee file handles close.

---

## 7. Common Mistakes

| Mistake | Why It Fails | Exception / Diagnostic | Correct Pattern |
|---|---|---|---|
| Calling `list(stage)` inside an intermediate stage | Materializes the entire dataset, causing high memory usage or OOM | Massive memory spike | Keep intermediate stages as generators |
| Re-iterating a generator pipeline | Pipelines are single-pass; second pass yields empty results | Missing data on second pass | Re-create the pipeline for a second run |
| Putting `sort()` inside a streaming pipeline | Sorting requires reading all elements; blocks lazy execution | Memory exhaustion / pipeline stalls | Stream pre-sorted data or use heap |
| Unhandled exception crashing entire pipeline without cleanup | Upstream file descriptors remain open if not in `try/finally` | Leaked file descriptors | Wrap file reading in `try...finally` |
| Using recursion for deep pipelines | Deeply nested recursive generators exceed call stack limit | `RecursionError: maximum recursion depth exceeded` | Use iterative loops or `itertools.chain` |
| Calling `len(pipeline)` | Iterators do not implement `__len__` | `TypeError: object of type 'generator' has no len()` | Use `sum(1 for _ in pipeline)` |

---

## 8. Performance & Complexity

- **Time Complexity**: $O(N)$ total time where $N$ is the number of records processed.
- **Space Complexity**: Strictly $O(1)$ auxiliary memory overhead regardless of dataset size (only one item active at any moment).
- **CPU Cache Locality**: High data locality since values are created, transformed, and consumed within microseconds, fitting inside CPU L1/L2 caches.

---

## 9. CPython Internals & Bytecode

Let's examine how generator expressions compile into specialized code objects:

```python
import dis

def make_pipeline(nums):
    return (x * 2 for x in nums if x > 0)

dis.dis(make_pipeline)
```

**Bytecode output**:
```text
  1           0 RESUME                   0

  2           2 LOAD_CONST               1 (<code object <genexpr> at ...>)
              4 MAKE_FUNCTION            0
             12 LOAD_FAST                0 (nums)
             14 GET_ITER
             16 CALL                     1
             24 RETURN_VALUE
```
CPython compiles the generator expression into an anonymous code object `<genexpr>`. Invoking `make_pipeline` simply instantiates the generator object without executing any iteration loop until the caller demands items.

---

## 10. Best Practices

1. **Follow the Single-Responsibility Principle**: Each generator function in a pipeline should do exactly one thing: read, filter, transform, or format.
2. **Never Materialize Unnecessarily**: Avoid calling `list()`, `tuple()`, or `set()` on intermediate stages.
3. **Terminate Lazily**: Use generator filters and slicing (e.g., `itertools.islice`) to stop reading files as soon as required criteria are met.

---

## 11. Real-World Case Study: Real-Time Sensor Telemetry Anomaly Detector

```python
from typing import Iterator, NamedTuple
import statistics

class SensorReading(NamedTuple):
    sensor_id: str
    temperature: float
    timestamp: int

def simulate_telemetry_feed() -> Iterator[SensorReading]:
    readings = [
        SensorReading("S1", 21.5, 1000),
        SensorReading("S1", 22.0, 1001),
        SensorReading("S1", 21.8, 1002),
        SensorReading("S1", 95.4, 1003),  # Anomaly!
        SensorReading("S1", 22.1, 1004),
    ]
    for r in readings:
        yield r

def detect_temperature_spikes(
    readings: Iterator[SensorReading],
    max_threshold: float = 50.0
) -> Iterator[str]:
    for r in readings:
        if r.temperature > max_threshold:
            yield f"[ALERT] Sensor {r.sensor_id} reported extreme temp {r.temperature}C at t={r.timestamp}"

# Connect Pipeline
sensor_stream = simulate_telemetry_feed()
alert_pipeline = detect_temperature_spikes(sensor_stream, max_threshold=50.0)

for alert in alert_pipeline:
    print(alert)
```
**Output**:
```text
[ALERT] Sensor S1 reported extreme temp 95.4C at t=1003
```

---

## 12. Interview Questions

### Q1: What is the primary architectural advantage of generator pipelines over list operations?
<details>
<summary><strong>Answer</strong></summary>
Generator pipelines achieve $O(1)$ memory consumption by processing elements one at a time on-demand (lazy evaluation). In contrast, list operations materialize all intermediate stages in RAM, consuming $O(N)$ memory and risking out-of-memory crashes on large datasets.
</details>

### Q2: Why is the execution of a generator pipeline described as "pull-driven"?
<details>
<summary><strong>Answer</strong></summary>
Execution is driven entirely by the final consumer requesting the next item via <code>next()</code>. This demand propagates backwards through each connected generator stage until the producer yields a value. If the consumer stops requesting items, the entire pipeline immediately pauses.
</details>

### Q3: What happens if an exception is raised in the middle of a generator pipeline?
<details>
<summary><strong>Answer</strong></summary>
The exception immediately propagates down to the consumer frame that called <code>next()</code>. Execution of the pipeline is halted, and any enclosing <code>finally</code> blocks in active generator stages are executed during stack unwinding.
</details>

### Q4: Can you sort items lazily inside a generator pipeline?
<details>
<summary><strong>Answer</strong></summary>
No. Sorting fundamentally requires observing every element in the dataset before the first sorted item can be identified. Any sorting operation (such as <code>sorted()</code>) forces eager materialization of all upstream items, breaking the $O(1)$ memory guarantee.
</details>

### Q5: How do you calculate the count of items in a generator pipeline without loading them all into a list?
<details>
<summary><strong>Answer</strong></summary>
Use the idiomatic expression <code>sum(1 for _ in pipeline)</code>. This iterates through all items lazily, counting them in $O(1)$ memory without creating a list.
</details>

### Q6: How does Python's generator pipeline compare to Unix pipelines (`cat | grep | awk`)?
<details>
<summary><strong>Answer</strong></summary>
Both architectures follow the single-responsibility filter pattern with streaming data flow. However, while Unix pipes use OS inter-process communication (IPC) and separate processes with OS buffers, Python generator pipelines run within a single thread and process using function frame suspension, eliminating OS process-switching overhead.
</details>

### Q7: What is the impact of placing `tuple(pipeline)` in an intermediate pipeline stage?
<details>
<summary><strong>Answer</strong></summary>
It acts as a synchronization barrier and memory sink. It consumes the entire upstream stream immediately, loads all elements into a tuple in RAM, and only then allows downstream processing to begin, destroying streaming performance.
</details>

### Q8: How can a generator pipeline be terminated early without processing remaining data?
<details>
<summary><strong>Answer</strong></summary>
The consumer can simply execute a <code>break</code> statement, return, or use <code>itertools.islice</code>. Once the consumer stops calling <code>next()</code>, the upstream generator frames are garbage collected and closed via <code>GeneratorExit</code>.
</details>

---

## 13. Exercises & Challenges

### Challenge 1 (Easy): Basic Pipeline Transformer
Construct a three-stage pipeline:
1. `numbers()`: yields integers 1 through 10.
2. `filter_odds()`: filters and yields only odd integers.
3. `multiply_by_ten()`: multiplies each incoming integer by 10.
Consume into a list and verify output.
<details>
<summary><strong>Solution</strong></summary>

```python
def numbers():
    yield from range(1, 11)

def filter_odds(stream):
    for x in stream:
        if x % 2 != 0:
            yield x

def multiply_by_ten(stream):
    for x in stream:
        yield x * 10

pipeline = multiply_by_ten(filter_odds(numbers()))
assert list(pipeline) == [10, 30, 50, 70, 90]
```
</details>

### Challenge 2 (Easy): Word Counter Stream
Write a generator pipeline that takes a list of sentences, splits them into individual words, and counts the total number of words without creating intermediate lists of words.
<details>
<summary><strong>Solution</strong></summary>

```python
sentences = ["Hello world from Python", "Generators save memory", "Pipeline architecture"]

word_stream = (word for s in sentences for word in s.split())
total_words = sum(1 for _ in word_stream)

assert total_words == 8
```
</details>

### Challenge 3 (Medium): Streaming CSV Parser
Write a generator pipeline that accepts raw CSV lines, strips whitespace, skips comment lines starting with `#`, parses comma-separated values into dictionaries using header keys, and filters records where `status == "ACTIVE"`.
<details>
<summary><strong>Solution</strong></summary>

```python
raw_csv = [
    "# Config File v1",
    "id,name,status",
    "1,Alice,ACTIVE",
    "# Maintenance user",
    "2,Bob,INACTIVE",
    "3,Charlie,ACTIVE"
]

def clean_lines(lines):
    for line in lines:
        s = line.strip()
        if s and not s.startswith("#"):
            yield s

def parse_csv_dicts(lines):
    header = next(lines).split(",")
    for line in lines:
        values = line.split(",")
        yield dict(zip(header, values))

def filter_active(records):
    for r in records:
        if r.get("status") == "ACTIVE":
            yield r

cleaned = clean_lines(raw_csv)
records = parse_csv_dicts(cleaned)
active_users = list(filter_active(records))

assert len(active_users) == 2
assert active_users[0]["name"] == "Alice"
assert active_users[1]["name"] == "Charlie"
```
</details>

### Challenge 4 (Medium): Moving Average Filter Stream
Implement a generator stage `moving_average(stream, window_size=3)` that yields the rolling average of numbers from an incoming numerical generator with $O(window\_size)$ memory.
<details>
<summary><strong>Solution</strong></summary>

```python
from collections import deque

def moving_average(stream, window_size: int = 3):
    window = deque(maxlen=window_size)
    for val in stream:
        window.append(val)
        if len(window) == window_size:
            yield sum(window) / window_size

nums = [10, 20, 30, 40, 50]
averages = list(moving_average(nums, window_size=3))
assert averages == [20.0, 30.0, 40.0]
```
</details>

### Challenge 5 (Hard): Deduplicating Stream with Bounded LRU Cache
Implement a generator pipeline stage `deduplicate_stream(stream, max_recent=1000)` that discards consecutive or recent duplicates using an in-memory cache bounded at `max_recent` items, ensuring memory cannot grow indefinitely on infinite streams.
<details>
<summary><strong>Solution</strong></summary>

```python
from collections import OrderedDict

def deduplicate_stream(stream, max_recent: int = 3):
    recent = OrderedDict()
    for item in stream:
        if item in recent:
            continue
        recent[item] = None
        if len(recent) > max_recent:
            recent.popitem(last=False)  # Evict oldest
        yield item

input_data = ["A", "B", "A", "C", "D", "E", "A"]  # "A" should re-appear after evicted from size 3
res = list(deduplicate_stream(input_data, max_recent=3))
assert res == ["A", "B", "C", "D", "E", "A"]
```
</details>

### Challenge 6 (Hard): Branching Pipeline Multiplexer
Build a pipeline multiplexer `TeePipeline` that splits an incoming single-pass generator into two independent consumer pipelines (e.g., calculating both minimum and maximum in a single pass without storing the entire dataset in RAM).
<details>
<summary><strong>Solution</strong></summary>

```python
import itertools

def analyze_stream(stream):
    # itertools.tee creates two independent cursors backed by a shared FIFO buffer
    s1, s2 = itertools.tee(stream, 2)
    min_val = min(s1)
    max_val = max(s2)
    return min_val, max_val

data = (x * 3 for x in [5, 2, 8, 1, 9, 4])
min_v, max_v = analyze_stream(data)

assert min_v == 3   # 1 * 3
assert max_v == 27  # 9 * 3
```
</details>
