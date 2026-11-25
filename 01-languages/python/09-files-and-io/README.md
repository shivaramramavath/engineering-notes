# Module 09: Files, Streams & OS (`09-files-and-io`)

> **Scope**: Object-oriented filesystem paths (`pathlib`), context-managed I/O, binary vs text encodings, structured serialization (CSV/JSON/TOML/YAML), safe serialization tradeoffs (`pickle`), embedded relational data (`sqlite3`), in-memory streams (`io.StringIO`/`io.BytesIO`), shell utilities (`shutil`, `tempfile`, `os`), archival compression (`zipfile`, `tarfile`, `gzip`, `lzma`), CLI interfaces (`argparse`, Click, Typer), 12-factor configuration (`os.environ`, `pydantic-settings`), process execution (`subprocess`), and crash-consistent atomic writes/file locking.

---

## 📋 Module Overview

| Metric | Details |
|---|---|
| **Directory** | `01-languages/python/09-files-and-io` |
| **Status** | 100% Complete (13 Notes + Module README) |
| **Notes Total** | 13 Notes (`00-` to `12-`), all >= 500 lines |
| **Template Compliance** | 13/13 Mandatory Sections (`_templates/concept-template.md`) |
| **Previous Module** | [08-iterators-generators-and-functional](../08-iterators-generators-and-functional/README.md) |
| **Next Module** | [10-standard-library-tour](../10-standard-library-tour/README.md) |

---

## 🎯 Learning Outcomes
By completing this module, you will:
1. **Master Object-Oriented Path Traversal**: Replace legacy `os.path` string manipulation with modern, cross-platform, immutable `pathlib.Path` objects.
2. **Safely Manage File Streams and Encodings**: Handle BOM headers, character encoding fallbacks (UTF-8, UTF-16, Latin-1), universal newline translations, and userspace buffering.
3. **Parse and Generate Structured Data**: Efficiently stream CSVs, serialize JSON payloads, and configure settings with TOML and YAML while avoiding `pickle` RCE vulnerabilities.
4. **Implement Embedded Relational Storage**: Harness `sqlite3` in WAL mode for zero-configuration, concurrent, transactional data pipelines.
5. **Optimize Memory with In-Memory Streams**: Use `io.StringIO` and `io.BytesIO` for zero-disk unit testing, dynamic archives, and zero-copy `memoryview` inspections.
6. **Execute Robust Filesystem Automation**: Leverage `tempfile` cryptographic isolation, `shutil` recursive operations, and `os.scandir` kernel attribute caching.
7. **Defend Against Archival Vulnerabilities**: Prevent Zip Slip directory traversals with PEP 706 `data_filter` and guard against ZIP bomb resource exhaustion.
8. **Build Production CLIs and 12-Factor Configs**: Construct declarative interfaces with `argparse`, and sanitize/validate environment variables using `pydantic-settings`.
9. **Manage Subprocesses Resiliently**: Prevent shell injection vulnerabilities, deadlocks from pipe saturation, and enforce hard timeouts and signal escalation.
10. **Guarantee Crash Durability**: Prevent torn writes using atomic replacement via `os.replace()` paired with `os.fsync()` and multi-process file locking.

---

## 📑 Syllabus & Notes Directory

| # | Note | Description | Focus Areas |
|---|---|---|---|
| 00 | [Modern Filesystem Navigation with pathlib](./00-pathlib.md) | Pure paths vs concrete paths, `/` operator overloading, directory globbing, and cross-platform resolution. | `Path`, `PurePath`, `rglob`, `resolve`, `stat` |
| 01 | [Reading and Writing Files](./01-reading-and-writing-files.md) | File object lifecycles, context managers, buffer modes, chunk streaming, and resource leak prevention. | `open`, `with`, `read`, `write`, `buffering` |
| 02 | [Text vs Binary Streams and Encodings](./02-text-vs-binary-and-encodings.md) | Unicode code points, UTF-8/16/32 byte representations, BOM headers, error handlers, and newline normalization. | `bytes`, `str`, `errors='replace'`, `newlines` |
| 03 | [Structured Data Formats: CSV, JSON, TOML, and YAML](./03-csv-json-toml-yaml.md) | Standard library `csv` & `json`, Python 3.11+ `tomllib`, PyYAML safety, and large-stream processing. | `csv.DictWriter`, `json.dumps`, `tomllib`, `yaml.safe_load` |
| 04 | [Object Serialization with pickle and Its Security Risks](./04-pickle-and-its-risks.md) | Pickle protocol internals, opcode execution, `__reduce__` exploit vectors, and safe serialization alternatives. | `pickle`, `__reduce__`, RCE exploits, `shelve` |
| 05 | [SQLite3 Basics in Python](./05-sqlite3-basics.md) | Embedded relational database, connection lifecycles, cursors, parameterized queries, and WAL mode concurrency. | `sqlite3.connect`, `WAL`, `executemany`, `sqlite3.Row` |
| 06 | [In-Memory Streams: io.StringIO and io.BytesIO](./06-io-streams-stringio-bytesio.md) | RAM-backed file polymorphism, cursor positioning (`seek`/`tell`), `getbuffer()` zero-copy, and mock testing. | `io.StringIO`, `io.BytesIO`, `seek`, `getbuffer` |
| 07 | [Filesystem Operations: tempfile, shutil, and os](./07-tempfile-shutil-and-os.md) | Secure temp directories, recursive copying, tree purging, disk quotas, and fast `os.scandir` directory traversal. | `tempfile`, `shutil.copy2`, `shutil.rmtree`, `os.scandir` |
| 08 | [Archiving and Compression](./08-zip-and-compression.md) | `zipfile`, `tarfile`, `gzip`, `bz2`, `lzma`, Zip Slip security defenses, and PEP 706 extraction filters. | `zipfile`, `tarfile`, `data_filter`, `gzip`, `lzma` |
| 09 | [Command-Line Interfaces: argparse, sys.argv, Click, and Typer](./09-command-line-args-argparse-click-typer.md) | Low-level argument parsing, standard library `argparse`, subcommands, type coercion, and modern CLI paradigms. | `sys.argv`, `argparse`, subparsers, flags, exit codes |
| 10 | [Configuration and Environment Variables](./10-environment-variables-and-config.md) | 12-Factor configuration, `os.environ` synchronization, dotenv loading, Pydantic-Settings, and secret masking. | `os.environ`, `os.getenv`, `python-dotenv`, `SecretStr` |
| 11 | [Subprocess and Process Management in Python](./11-subprocess.md) | Process spawning, `subprocess.run` vs `Popen`, pipe deadlocks, shell injection prevention, and signal escalation. | `subprocess.run`, `Popen`, `PIPE`, `timeout`, `SIGKILL` |
| 12 | [Atomic File Writes and File Locking in Python](./12-file-locking-and-atomic-writes.md) | Crash consistency, preventing torn writes with `os.replace()`, `os.fsync()`, and cross-platform file locking. | `os.replace`, `os.fsync`, `fcntl.flock`, `msvcrt` |

---

## 🛠️ Verification & Quality Assurance
- **Line Count**: All 13 notes exceed 500 lines of rigorous technical instruction.
- **Completeness**: 0 placeholder notes, 0 omitted exercises, 0 incomplete interview scenarios.
- **CPython Alignment**: Modernized for Python 3.12+ (PEP 706 `data_filter`, `delete_on_close`, `exit_on_error`, `tomllib`).
- **Markdown-Only Rule**: Zero binary files (`.py`, `.db`, `.zip`) stored in the repository.
