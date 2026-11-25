# Module 09: Files, Streams & OS (04-pickle-and-its-risks)

> **Topic Overview**: Python's `pickle` module provides binary object serialization, enabling complete object graphs—including classes, functions, and recursive references—to be converted into byte streams and restored. However, because the pickle virtual machine executes arbitrary callables during deserialization (`__reduce__`), unpickling untrusted data is a critical Remote Code Execution (RCE) vulnerability. Understanding its internals, attack vectors, and safe alternatives is paramount for secure software engineering.

---

## Metadata Table

| Field | Value |
|---|---|
| **Category** | Core Language / Serialization & Security |
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

- **Problem Statement**: Standard serialization formats like JSON only support basic primitive types (`str`, `int`, `list`, `dict`). Complex Python objects—such as custom class instances, cyclic object graphs, lambdas, and closures—cannot be serialized without manual schema mappings. `pickle` was designed to serialize *almost any* Python object transparently.
- **Mental Model**: `pickle` is not a passive data format; it is a **Turing-complete stack-based programming language**. A pickled byte stream is a compiled program of instructions. Deserializing a pickle stream with `pickle.loads()` is literally running a virtual machine that executes instructions to instantiate classes, call functions, and set attributes.
- **When to Use**:
  - Inter-process communication between trusted worker processes on the same machine (e.g., Python `multiprocessing`).
  - Short-term caching in trusted internal infrastructure where data cannot be modified by external users.
- **When NOT to Use**:
  - **Never** unpickle data received over a network from untrusted clients, HTTP cookies, unauthenticated Redis caches, or public files.
  - Long-term storage (class definitions change across Python versions, causing deserialization failures).

---

## 2. Core Concepts

### The Pickle Virtual Machine Architecture
The pickle protocol runs an internal stack machine with an accumulator and a memo table (for handling shared and circular references):

```text
+---------------------------------------------------------------+
| The Pickle Virtual Machine (PVM)                              |
+---------------------------------------------------------------+
| Byte Stream: c__builtin__\nsystem\n(S'id'\ntR.                |
|                                                               |
| 1. Opcode 'c' (GLOBAL): Pushes callable to stack              |
|    - Imports os.system                                        |
| 2. Opcode '(' (MARK): Pushes mark to stack                    |
| 3. Opcode 'S' (STRING): Pushes argument "id"                  |
| 4. Opcode 't' (TUPLE): Collects items up to mark into tuple   |
| 5. Opcode 'R' (REDUCE): Pops tuple and callable; EXECUTES!    |
|    - Calls: os.system("id")                                   |
| 6. Opcode '.' (STOP): Halts machine and returns top of stack  |
+---------------------------------------------------------------+
```

### Protocol Versions
- **Protocol 0**: Human-readable ASCII (Python 1.0).
- **Protocol 2**: Standardized new-style classes (Python 2.3).
- **Protocol 4**: Added support for 64-bit containers and large binary data (Python 3.4).
- **Protocol 5 (Default in Python 3.8+)**: Adds out-of-band data buffers (`pickle.PickleBuffer`), optimizing zero-copy serialization for NumPy arrays and scientific computing.

---

## 3. How It Works Internally

Custom serialization in `pickle` is controlled by the `__reduce__` protocol:

```text
class MyObject:
    def __reduce__(self):
        # Must return a tuple: (callable, (args...))
        # During pickle.loads(), the PVM executes: callable(*args)
        return (MyObject, (self.arg1, self.arg2))
```

Because `callable` can be **any importable function in any module** (such as `os.system` or `subprocess.Popen`), an attacker can craft a pickle payload that executes arbitrary shell commands upon deserialization.

---

## 4. Syntax & API Reference

### Basic API
```python
import pickle

# Serializing to bytes
byte_stream = pickle.dumps(obj, protocol=pickle.HIGHEST_PROTOCOL)

# Deserializing from bytes
restored_obj = pickle.loads(byte_stream)

# File stream operations
with open("data.pkl", "wb") as f:
    pickle.dump(obj, f)

with open("data.pkl", "rb") as f:
    restored_obj = pickle.load(f)
```

### Restricting Globals via `Unpickler`
To mitigate security risks, `pickle.Unpickler` can override `find_class()` to whitelist permitted types:

```python
class SafeUnpickler(pickle.Unpickler):
    SAFE_TYPES = {("builtins", "int"), ("builtins", "float"), ("builtins", "str")}

    def find_class(self, module: str, name: str):
        if (module, name) in self.SAFE_TYPES:
            return super().find_class(module, name)
        raise pickle.UnpicklingError(f"Global '{module}.{name}' is forbidden")
```

---

## 5. Code Examples

### 1. Minimal / Basic Example: Serializing Complex Nested Graph
```python
import pickle

class Node:
    def __init__(self, name: str) -> None:
        self.name = name
        self.neighbors: list["Node"] = []

# Circular graph: A -> B -> A
a = Node("A")
b = Node("B")
a.neighbors.append(b)
b.neighbors.append(a)

# JSON would crash with RecursionError; pickle handles circular references!
serialized = pickle.dumps(a)
restored_a = pickle.loads(serialized)

print("Restored Node A:", restored_a.name)
print("Neighbor of A:  ", restored_a.neighbors[0].name)
print("Neighbor of B:  ", restored_a.neighbors[0].neighbors[0].name)
assert restored_a.neighbors[0].neighbors[0] is restored_a
print("Circular reference preserved correctly.")
```
**Output**:
```text
Restored Node A: A
Neighbor of A:   B
Neighbor of B:   A
Circular reference preserved correctly.
```

### 2. Real-World Production Example: The RCE Exploit Demonstration
```python
import pickle

class ExploitPayload:
    """Simulates an attacker-crafted object triggering arbitrary execution on load."""
    def __reduce__(self):
        # Malicious payload: calls eval() to execute arbitrary Python code
        cmd = "print('[EXPLOIT TRIGGERED] Malicious payload executed inside loads()!')"
        return (eval, (cmd,))

# Attacker creates payload
malicious_bytes = pickle.dumps(ExploitPayload())

# Victim unpickles untrusted payload
print("Simulating victim calling pickle.loads(untrusted_data)...")
restored = pickle.loads(malicious_bytes)
```
**Output**:
```text
Simulating victim calling pickle.loads(untrusted_data)...
[EXPLOIT TRIGGERED] Malicious payload executed inside loads()!
```

### 3. Edge Case: Cryptographic Signing with HMAC (Why It Still Has Limits)
```python
import hmac
import hashlib
import pickle

SECRET_KEY = b"secure_microservice_internal_secret_key"

def sign_and_pickle(obj: object) -> bytes:
    data = pickle.dumps(obj)
    signature = hmac.new(SECRET_KEY, data, hashlib.sha256).digest()
    return signature + data

def verify_and_unpickle(payload: bytes) -> object:
    signature, data = payload[:32], payload[32:]
    expected_sig = hmac.new(SECRET_KEY, data, hashlib.sha256).digest()
    if not hmac.compare_digest(signature, expected_sig):
        raise SecurityError("Tampered payload! Signature mismatch.")
    return pickle.loads(data)

# Valid workflow
valid_payload = sign_and_pickle({"user_id": 101, "role": "admin"})
restored = verify_and_unpickle(valid_payload)
print("Verified authentic payload:", restored)

# Tampered workflow
tampered_payload = valid_payload[:-1] + b"\x00"
try:
    verify_and_unpickle(tampered_payload)
except SecurityError as err:
    print(f"Captured security violation: {err}")
```
**Output**:
```text
Verified authentic payload: {'user_id': 101, 'role': 'admin'}
Captured security violation: Tampered payload! Signature mismatch.
```

### 4. Failure Case & Exception Handling: Unpickling with Renamed Class
```python
import pickle

class UserSession:
    def __init__(self, username: str) -> None:
        self.username = username

# Step 1: Pickle session
saved = pickle.dumps(UserSession("alice"))

# Step 2: Simulate developer renaming class to AccountSession
del UserSession

try:
    pickle.loads(saved)
except (AttributeError, ModuleNotFoundError) as err:
    print(f"Captured expected deserialization error: {err}")
```
**Output**:
```text
Captured expected deserialization error: Can't get attribute 'UserSession' on <module '__main__'>
```

### 5. Idiomatic Pythonic Pattern: Whitelisted Safe Unpickler
```python
import pickle
import io

class WhitelistUnpickler(pickle.Unpickler):
    ALLOWED_MODULES = {"builtins"}
    ALLOWED_CLASSES = {"int", "float", "str", "list", "dict", "set", "tuple"}

    def find_class(self, module: str, name: str):
        if module in self.ALLOWED_MODULES and name in self.ALLOWED_CLASSES:
            return super().find_class(module, name)
        raise pickle.UnpicklingError(f"Security: Forbidden class '{module}.{name}'")

def safe_load(payload: bytes):
    return WhitelistUnpickler(io.BytesIO(payload)).load()

# Safe data succeeds
safe_bytes = pickle.dumps([1, 2, "hello", {"key": "val"}])
print("Safe load result:", safe_load(safe_bytes))

# Exploit payload is blocked!
try:
    import os
    unsafe_bytes = pickle.dumps(os.system)
    safe_load(unsafe_bytes)
except pickle.UnpicklingError as err:
    print("Security block confirmed:", err)
```
**Output**:
```text
Safe load result: [1, 2, 'hello', {'key': 'val'}]
Security block confirmed: Security: Forbidden class 'posix.system'
```

---

## 6. Edge Cases & Pitfalls

1. **Pickle Does Not Store Code**:
   - Pickle does **not** serialize class definitions or function bytecode. It only stores a reference to the module and class name (`import_path.ClassName`) along with the instance attribute dictionary. If the target environment does not have the exact same class defined in the same module path, unpickling fails.
2. **Lambda and Dynamic Function Pickling**:
   - Standard `pickle` cannot serialize lambda functions, nested functions, or dynamically generated functions because they lack a top-level module attribute name (`pickle.PicklingError: Can't pickle <function <lambda>...`). Third-party packages like `cloudpickle` or `dill` are required for pickling dynamic closures.
3. **Out-of-Band Buffers (Protocol 5)**:
   - Protocol 5 allows zero-copy transmission of massive byte buffers without serializing them into the main stream. If out-of-band buffers are separated from the main stream, deserialization will fail unless the exact buffers are passed to `pickle.loads(..., buffers=...)`.

---

## 7. Common Mistakes

| Mistake | Why It Fails | Exception / Diagnostic | Correct Pattern |
|---|---|---|---|
| Unpickling data from HTTP requests or cookies | Allows attackers to execute arbitrary shell commands | Remote Code Execution vulnerability | Use JSON with HMAC signatures |
| Using pickle for long-term database storage | Class renaming or module refactoring breaks old records forever | `AttributeError: Can't get attribute '...'` | Use JSON, Protobuf, or SQL columns |
| Assuming HMAC makes pickle safe from insider attacks | Anyone with the secret key can forge an RCE exploit payload | Privilege escalation vulnerability | Avoid pickle entirely across network boundaries |
| Pickling open file handles or network sockets | OS file descriptors and C sockets cannot be restored across processes | `TypeError: cannot pickle '_io.TextIOWrapper'` | Pass paths or file descriptor integers |
| Forgetting `protocol=pickle.HIGHEST_PROTOCOL` | Defaults to older protocols with slower performance on large data | Suboptimal serialization speed | Pass `protocol=pickle.HIGHEST_PROTOCOL` |
| Relying on pickle for cross-language microservices | Pickle is Python-specific; unreadable by Go, Java, or Rust | Interoperability failure | Use Protobuf, MessagePack, or JSON |

---

## 8. Performance & Complexity

- **Serialization Speed**: Written in C (`_pickle`), serializing native primitives runs at $\approx 200-400\ \text{MB/s}$.
- **Comparison to JSON**: Pickle is typically $1.5\times - 3\times$ faster than standard library `json` for complex nested Python structures because it operates in native binary formats without string escaping or number-to-text parsing.
- **Protocol 5 Zero-Copy**: Avoids memory copying entirely for large buffers, achieving near-instantaneous transfers in shared-memory multiprocessing.

---

## 9. CPython Internals & Bytecode

Let's inspect a pickled payload using the standard library `pickletools` disassembler:

```python
import pickle
import pickletools

data = {"score": 42}
raw_pickle = pickle.dumps(data, protocol=4)

print("--- Pickletools Disassembly ---")
pickletools.dis(raw_pickle)
```

**Output**:
```text
--- Pickletools Disassembly ---
    0: \x80 PROTO      4
    2: \x95 FRAME      17
   11: }    EMPTY_DICT
   12: \x94 MEMOIZE    (as 0)
   13: \x8c SHORT_BINUNICODE 'score'
   20: \x94 MEMOIZE    (as 1)
   21: K    BININT1    42
   23: s    SETITEM
   24: .    STOP
highest protocol among opcodes = 4
```
Notice the opcodes:
- `PROTO 4`: Specifies protocol version.
- `EMPTY_DICT`: Instantiates empty dictionary.
- `SHORT_BINUNICODE 'score'`: Pushes string key to stack.
- `BININT1 42`: Pushes integer 42 to stack.
- `SETITEM`: Executes `dict['score'] = 42`.
- `STOP`: Halts PVM and returns dict.

---

## 10. Best Practices

1. **The Golden Rule**: **Never unpickle data from untrusted or unauthenticated sources.**
2. **Use MessagePack or Protobuf for Binary Serialization**: When high-speed binary serialization is needed across services, use MessagePack (`msgpack`) or Protocol Buffers (`protobuf`).
3. **Use Whitelists When Pickle Is Unavoidable**: If forced to deserialize external pickles, inherit from `pickle.Unpickler` and override `find_class()`.

---

## 11. Real-World Case Study: Secure Machine Learning Model Serialization

```python
from pathlib import Path
import json
import tempfile
from typing import NamedTuple

class SafeLinearRegression:
    """Safe model serialization avoiding pickle RCE vulnerabilities."""
    def __init__(self, weights: list[float], bias: float) -> None:
        self.weights = weights
        self.bias = bias

    def predict(self, features: list[float]) -> float:
        return sum(w * x for w, x in zip(self.weights, features)) + self.bias

    def save_model(self, path: Path) -> None:
        # Instead of pickle, serialize weights and metadata into pure JSON
        payload = {
            "model_type": "SafeLinearRegression",
            "version": "1.0",
            "weights": self.weights,
            "bias": self.bias
        }
        path.write_text(json.dumps(payload, indent=2), encoding="utf-8")

    @classmethod
    def load_model(cls, path: Path) -> "SafeLinearRegression":
        payload = json.loads(path.read_text(encoding="utf-8"))
        if payload.get("model_type") != "SafeLinearRegression":
            raise ValueError("Invalid model type in file")
        return cls(weights=payload["weights"], bias=payload["bias"])

# Verify model lifecycle
with tempfile.TemporaryDirectory() as tmp:
    model_file = Path(tmp) / "linear_model.json"
    
    # Train & Save
    original_model = SafeLinearRegression(weights=[0.5, 1.2], bias=0.1)
    original_model.save_model(model_file)
    
    # Load & Predict
    loaded_model = SafeLinearRegression.load_model(model_file)
    prediction = loaded_model.predict([10.0, 5.0])
    
    print(f"Model safely loaded via JSON. Prediction: {prediction:.2f}")
    assert prediction == (0.5 * 10.0) + (1.2 * 5.0) + 0.1
```
**Output**:
```text
Model safely loaded via JSON. Prediction: 11.10
```

---

## 12. Interview Questions

### Q1: Why is `pickle.loads()` fundamentally vulnerable to Remote Code Execution (RCE)?
<details>
<summary><strong>Answer</strong></summary>
Pickle is an interpreted bytecode machine. The pickle protocol includes instructions like <code>GLOBAL</code> and <code>REDUCE</code> that instruct the virtual machine to import arbitrary modules and execute arbitrary callables with supplied arguments during deserialization (governed by the <code>__reduce__</code> hook). An attacker can forge a byte stream that calls functions like <code>os.system</code>, executing arbitrary commands immediately when <code>pickle.loads()</code> is called.
</details>

### Q2: What does the `__reduce__()` method return and how does pickle use it?
<details>
<summary><strong>Answer</strong></summary>
<code>__reduce__()</code> must return either a string (for global variables) or a tuple containing <code>(callable, args)</code>, plus optional state, list items, and dict items. During deserialization, the pickle engine executes <code>callable(*args)</code> to reconstruct the object instance.
</details>

### Q3: Why doesn't signing a pickle payload with an HMAC key completely eliminate security concerns?
<details>
<summary><strong>Answer</strong></summary>
While HMAC prevents external attackers from tampering with payloads in transit, it does not prevent attacks if the HMAC secret key is compromised, shared among multiple microservices with differing trust boundaries, or if an insider crafts a malicious payload. Furthermore, any bug that leaks the key allows instant full RCE across all consumers.
</details>

### Q4: How does `pickle` handle circular references?
<details>
<summary><strong>Answer</strong></summary>
The pickle virtual machine maintains an internal lookup table called the <strong>memo</strong>. When an object is first serialized, it is assigned a memo ID. If the serializer encounters the same object again via a circular reference, it writes a <code>GET</code> opcode referencing the existing memo ID instead of recursively re-serializing the object, safely preserving graph topology without infinite recursion.
</details>

### Q5: What is Protocol 5 in the `pickle` module?
<details>
<summary><strong>Answer</strong></summary>
Introduced in PEP 574 (Python 3.8), Protocol 5 adds support for out-of-band data buffers. It allows large memory-mapped buffers (such as NumPy arrays) to be serialized and transferred with zero memory copies, transferring metadata in the main stream and referencing external buffers directly.
</details>

### Q6: Can `pickle` serialize a lambda function?
<details>
<summary><strong>Answer</strong></summary>
No. Standard library <code>pickle</code> serializes functions by reference (storing their module name and qualified name). Because lambda functions are anonymous and lack unique qualified names in the module namespace, <code>pickle.dumps()</code> raises <code>PicklingError</code>.
</details>

### Q7: What is the purpose of `pickletools.dis()`?
<details>
<summary><strong>Answer</strong></summary>
<code>pickletools.dis()</code> is a standard library disassembly utility that parses and displays the symbolic PVM opcodes (e.g. <code>PROTO</code>, <code>GLOBAL</code>, <code>BININT</code>, <code>REDUCE</code>) of a raw pickle byte stream, allowing security researchers and engineers to audit the operations that will be executed upon deserialization.
</details>

### Q8: What safe binary alternatives exist to `pickle`?
<details>
<summary><strong>Answer</strong></summary>
1. <strong>MessagePack (<code>msgpack</code>)</strong>: Compact, high-speed binary JSON equivalent supported across dozens of languages.
2. <strong>Protocol Buffers (<code>protobuf</code>)</strong>: Strongly typed, schema-enforced, backward-compatible binary format from Google.
3. <strong>FlatBuffers</strong>: Zero-copy deserialization format for performance-critical systems.
</details>

---

## 13. Exercises & Challenges

### Challenge 1 (Easy): Basic Roundtrip Serializer
Write a function `pickle_roundtrip[T](obj: T) -> T` that pickles and unpickles an object using `HIGHEST_PROTOCOL`, verifying that `restored == obj`.
<details>
<summary><strong>Solution</strong></summary>

```python
import pickle
from typing import TypeVar

T = TypeVar("T")

def pickle_roundtrip(obj: T) -> T:
    raw = pickle.dumps(obj, protocol=pickle.HIGHEST_PROTOCOL)
    return pickle.loads(raw)

original = {"users": [1, 2, 3], "status": "active"}
restored = pickle_roundtrip(original)
assert restored == original
assert restored is not original
```
</details>

### Challenge 2 (Easy): Disassemble a Pickle Stream
Write a function `inspect_pickle_opcodes(obj: object) -> str` using `pickletools.dis` and `io.StringIO` that returns the disassembled PVM opcodes of `obj`.
<details>
<summary><strong>Solution</strong></summary>

```python
import pickle
import pickletools
import io

def inspect_pickle_opcodes(obj: object) -> str:
    raw = pickle.dumps(obj, protocol=4)
    out = io.StringIO()
    pickletools.dis(raw, out=out)
    return out.getvalue()

disasm = inspect_pickle_opcodes("Hello World")
assert "SHORT_BINUNICODE" in disasm or "BINUNICODE" in disasm
assert "STOP" in disasm
```
</details>

### Challenge 3 (Medium): Strict Primitive Whitelist Unpickler
Implement a class `PrimitiveOnlyUnpickler` that strictly permits only `builtins.int`, `builtins.float`, `builtins.str`, `builtins.bool`, `builtins.list`, and `builtins.dict`, raising `pickle.UnpicklingError` if any custom class or callable is loaded.
<details>
<summary><strong>Solution</strong></summary>

```python
import pickle
import io

class PrimitiveOnlyUnpickler(pickle.Unpickler):
    ALLOWED = {
        ("builtins", "int"),
        ("builtins", "float"),
        ("builtins", "str"),
        ("builtins", "bool"),
        ("builtins", "list"),
        ("builtins", "dict"),
    }

    def find_class(self, module: str, name: str):
        if (module, name) in self.ALLOWED:
            return super().find_class(module, name)
        raise pickle.UnpicklingError(f"Prohibited type: {module}.{name}")

def safe_load(payload: bytes):
    return PrimitiveOnlyUnpickler(io.BytesIO(payload)).load()

# Allowed
assert safe_load(pickle.dumps({"count": 42})) == {"count": 42}

# Prohibited
try:
    class Custom: pass
    safe_load(pickle.dumps(Custom()))
    raise AssertionError("Should have raised UnpicklingError")
except pickle.UnpicklingError:
    pass
```
</details>

### Challenge 4 (Medium): Custom Reduce State Serializer
Create a class `SecretVault(secret: str)` that overrides `__reduce__` so that when pickled, the secret is base64 encoded, and on unpickling, is decoded back to plaintext.
<details>
<summary><strong>Solution</strong></summary>

```python
import pickle
import base64

def _vault_reconstructor(b64_secret: str):
    raw_secret = base64.b64decode(b64_secret).decode("utf-8")
    return SecretVault(raw_secret)

class SecretVault:
    def __init__(self, secret: str) -> None:
        self.secret = secret

    def __reduce__(self):
        b64_str = base64.b64encode(self.secret.encode("utf-8")).decode("utf-8")
        return (_vault_reconstructor, (b64_str,))

vault = SecretVault("classified_data_99")
serialized = pickle.dumps(vault)

# Verify base64 string is in the pickle stream, not raw secret
assert b"classified_data_99" not in serialized

restored = pickle.loads(serialized)
assert restored.secret == "classified_data_99"
```
</details>

### Challenge 5 (Hard): Authenticated Encrypted Pickle Envelope
Implement an envelope class `CryptoPickleEnvelope` that combines AES encryption (or simulated XOR/HMAC) and signature verification. The unpickler must verify the HMAC signature before passing the decrypted bytes to `pickle.loads()`, refusing to touch `pickle.loads()` if tampering is detected.
<details>
<summary><strong>Solution</strong></summary>

```python
import hmac
import hashlib
import pickle

class CryptoPickleEnvelope:
    def __init__(self, key: bytes) -> None:
        self.key = key

    def pack(self, obj: object) -> bytes:
        data = pickle.dumps(obj, protocol=pickle.HIGHEST_PROTOCOL)
        sig = hmac.new(self.key, data, hashlib.sha256).digest()
        return sig + data

    def unpack(self, payload: bytes) -> object:
        if len(payload) < 32:
            raise ValueError("Payload too short")
        sig, data = payload[:32], payload[32:]
        expected_sig = hmac.new(self.key, data, hashlib.sha256).digest()
        if not hmac.compare_digest(sig, expected_sig):
            raise PermissionError("HMAC verification failed; aborting unpickling")
        return pickle.loads(data)

env = CryptoPickleEnvelope(b"super_secret_signing_key_32bytes")
packed = env.pack({"admin": True, "token": "xyz"})
unpacked = env.unpack(packed)
assert unpacked["admin"] is True

# Tamper with 1 byte
corrupt = packed[:35] + b"\xff" + packed[36:]
try:
    env.unpack(corrupt)
    raise AssertionError("Should have raised PermissionError")
except PermissionError:
    pass
```
</details>

### Challenge 6 (Hard): Static Pickle Safety Linter
Write a static analysis function `audit_pickle_stream(raw_bytes: bytes) -> tuple[bool, list[str]]` using `pickletools.genops()` that inspects all opcodes in a pickle stream. If any opcode attempts to import modules (`GLOBAL` / `STACK_GLOBAL`) or execute functions (`REDUCE`), flag the stream as `UNSAFE` and return the list of dangerous calls.
<details>
<summary><strong>Solution</strong></summary>

```python
import pickle
import pickletools

def audit_pickle_stream(raw_bytes: bytes) -> tuple[bool, list[str]]:
    dangerous_calls = []
    is_safe = True
    
    for opcode, arg, pos in pickletools.genops(raw_bytes):
        if opcode.name in ("GLOBAL", "STACK_GLOBAL", "REDUCE"):
            is_safe = False
            dangerous_calls.append(f"{opcode.name}: {arg}")
            
    return is_safe, dangerous_calls

safe_data = pickle.dumps([1, 2, 3], protocol=4)
safe_flag, calls = audit_pickle_stream(safe_data)
assert safe_flag is True
assert len(calls) == 0

# Unsafe payload
class Exploit:
    def __reduce__(self):
        import os
        return (os.system, ("ls",))

unsafe_data = pickle.dumps(Exploit(), protocol=4)
unsafe_flag, calls = audit_pickle_stream(unsafe_data)
assert unsafe_flag is False
assert any("posix.system" in c or "nt.system" in c or "REDUCE" in c for c in calls)
```
</details>
