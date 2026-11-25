---
title: "Configuration and Environment Variables: os.environ, dotenv, and Pydantic Settings"
description: "Mastering application configuration in Python: os.environ vs os.getenv, 12-Factor App methodology, dotenv loading mechanics, type-safe settings with Pydantic-Settings, process environment inheritance, and secrets sanitization."
module: "09-files-and-io"
order: 10
tags:
  - python
  - config
  - environment-variables
  - os-environ
  - dotenv
  - pydantic-settings
  - 12-factor
difficulty: "Intermediate"
cpython_version: "3.12+"
last_verified: "2026-03"
---

# Configuration and Environment Variables: os.environ, dotenv, and Pydantic Settings

[<- Back to Module Index](./README.md)

---

## Table of Contents
1. [What & Why](#what--why)
2. [Core Concepts & Mental Model](#core-concepts--mental-model)
3. [How It Works Internally](#how-it-works-internally)
4. [Syntax & API Reference](#syntax--api-reference)
5. [Code Examples](#code-examples)
6. [Edge Cases, Pitfalls & Anti-Patterns](#edge-cases-pitfalls--anti-patterns)
7. [Performance & Memory Characteristics](#performance--memory-characteristics)
8. [CPython Internals & Bytecode](#cpython-internals--bytecode)
9. [Best Practices & Idiomatic Patterns](#best-practices--idiomatic-patterns)
10. [Real-World Case Study: Production 12-Factor Microservice Settings Manager](#real-world-case-study-production-12-factor-microservice-settings-manager)
11. [Interview Questions & Deep-Dive Scenarios](#interview-questions--deep-dive-scenarios)
12. [Practical Exercises & Code Katas](#practical-exercises--code-katas)

---

## What & Why

Modern software architecture adheres to the **Twelve-Factor App methodology**, which stipulates that application configuration—anything that varies across deployments (staging, production, development)—must be strictly decoupled from the codebase and injected via environment variables.

### Motivation
Hardcoding configuration variables (such as database credentials, API keys, cache timeouts, or service endpoints) directly in source files introduces catastrophic vulnerabilities:
1. Accidental leakage of sensitive credentials to public Git repositories.
2. Inability to change runtime behavior without recompiling, packaging, and redeploying code.
3. Violation of immutable container deployment principles (Docker, Kubernetes).

Python provides built-in environment access via `os.environ` and `os.getenv`, and the ecosystem extends this with `.env` file parsers (`python-dotenv`) and enterprise validation engines (`pydantic-settings`).

### Real-World Analogy
Consider an international power adapter. A consumer appliance (your application code) is manufactured with a standardized plug. Instead of rewiring the appliance's internal motor every time you travel to the UK, Japan, or the US, you plug into the wall socket (environment variables) provided by the local building infrastructure. The wall socket supplies the specific voltage and frequency (configuration) required for that physical location.

### When to Use vs When Not to Use
- **Use Environment Variables for:**
  - Secrets: API tokens, private keys, database passwords.
  - Per-environment endpoints: Database connection strings, Redis URLs, external API hosts.
  - Runtime operational flags: Log levels (`LOG_LEVEL=DEBUG`), worker thread counts, feature toggles.
- **Do Not Use Environment Variables for:**
  - Complex, deeply nested hierarchical configuration structures (use TOML, YAML, or JSON files instead).
  - High-frequency dynamic configurations that mutate continuously at runtime (use a distributed consensus store like Consul, Etcd, or Redis).
  - Non-sensitive static application defaults (e.g. math constants, default pagination size).

---

## Core Concepts & Mental Model

```
+-------------------------------------------------------------------------+
| Host OS Process Table                                                   |
| Parent Process (Bash, Docker, systemd, K8s)                             |
| Environment Block: {"PATH": "...", "PORT": "8080", "DB_PASS": "s3cr3t"} |
+-------------------------------------------------------------------------+
                                    |
                                    | Fork / Exec (Inherits environment block)
                                    v
+-------------------------------------------------------------------------+
| Python Process Runtime                                                  |
|                                                                         |
|   +-----------------------+              +--------------------------+   |
|   | .env File (Local Dev) |              | os.environ               |   |
|   | DB_HOST=localhost     | -----------> | - Custom Mapping Object  |   |
|   | (python-dotenv)       |              | - Synchronized with C    |   |
|   +-----------------------+              |   environ pointer        |   |
|                                          +--------------------------+   |
|                                                        |                |
|                                                        v                |
|                                          +--------------------------+   |
|                                          | Pydantic-Settings        |   |
|                                          | - Type validation        |   |
|                                          | - Default coercion       |   |
|                                          | - Immutable Settings Obj |   |
|                                          +--------------------------+   |
+-------------------------------------------------------------------------+
```

### Key Distinctions: `os.environ` vs `os.getenv`
- **`os.environ`**: A mutable `os._Environ` mapping object initialized at process startup. Accessing a missing key via `os.environ["KEY"]` raises `KeyError`. Modifying `os.environ["KEY"] = "val"` updates the internal Python mapping **and** calls the C-level `putenv()`/`SetEnvironmentVariable()` system call so child processes inherit the change.
- **`os.getenv(key, default=None)`**: A convenience wrapper around `os.environ.get()`. Returns `None` or the specified default if the key is absent, preventing `KeyError`.

---

## How It Works Internally

### 1. Process Environment Inheritance
When a parent process spawns a child process (via POSIX `fork()` + `execve()` or Windows `CreateProcess()`), the operating system copies the parent's environment block (an array of null-terminated `KEY=VALUE\0` strings) into the child's address space. A child process **can never modify the environment of its parent process**—environment variables flow strictly downward.

### 2. Synchronization between Python and C `environ`
CPython's `os.environ` is not a raw Python dictionary; it is an instance of `os._Environ` (`Lib/os.py`).
When you execute:
```python
os.environ["API_KEY"] = "token123"
```
CPython performs two operations:
1. Updates its internal Python dictionary cache.
2. Calls the POSIX C library function `putenv("API_KEY=token123")` (or Windows `SetEnvironmentVariableW()`).
This guarantees that if Python subsequently calls C extension libraries or spawns subprocesses via `subprocess.run()`, the underlying OS environment block reflects the modified variable.

---

## Syntax & API Reference

### Built-in `os` Module
```python
import os

# Reading
val: str = os.environ["MANDATORY_KEY"]           # Raises KeyError if missing
val: str | None = os.getenv("OPTIONAL_KEY")     # Returns None if missing
val: str = os.getenv("TIMEOUT", "30")           # Returns "30" if missing

# Mutating
os.environ["NEW_KEY"] = "string_value_only"     # Value MUST be str, not int/bool!
del os.environ["OBSOLETE_KEY"]

# Bulk inspection
all_vars: dict[str, str] = dict(os.environ)
```

### `python-dotenv` Integration
```python
from dotenv import load_dotenv, find_dotenv

# Search upward for .env file and load into os.environ
# override=False preserves existing environment variables (e.g. from Docker/K8s)
load_dotenv(find_dotenv(), override=False)
```

### `pydantic-settings` (Modern Production Paradigm)
```python
from pydantic_settings import BaseSettings, SettingsConfigDict
from pydantic import Field, SecretStr, PostgresDsn

class AppSettings(BaseSettings):
    model_config = SettingsConfigDict(
        env_file=".env",
        env_file_encoding="utf-8",
        case_sensitive=False,
        extra="ignore"
    )

    app_name: str = "MyService"
    port: int = Field(default=8080, ge=1, le=65535)
    database_url: PostgresDsn
    api_secret: SecretStr                       # Masks secret in logs / repr
```

---

## Code Examples

### Example 1: Robust Reading and Type Parsing with Standard Library
```python
import os

def get_config_value(key: str, default: int) -> int:
    raw = os.getenv(key)
    if raw is None:
        return default
    try:
        return int(raw)
    except ValueError:
        raise ValueError(f"Environment variable '{key}' must be a valid integer; got '{raw}'")

def run_stdlib_config_demo():
    # Setup test environment
    os.environ["MAX_CONNECTIONS"] = "150"
    os.environ["PORT"] = "8080"

    max_conn = get_config_value("MAX_CONNECTIONS", default=10)
    timeout = get_config_value("TIMEOUT_SECONDS", default=30)

    print("Max Connections (from env):", max_conn)
    print("Timeout (fallback default):", timeout)

run_stdlib_config_demo()
# Output:
# Max Connections (from env): 150
# Timeout (fallback default): 30
```

### Example 2: In-Memory Dotenv Parser Implementation
*(Demonstrating how `.env` files are parsed without external dependencies)*
```python
import io
import re

def parse_dotenv(stream: io.StringIO) -> dict[str, str]:
    """
    Parses key-value pairs from a .env format string.
    Supports comments (#), quotes, and export prefixes.
    """
    env_vars = {}
    line_re = re.compile(r'^(?:export\s+)?([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(.*)$')

    for raw_line in stream:
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue

        match = line_re.match(line)
        if not match:
            continue

        key, val = match.groups()
        # Strip matching surrounding quotes
        if (val.startswith('"') and val.endswith('"')) or (val.startswith("'") and val.endswith("'")):
            val = val[1:-1]
        else:
            # Inline comment stripping for unquoted values
            val = val.split("#", 1)[0].strip()

        env_vars[key] = val
    return env_vars

def test_dotenv_parser():
    sample_env = (
        "# Database Credentials\n"
        "DB_HOST=127.0.0.1 # local connection\n"
        'DB_PASS="p@ss#word!123"\n'
        "export POOL_SIZE=25\n"
    )
    result = parse_dotenv(io.StringIO(sample_env))
    print("Parsed .env dictionary:")
    for k, v in result.items():
        print(f"  {k} = {v}")

test_dotenv_parser()
# Output:
# Parsed .env dictionary:
#   DB_HOST = 127.0.0.1
#   DB_PASS = p@ss#word!123
#   POOL_SIZE = 25
```

### Example 3: Boolean Variable Parsing Pitfall
```python
import os

def parse_bool_env(key: str, default: bool = False) -> bool:
    """
    Safely converts environment strings ('true', '1', 'yes') into Python booleans.
    Avoids the fatal bool("false") == True trap.
    """
    val = os.getenv(key)
    if val is None:
        return default
    
    normalized = val.strip().lower()
    if normalized in ("true", "1", "yes", "on", "t"):
        return True
    elif normalized in ("false", "0", "no", "off", "f"):
        return False
    else:
        raise ValueError(f"Invalid boolean value for environment variable '{key}': '{val}'")

def run_bool_demo():
    os.environ["FEATURE_X_ENABLED"] = "False"
    os.environ["DEBUG_MODE"] = "1"

    # Naive bad parsing:
    naive_val = bool(os.getenv("FEATURE_X_ENABLED"))
    print("NAIVE bool('False'):", naive_val)  # TRAP: True!

    # Correct parsing:
    safe_feature_x = parse_bool_env("FEATURE_X_ENABLED")
    safe_debug = parse_bool_env("DEBUG_MODE")
    print("SAFE FEATURE_X_ENABLED:", safe_feature_x)
    print("SAFE DEBUG_MODE:", safe_debug)

run_bool_demo()
# Output:
# NAIVE bool('False'): True
# SAFE FEATURE_X_ENABLED: False
# SAFE DEBUG_MODE: True
```

### Example 4: Child Process Environment Isolation with `subprocess`
```python
import os
import subprocess
import sys

def run_subprocess_env_demo():
    # Construct an explicit isolated environment dictionary for the child process
    child_env = os.environ.copy()
    child_env["ISOLATED_VAR"] = "child_process_secret"
    # Redact sensitive host credentials from leaking into child process
    child_env.pop("HOST_MASTER_KEY", None)

    code = "import os; print('Child sees ISOLATED_VAR:', os.getenv('ISOLATED_VAR'))"
    
    result = subprocess.run(
        [sys.executable, "-c", code],
        env=child_env,
        capture_output=True,
        text=True,
        check=True
    )
    print("Subprocess Output:", result.stdout.strip())
    print("Parent sees ISOLATED_VAR:", os.getenv("ISOLATED_VAR"))

run_subprocess_env_demo()
# Output:
# Subprocess Output: Child sees ISOLATED_VAR: child_process_secret
# Parent sees ISOLATED_VAR: None
```

### Example 5: Masking Sensitive Secrets in Memory and Logs
```python
import os
from typing import NamedTuple

class SecretValue:
    """Wrapper that prevents sensitive strings from accidentally leaking in logs or tracebacks."""
    def __init__(self, value: str):
        self._value = value

    def get_secret_value(self) -> str:
        return self._value

    def __repr__(self) -> str:
        return "SecretValue('**********')"

    def __str__(self) -> str:
        return "**********"

class DatabaseConfig(NamedTuple):
    host: str
    port: int
    password: SecretValue

def run_secret_mask_demo():
    os.environ["DB_PASS"] = "super_classified_token"
    
    cfg = DatabaseConfig(
        host="db.internal.net",
        port=5432,
        password=SecretValue(os.environ["DB_PASS"])
    )

    # Logging or printing config does NOT leak secret
    print("Config repr:", repr(cfg))
    print("Direct password print:", cfg.password)
    # Explicit secret extraction when dialing DB driver
    print("Actual secret used for connection:", cfg.password.get_secret_value()[:5] + "...")

run_secret_mask_demo()
# Output:
# Config repr: DatabaseConfig(host='db.internal.net', port=5432, password=SecretValue('**********'))
# Direct password print: **********
# Actual secret used for connection: super...
```

---

## Edge Cases, Pitfalls & Anti-Patterns

### 1. Assigning Non-String Types to `os.environ`
`os.environ` accepts **only string keys and string values**.
Executing `os.environ["PORT"] = 8080` or `os.environ["DEBUG"] = False` raises `TypeError: str expected, not int`. Always cast to string explicitly: `os.environ["PORT"] = str(8080)`.

### 2. Overwriting Production Host Variables with `.env` Files
When deploying to container environments (Kubernetes, AWS ECS), environment variables are injected at the container level. If your `.env` loader unconditionally overwrites variables (`override=True`), local development defaults will override production secrets!
- **Rule**: Always configure `.env` loaders with `override=False` (the default in `python-dotenv`).

### 3. Case Sensitivity Discrepancies Across Platforms
On Windows, environment variable names are case-insensitive (`os.environ["path"]` matches `"PATH"`). On POSIX (Linux/macOS), environment variables are strictly case-sensitive (`"Path"`, `"PATH"`, and `"path"` can coexist as distinct variables). Writing cross-platform code requires sticking strictly to uppercase alphanumeric keys (`UPPERCASE_SNAKE_CASE`).

### Common Mistakes Table

| Bad Code | Good Code | Why | Severity |
| :--- | :--- | :--- | :--- |
| `os.environ["PORT"] = 8080` | `os.environ["PORT"] = str(8080)` | `os.environ` raises `TypeError` on non-string values. | High |
| `is_debug = bool(os.getenv("DEBUG"))` | `parse_bool_env("DEBUG")` | Any non-empty string (`"False"`, `"0"`) evaluates to `True` in Python! | Critical |
| `load_dotenv(override=True)` in prod | `load_dotenv(override=False)` | Overwrites genuine production secrets injected by Kubernetes or Docker. | Critical |
| Logging `str(settings)` with raw credentials | Use `SecretStr` or custom masking wrapper | Secrets are dumped to centralized log aggregators (Elastic, Datadog). | Critical |
| Expecting child process to mutate parent's env | Use IPC, files, or stdout communication | Environment variables flow strictly downward from parent to child. | High |
| `os.environ.get("KEY", 30)` expecting `int` | `int(os.environ.get("KEY", "30"))` | If `"KEY"` exists, it returns `str`, resulting in inconsistent types. | Medium |

---

## Performance & Memory Characteristics

### Access Latency
- `os.environ.get("KEY")`: $\approx 40 \text{ ns}$ (pure Python dictionary lookup against internal cache).
- `os.environ["KEY"] = "val"`: $\approx 1.2 \mu\text{s}$ (invokes native OS `putenv` / `SetEnvironmentVariableW` system call).

### Memory Footprint
The entire environment block of a typical container process is negligible (<64 KB). However, calling `dict(os.environ)` repeatedly in high-frequency loops allocates new dictionary instances. Cache your validated settings object once at application startup.

---

## CPython Internals & Bytecode

In CPython (`Modules/posixmodule.c` and `Lib/os.py`):
`os.environ` is initialized when the `os` module loads:
```python
# Lib/os.py
def _createenviron():
    # Reads external C pointer: extern char **environ;
    # Decodes bytes to str using os.fsdecode
    data = {}
    for key, value in _posix_environ():
        data[key] = value
    return _Environ(data)
```

The `_Environ` class overrides `__setitem__` to invoke the C-level `posix.putenv()`:
```python
def __setitem__(self, key, value):
    # Enforces str type
    value = _check_type(value)
    key = _check_type(key)
    putenv(key, value)
    self._data[key] = value
```

---

## Best Practices & Idiomatic Patterns

1. **Fail-Fast on Missing Critical Secrets**:
   Never use silent fallback defaults for production database passwords or encryption keys:
   ```python
   # Bad: db_pass = os.getenv("DB_PASS", "default_secret")
   # Good:
   db_pass = os.environ["DB_PASS"]  # Crashes immediately at startup if missing
   ```
2. **Centralize Settings in a Single Module**:
   Never scatter `os.getenv()` calls across 50 different application files. Parse and validate all configuration once inside a dedicated `config.py` module and export the typed instance.
3. **Keep `.env` Out of Source Control**:
   Always add `.env`, `.env.local`, and `*.env` to your project's `.gitignore`. Commit only `.env.example` containing dummy keys without actual credentials.

---

## Real-World Case Study: Production 12-Factor Microservice Settings Manager

The following module implements a complete, zero-dependency, 12-Factor configuration loader with automatic `.env` discovery, type casting, validation, and secret masking.

```python
import os
from pathlib import Path
from typing import Any, Optional, Dict

class Secret:
    def __init__(self, value: str):
        self._val = value

    def expose(self) -> str:
        return self._val

    def __repr__(self) -> str:
        return "'[REDACTED]'"

    def __str__(self) -> str:
        return "[REDACTED]"

class AppConfig:
    def __init__(self, env_file: Optional[Path] = None):
        if env_file and env_file.exists():
            self._load_dotenv_file(env_file)

        # Mandatory production settings
        self.app_name: str = self._get_required("APP_NAME")
        self.port: int = self._get_int("PORT", default=8080)
        self.debug: bool = self._get_bool("DEBUG", default=False)
        self.database_url: str = self._get_required("DATABASE_URL")
        self.api_secret: Secret = Secret(self._get_required("API_SECRET"))

    def _load_dotenv_file(self, path: Path) -> None:
        """Loads .env file without overwriting existing host environment variables."""
        content = path.read_text(encoding="utf-8")
        for line in content.splitlines():
            line = line.strip()
            if not line or line.startswith("#") or "=" not in line:
                continue
            k, v = line.split("=", 1)
            k, v = k.strip(), v.strip().strip('"').strip("'")
            if k not in os.environ:
                os.environ[k] = v

    def _get_required(self, key: str) -> str:
        val = os.getenv(key)
        if not val:
            raise KeyError(f"Configuration error: Missing mandatory environment variable '{key}'")
        return val

    def _get_int(self, key: str, default: int) -> int:
        val = os.getenv(key)
        return int(val) if val is not None else default

    def _get_bool(self, key: str, default: bool) -> bool:
        val = os.getenv(key)
        if val is None:
            return default
        return val.strip().lower() in ("true", "1", "yes", "on")

    def to_dict(self) -> Dict[str, Any]:
        """Safely serializes config for diagnostics without leaking secrets."""
        return {
            "app_name": self.app_name,
            "port": self.port,
            "debug": self.debug,
            "database_url": self.database_url,
            "api_secret": repr(self.api_secret)
        }

# Verification
if __name__ == "__main__":
    import tempfile
    with tempfile.TemporaryDirectory() as tmp:
        env_path = Path(tmp) / ".env"
        env_path.write_text(
            "APP_NAME=PaymentGateway\n"
            "PORT=9000\n"
            "DEBUG=false\n"
            "DATABASE_URL=postgres://user:pw@db:5432/pay\n"
            "API_SECRET=sk_live_99238472918\n"
        )
        
        cfg = AppConfig(env_file=env_path)
        print("Loaded Configuration:")
        for k, v in cfg.to_dict().items():
            print(f"  {k}: {v}")
```

---

## Interview Questions & Deep-Dive Scenarios

<details>
<summary><strong>1. Junior: What is the difference between <code>os.environ['KEY']</code> and <code>os.getenv('KEY')</code>?</strong></summary>

`os.environ['KEY']` looks up `'KEY'` in the environment mapping and raises a `KeyError` if the key does not exist. `os.getenv('KEY', default=None)` safely queries the environment, returning `None` (or a specified fallback value) if the key is missing. Use `os.environ['KEY']` for mandatory configuration that must crash if missing, and `os.getenv()` for optional settings with defaults.
</details>

<details>
<summary><strong>2. Junior: Why does <code>bool(os.getenv("DEBUG", "False"))</code> evaluate to <code>True</code>?</strong></summary>

In Python, any non-empty string is truthy. Because `os.getenv()` returns the string `"False"` (a string of length 5), `bool("False")` evaluates to `True`. To parse booleans safely, compare against recognized strings: `os.getenv("DEBUG", "false").lower() in ("true", "1", "yes")`.
</details>

<details>
<summary><strong>3. Mid: If a Python script modifies <code>os.environ['VAR'] = 'new'</code>, can its parent shell see this modification after the script finishes?</strong></summary>

No. Operating system process environments flow strictly from parent to child during process creation. A child process receives its own independent copy of the environment memory block. Modifying `os.environ` updates the child's address space and affects any subsequent child subprocesses spawned by Python, but cannot alter the parent process's memory space.
</details>

<details>
<summary><strong>4. Mid: Why should you configure <code>load_dotenv(override=False)</code> in production container environments?</strong></summary>

In containerized deployments (Docker, Kubernetes), secrets and configuration are injected directly into the container's environment block by the orchestration system. If `load_dotenv(override=True)` runs, any static development `.env` file packaged inside the image would overwrite the genuine production secrets injected by the orchestrator.
</details>

<details>
<summary><strong>5. Senior: How does CPython synchronize changes made to <code>os.environ</code> with low-level C libraries and subprocesses?</strong></summary>

`os.environ` is an instance of `os._Environ`. Whenever `__setitem__` or `__delitem__` is invoked on it, CPython updates its internal Python dictionary and simultaneously calls the OS C runtime function `putenv()` / `unsetenv()` (or `SetEnvironmentVariableW` on Windows). This synchronizes the process's native C `char **environ` block, ensuring that C extensions and subprocesses spawned via `fork()`/`execve()` inherit the modified values.
</details>

<details>
<summary><strong>6. Senior: How does <code>pydantic-settings</code> protect credentials from leaking in logs and error traces?</strong></summary>

`pydantic-settings` provides the `SecretStr` (and `SecretBytes`) data type. Objects of this type override `__repr__` and `__str__` to return `'**********'`. Even if a developer accidentally dumps the entire settings object via `print(settings)` or a logging statement, the plain-text secret is not displayed. The underlying string can only be accessed via an explicit call to `secret.get_secret_value()`.
</details>

<details>
<summary><strong>7. Staff: How would you architect configuration management for a zero-downtime Python service that must update API rate limits without restarting processes?</strong></summary>

1. Split configuration into **static boot settings** (database credentials, listening port) managed via immutable environment variables (`AppConfig` loaded at startup).
2. Split **dynamic runtime settings** (rate limits, circuit breaker thresholds) into an external configuration store (Consul, Etcd, AWS AppConfig).
3. Implement a background thread or async task subscribing to configuration update events (via long-polling or Redis pub/sub) that atomically swaps an immutable configuration snapshot (`types.MappingProxyType` or frozen dataclass) in application memory.
</details>

<details>
<summary><strong>8. Staff: What are the security implications of reading environment variables from <code>/proc/[pid]/environ</code> on Linux, and how do you protect application secrets?</strong></summary>

On Linux, any process running with the same UID (or root) can read `/proc/<pid>/environ`, exposing the initial environment variables passed to that process.
Mitigations:
1. Clear sensitive keys from `os.environ` immediately after reading them at startup (`del os.environ["SECRET_KEY"]`). Note that while this updates the C `environ` pointer, some Linux kernels retain the initial `/proc/<pid>/environ` block unless overwritten via `prctl(PR_SET_MM_ENV_*)`.
2. Mount Kubernetes secrets as tmpfs files rather than environment variables, reading them into memory and deleting/unlinking the files.
</details>

---

## Practical Exercises & Code Katas

### Exercise 1: Typed Environment Getter Factory
**Requirement:** Implement a function `get_env(key: str, default: T, cast: Callable[[str], T]) -> T` that returns the cast value if the environment variable exists, otherwise returning the default.

<details>
<summary>Show Solution</summary>

```python
import os
from typing import TypeVar, Callable

T = TypeVar("T")

def get_env(key: str, default: T, cast: Callable[[str], T]) -> T:
    val = os.getenv(key)
    if val is None:
        return default
    try:
        return cast(val)
    except Exception as e:
        raise ValueError(f"Failed to cast env var '{key}' with value '{val}': {e}")

# Verification
os.environ["TIMEOUT"] = "45"
timeout = get_env("TIMEOUT", default=10, cast=int)
print("Parsed timeout:", timeout, type(timeout))
assert timeout == 45
```
</details>

### Exercise 2: Cascading Configuration Resolution
**Requirement:** Create a config resolver function that resolves values with the following precedence: CLI argument > Environment variable > Default fallback.

<details>
<summary>Show Solution</summary>

```python
import os
from typing import Optional, Any

def resolve_config(cli_val: Optional[Any], env_key: str, default: Any) -> Any:
    if cli_val is not None:
        return cli_val
    env_val = os.getenv(env_key)
    if env_val is not None:
        return env_val
    return default

# Tests
os.environ["HOST"] = "env.company.internal"
print("CLI Overrides:", resolve_config("cli.host.net", "HOST", "localhost"))
print("Env Used:", resolve_config(None, "HOST", "localhost"))
print("Default Used:", resolve_config(None, "MISSING_HOST", "localhost"))
```
</details>

### Exercise 3: Environment Variable Masker
**Requirement:** Implement a function `sanitize_environment() -> dict[str, str]` that returns a copy of `os.environ` where values for keys containing `"KEY"`, `"SECRET"`, `"PASSWORD"`, or `"TOKEN"` are replaced with `"[REDACTED]"`.

<details>
<summary>Show Solution</summary>

```python
import os

def sanitize_environment() -> dict[str, str]:
    sensitive_keywords = {"KEY", "SECRET", "PASSWORD", "TOKEN", "PASS", "AUTH"}
    sanitized = {}
    for k, v in os.environ.items():
        if any(keyword in k.upper() for keyword in sensitive_keywords):
            sanitized[k] = "[REDACTED]"
        else:
            sanitized[k] = v
    return sanitized

# Test
os.environ["DATABASE_PASSWORD"] = "secret123"
os.environ["PUBLIC_HOST"] = "localhost"
cleaned = sanitize_environment()
print("DB Pass:", cleaned["DATABASE_PASSWORD"])
print("Public Host:", cleaned["PUBLIC_HOST"])
assert cleaned["DATABASE_PASSWORD"] == "[REDACTED]"
```
</details>

### Exercise 4: Prefix-Filtered Environment Extractor
**Requirement:** Write a function `extract_prefixed_env(prefix: str) -> dict[str, str]` that extracts all environment variables starting with `prefix`, stripping the prefix and converting keys to lowercase.

<details>
<summary>Show Solution</summary>

```python
import os

def extract_prefixed_env(prefix: str) -> dict[str, str]:
    result = {}
    p_len = len(prefix)
    for k, v in os.environ.items():
        if k.startswith(prefix):
            sub_key = k[p_len:].lower().lstrip("_")
            result[sub_key] = v
    return result

# Test
os.environ["APP_DATABASE_URL"] = "postgres://..."
os.environ["APP_REDIS_HOST"] = "redis://..."
os.environ["OTHER_VAR"] = "ignore"

extracted = extract_prefixed_env("APP_")
print("Extracted App Config:", extracted)
assert "database_url" in extracted
assert "redis_host" in extracted
assert "other_var" not in extracted
```
</details>

### Exercise 5: Dotenv Writer
**Requirement:** Write a function `dump_dotenv(filepath: Path, env_dict: dict[str, str])` that exports a dictionary into a clean `.env` formatted file, quoting values containing spaces or `#`.

<details>
<summary>Show Solution</summary>

```python
from pathlib import Path

def dump_dotenv(filepath: Path, env_dict: dict[str, str]) -> None:
    lines = []
    for k, v in sorted(env_dict.items()):
        if any(c in v for c in (" ", "#", '"', "'", "\n")):
            escaped = v.replace('"', '\\"')
            lines.append(f'{k}="{escaped}"')
        else:
            lines.append(f"{k}={v}")
    filepath.write_text("\n".join(lines) + "\n", encoding="utf-8")

# Test
import tempfile
with tempfile.TemporaryDirectory() as tmp:
    p = Path(tmp) / ".env"
    dump_dotenv(p, {"HOST": "127.0.0.1", "MOTD": "Welcome to server #1"})
    print("Exported .env:\n" + p.read_text())
```
</details>

### Exercise 6: Ephemeral Environment Variable Context Manager
**Requirement:** Create a context manager `temporary_env(**kwargs)` that temporarily sets or overrides environment variables inside a block, restoring the original environment state upon exit.

<details>
<summary>Show Solution</summary>

```python
import os
import contextlib

@contextlib.contextmanager
def temporary_env(**kwargs):
    old_values = {}
    for k, v in kwargs.items():
        old_values[k] = os.environ.get(k)
        if v is None:
            os.environ.pop(k, None)
        else:
            os.environ[k] = str(v)
    try:
        yield
    finally:
        for k, old_v in old_values.items():
            if old_v is None:
                os.environ.pop(k, None)
            else:
                os.environ[k] = old_v

# Test
os.environ["MY_FLAG"] = "original"
with temporary_env(MY_FLAG="temporary", NEW_FLAG="spawned"):
    print("Inside block:", os.getenv("MY_FLAG"), os.getenv("NEW_FLAG"))
    assert os.getenv("MY_FLAG") == "temporary"

print("Outside block:", os.getenv("MY_FLAG"), os.getenv("NEW_FLAG"))
assert os.getenv("MY_FLAG") == "original"
assert os.getenv("NEW_FLAG") is None
```
</details>
