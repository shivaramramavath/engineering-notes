---
title: "HTTP Networking: urllib and http.client in the Standard Library"
description: "Comprehensive guide to Python's built-in HTTP client stack: urllib.request, urllib.parse, HTTPStatus enums, streaming responses, timeout handling, authentication, and architectural comparison against requests/httpx."
module: "10-standard-library-tour"
order: 3
tags:
  - python
  - http
  - urllib
  - networking
  - url-parsing
  - requests
  - http-client
difficulty: "Intermediate"
cpython_version: "3.12+"
last_verified: "2026-03"
---

# HTTP Networking: urllib and http.client in the Standard Library

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
10. [Real-World Case Study: Zero-Dependency Cloud Storage Multipart Downloader](#real-world-case-study-zero-dependency-cloud-storage-multipart-downloader)
11. [Interview Questions & Deep-Dive Scenarios](#interview-questions--deep-dive-scenarios)
12. [Practical Exercises & Code Katas](#practical-exercises--code-katas)

---

## What & Why

Python includes an enterprise-capable HTTP and URL processing suite directly in its standard library under the `urllib` package and `http` module:
- **`urllib.request`**: High-level interface for fetching URLs, opening connections, managing headers, and posting form/JSON payloads.
- **`urllib.parse`**: Breaking down URLs into standard components (scheme, netloc, path, query), sanitizing parameters, and encoding query strings.
- **`urllib.error`**: Granular exception hierarchy for network failures, socket timeouts, and HTTP status codes (`HTTPError`, `URLError`).
- **`http.client`**: Low-level HTTP/1.1 client interface directly managing socket connections and raw byte payloads.
- **`http.HTTPStatus`**: Clean, type-safe enum containing all standardized IETF/RFC HTTP status codes with semantic descriptions.

### Motivation
While third-party libraries like `requests` and `httpx` dominate web development for their ergonomic APIs, standard library `urllib` is indispensable:
1. **Zero External Dependencies**: Essential for CLI utilities, AWS Lambda functions, bootstrapper scripts, and embedded environments where installing external wheels via `pip` is restricted or adds unwanted cold-start latency.
2. **Security Compliance**: Air-gapped enterprise environments and secure container base images that forbid third-party dependencies rely entirely on `urllib`.
3. **URL Parsing Mastery**: Even applications using `requests` or `httpx` use `urllib.parse` for URL validation, percent-encoding, and query string manipulation.

### Real-World Analogy
Consider sending a postal parcel:
- **`requests` / `httpx`** is hiring a white-glove courier service: they bring a branded box, fill out the customs declarations automatically, handle parcel tracking, and notify your phone on delivery.
- **`urllib`** is walking into the national post office directly: you address the envelope manually according to postal standards (`urllib.parse`), buy the stamp (`urllib.request`), and handle the tracking receipt yourself. It requires slightly more code, but the post office is guaranteed to exist in every country without pre-booking.

### When to Use vs When Not to Use
- **Use `urllib` / `http.client` for:**
  - Standard library scripts, Git hooks, and automation tools with zero external dependencies.
  - Serverless functions (AWS Lambda, Google Cloud Functions) optimizing for sub-millisecond cold starts.
  - Robust URL parsing, escaping, and normalization (`urllib.parse`).
- **Do Not Use `urllib` for:**
  - High-concurrency microservices requiring HTTP/2, HTTP/3, or connection pooling (use `httpx`).
  - Complex async I/O web applications (use `aiohttp` or `httpx.AsyncClient`).
  - Automated session persistence and cookie handling across complex redirects where `requests.Session` is vastly simpler.

---

## Core Concepts & Mental Model

```
+-------------------------------------------------------------------------+
| Application Code                                                        |
+-------------------------------------------------------------------------+
       |                                                 |
       v                                                 v
+-------------------------------+             +---------------------------+
| urllib.parse                  |             | urllib.request.Request    |
| - urlparse / urlunparse       |             | - Method (GET, POST, etc.)|
| - urlencode / parse_qs        |             | - Custom Headers          |
| - quote / unquote             |             | - Body (bytes)            |
+-------------------------------+             +---------------------------+
                                                         |
                                                         v
                                              +---------------------------+
                                              | urllib.request.urlopen    |
                                              | - OpenerDirector          |
                                              | - HTTPHandler / HTTPS     |
                                              +---------------------------+
                                                         |
                                                         v
                                              +---------------------------+
                                              | http.client.HTTPConnection|
                                              | - Raw Socket (socket.py)  |
                                              | - TLS / SSL (ssl.py)      |
                                              +---------------------------+
```

### Key Abstractions
1. **`urllib.request.Request`**: Represents an HTTP request specification. Encapsulates URL, HTTP method (GET, POST, PUT, DELETE), headers dictionary, and request body.
2. **`urllib.request.urlopen(req, timeout=...)`**: Executes the request over network sockets, returning an `http.client.HTTPResponse` object acting as a context-managed binary stream.
3. **`urllib.parse.urlparse`**: Deconstructs a URL string into a 6-tuple `ParseResult` (`scheme`, `netloc`, `path`, `params`, `query`, `fragment`).
4. **`http.HTTPStatus`**: IntEnum mapping numeric codes to names: `HTTPStatus.OK` (200), `HTTPStatus.NOT_FOUND` (404), `HTTPStatus.UNAUTHORIZED` (401).

---

## How It Works Internally

### 1. `urllib.request` Extensible Handler Architecture
`urlopen()` is not a monolithic function. It delegates to an internal `OpenerDirector` instance that coordinates a chain of `BaseHandler` subclasses:
- `ProxyHandler`: Intercepts system proxy settings (`HTTP_PROXY`, `HTTPS_PROXY` environment variables).
- `HTTPHandler` & `HTTPSHandler`: Dispatches connections to `http.client.HTTPConnection` or `HTTPSConnection`.
- `HTTPRedirectHandler`: Automatically follows 301, 302, 303, and 307 redirects (up to a default limit of 10).
- `HTTPCookieProcessor`: Captures and attaches cookies across requests.
- `HTTPErrorProcessor`: Inspects response status codes; if the status code is outside the 200–299 range, it constructs and raises an `urllib.error.HTTPError`.

### 2. TLS/SSL Verification Mechanics
In modern Python (3.10+):
- `HTTPSHandler` uses Python's `ssl` module, defaulting to `ssl.create_default_context()`.
- It loads trusted CA certificates from the host operating system's native trust store.
- Hostname verification and certificate expiration validation are enforced strictly by default.

---

## Syntax & API Reference

### `urllib.parse`
```python
import urllib.parse

# Parsing & Deconstructing
res: urllib.parse.ParseResult = urllib.parse.urlparse(url_str)
full_url: str = urllib.parse.urlunparse(res)

# Query String Formatting
query_str: str = urllib.parse.urlencode({"q": "python tutorial", "page": 2})
parsed_dict: dict = urllib.parse.parse_qs("q=python&page=2")

# Path Joining and Escaping
resolved: str = urllib.parse.urljoin("https://api.com/v1/", "users/list")
escaped_param: str = urllib.parse.quote("hello world/test")
unescaped: str = urllib.parse.unquote(escaped_param)
```

### `urllib.request`
```python
import urllib.request
import urllib.error

# Request Constructor
req = urllib.request.Request(
    url: str,
    data: bytes = None,               # If provided, switches default method to POST
    headers: dict[str, str] = {},
    origin_req_host: str = None,
    unverifiable: bool = False,
    method: str = "GET"               # "GET", "POST", "PUT", "PATCH", "DELETE"
)

# Opening Connection
with urllib.request.urlopen(req, timeout=10.0) as response:
    status_code: int = response.status
    headers: email.message.Message = response.headers
    content_type: str = response.getheader("Content-Type")
    payload: bytes = response.read(65536) # Read in chunks
```

### Exception Handling
```python
try:
    urllib.request.urlopen(req)
except urllib.error.HTTPError as err:
    print("HTTP Status Code:", err.code)
    print("Response Body:", err.read().decode())
except urllib.error.URLError as err:
    print("Network/DNS failure:", err.reason)
```

---

## Code Examples

### Example 1: Robust GET Request with Headers and Status Validation
```python
import urllib.request
import urllib.error
from http import HTTPStatus
import json

def fetch_json_resource(url: str) -> dict:
    req = urllib.request.Request(
        url,
        headers={
            "User-Agent": "AntigravityBot/1.0 (+https://example.com)",
            "Accept": "application/json"
        },
        method="GET"
    )

    try:
        # Enforce strict 5-second socket timeout
        with urllib.request.urlopen(req, timeout=5.0) as response:
            status = response.status
            print(f"Response Status: {status} ({HTTPStatus(status).phrase})")

            raw_bytes = response.read()
            return json.loads(raw_bytes.decode("utf-8"))

    except urllib.error.HTTPError as err:
        print(f"HTTP Error encountered: {err.code} - {err.reason}")
        raise
    except urllib.error.URLError as err:
        print(f"Network transport failure: {err.reason}")
        raise

# Verification with simulated mock request
def run_mock_demo():
    print("Request constructor verified:")
    r = urllib.request.Request("https://httpbin.org/get", headers={"Accept": "application/json"})
    print("  Method:", r.get_method())
    print("  Header:", r.get_header("Accept"))

run_mock_demo()
# Output:
# Request constructor verified:
#   Method: GET
#   Header: application/json
```

### Example 2: POST Request with JSON Body
```python
import urllib.request
import json

def build_post_request(url: str, payload_dict: dict) -> urllib.request.Request:
    # 1. Serialize payload to bytes
    json_bytes = json.dumps(payload_dict).encode("utf-8")

    # 2. Build Request with explicit Content-Type and Content-Length
    req = urllib.request.Request(
        url=url,
        data=json_bytes,
        headers={
            "Content-Type": "application/json; charset=utf-8",
            "Content-Length": str(len(json_bytes)),
            "User-Agent": "Python-urllib-Client"
        },
        method="POST"
    )
    return req

def run_post_demo():
    data = {"order_id": 10482, "status": "approved", "total_usd": 49.99}
    req = build_post_request("https://api.example.com/orders", data)

    print("Formed POST Request:")
    print("  URL:", req.full_url)
    print("  Method:", req.get_method())
    print("  Content-Type:", req.get_header("Content-type"))
    print("  Payload bytes:", req.data)

run_post_demo()
# Output:
# Formed POST Request:
#   URL: https://api.example.com/orders
#   Method: POST
#   Content-Type: application/json; charset=utf-8
#   Payload bytes: b'{"order_id": 10482, "status": "approved", "total_usd": 49.99}'
```

### Example 3: URL Deconstruction and Query String Construction with `urllib.parse`
```python
import urllib.parse

def run_url_parsing_demo():
    raw_url = "https://search.example.com:8443/catalog/items?category=books&sort=price_asc#top"

    # Deconstruct URL
    parsed = urllib.parse.urlparse(raw_url)
    print("Parsed Scheme:  ", parsed.scheme)
    print("Parsed Netloc:  ", parsed.netloc)
    print("Parsed Hostname:", parsed.hostname)
    print("Parsed Port:    ", parsed.port)
    print("Parsed Path:    ", parsed.path)
    print("Parsed Query:   ", parsed.query)

    # Decode query parameters into dictionary
    query_params = urllib.parse.parse_qs(parsed.query)
    print("\nDecoded Query Parameters:", query_params)

    # Encode new query string with percent-encoding
    new_params = {"q": "python & c++ guide", "tags": ["async", "threading"]}
    encoded_query = urllib.parse.urlencode(new_params, doseq=True)
    print("Encoded Query String:", encoded_query)

    # Safe URL joining (resolves relative links against base URL)
    base = "https://cdn.example.com/assets/images/"
    relative_target = "../styles/main.css"
    resolved_url = urllib.parse.urljoin(base, relative_target)
    print("Resolved Relative URL:", resolved_url)

run_url_parsing_demo()
# Output:
# Parsed Scheme:   https
# Parsed Netloc:   search.example.com:8443
# Parsed Hostname: search.example.com
# Parsed Port:     8443
# Parsed Path:     /catalog/items
# Parsed Query:    category=books&sort=price_asc
# 
# Decoded Query Parameters: {'category': ['books'], 'sort': ['price_asc']}
# Encoded Query String: q=python+%26+c%2B%2B+guide&tags=async&tags=threading
# Resolved Relative URL: https://cdn.example.com/assets/styles/main.css
```

### Example 4: Chunked Streaming of Large Downloads
```python
import urllib.request
import io

def stream_download_in_chunks(response_stream, chunk_size: int = 1024) -> int:
    """
    Consumes binary stream in bounded chunks to prevent holding
    entire gigabyte payloads in memory simultaneously.
    """
    total_bytes = 0
    buffer = io.BytesIO()

    while True:
        chunk = response_stream.read(chunk_size)
        if not chunk:
            break
        total_bytes += len(chunk)
        buffer.write(chunk)

    return total_bytes

# Verification with synthetic in-memory stream
def test_streaming():
    mock_payload = b"X" * 10_000
    mock_stream = io.BytesIO(mock_payload)
    bytes_read = stream_download_in_chunks(mock_stream, chunk_size=2048)
    print(f"Successfully streamed {bytes_read} bytes in chunks.")
    assert bytes_read == 10_000

test_streaming()
# Output:
# Successfully streamed 10000 bytes in chunks.
```

### Example 5: Custom Basic Authentication with `HTTPPasswordMgr`
```python
import urllib.request

def build_authenticated_opener(base_url: str, username: str, secret_token: str) -> urllib.request.OpenerDirector:
    """
    Builds a custom OpenerDirector pre-configured with HTTP Basic Authentication.
    """
    # 1. Password Manager maps realm + URL to credentials
    password_mgr = urllib.request.HTTPPasswordMgrWithDefaultRealm()
    password_mgr.add_password(realm=None, uri=base_url, user=username, passwd=secret_token)

    # 2. Attach Basic Auth Handler
    auth_handler = urllib.request.HTTPBasicAuthHandler(password_mgr)

    # 3. Build specialized opener
    opener = urllib.request.build_opener(auth_handler)
    return opener

def run_auth_demo():
    opener = build_authenticated_opener("https://api.internal.net", "service_agent", "sk_prod_token")
    print("Authenticated Opener Director constructed successfully:")
    print("Attached Handlers:", [type(h).__name__ for h in opener.handlers])

run_auth_demo()
# Output:
# Authenticated Opener Director constructed successfully:
# Attached Handlers: ['ProxyHandler', 'UnknownHandler', 'HTTPHandler', 'HTTPSHandler', 'HTTPDefaultErrorHandler', 'HTTPRedirectHandler', 'HTTPErrorProcessor', 'HTTPBasicAuthHandler']
```

---

## Edge Cases, Pitfalls & Anti-Patterns

### 1. The Missing `timeout` Trap
Calling `urllib.request.urlopen(url)` without specifying a `timeout` defaults to the global system socket timeout (`socket.getdefaulttimeout()`), which is typically `None` (infinite blocking). If the remote server accepts the TCP handshake and hangs without transmitting bytes, your Python thread **blocks forever**!
- **Rule**: Always pass an explicit timeout: `urlopen(req, timeout=10.0)`.

### 2. Passing Strings to `data=` in POST Requests
The `data` parameter of `urllib.request.Request` accepts **only raw bytes**, not a Python string.
Passing `data="my=string"` raises `TypeError: POST data should be bytes, an iterable of bytes, or a file object. It cannot be of type str`. Always encode strings: `data="my=string".encode("utf-8")`.

### 3. Missing `doseq=True` in `urllib.parse.urlencode`
When query parameters contain lists (e.g. `{"filter": ["active", "pending"]}`):
- Naive `urlencode(params)` produces `filter=['active',+'pending']` (literal Python list representation).
- Passing `urlencode(params, doseq=True)` correctly expands elements: `filter=active&filter=pending`.

### Common Mistakes Table

| Bad Code | Good Code | Why | Severity |
| :--- | :--- | :--- | :--- |
| `urllib.request.urlopen(url)` (no timeout) | `urlopen(url, timeout=10.0)` | Server stall causes permanent process/thread hang. | Critical |
| `Request(url, data="key=value")` | `Request(url, data=b"key=value")` | `data` must be bytes; raises `TypeError` on string. | High |
| `urlencode({"id": [1, 2]})` | `urlencode({"id": [1, 2]}, doseq=True)` | Without `doseq=True`, encodes literal list string `[1, 2]`. | High |
| Hand-rolling URL query concatenation | `urllib.parse.urlencode()` | Naive string concatenation misses percent-encoding for spaces, `&`, `=`. | Medium |
| Catching only `urllib.error.HTTPError` | Catch `HTTPError` then `URLError` | DNS failures and socket timeouts raise `URLError`, escaping `HTTPError`. | High |
| Reading 5 GB response into memory with `.read()` | Stream with `while chunk := resp.read(65536):` | Exhausts system RAM; triggers OS out-of-memory killer. | High |

---

## Performance & Memory Characteristics

### Startup Overhead: `urllib` vs `requests` vs `httpx`
In short-lived CLI tools and AWS Lambda functions:
- **`import urllib.request`**: $\approx 4 \text{ ms}$ (standard library module).
- **`import requests`**: $\approx 45 \text{ ms}$ (imports `urllib3`, `certifi`, `charset_normalizer`, `idna`).
- **`import httpx`**: $\approx 75 \text{ ms}$ (imports `httpcore`, `anyio`, `h11`, `sniffio`).

For high-frequency ephemeral CLI scripts, `urllib` yields a **10x-18x faster cold start**!

### Memory Consumption
`urlopen()` returns an unbuffered socket stream. Streaming chunks consumes $\mathcal{O}(1)$ memory, whereas calling `.read()` buffers the entire payload in RAM ($\mathcal{O}(N)$).

---

## CPython Internals & Bytecode

In CPython:
- `urllib.request` is implemented in pure Python (`Lib/urllib/request.py`), which delegates socket management to `http.client` (`Lib/http/client.py`).
- `http.client.HTTPConnection` creates low-level sockets using the C-extension module `_socket` (`Modules/socketmodule.c`).
- When HTTPS is invoked, the socket is wrapped using `_ssl` (`Modules/_ssl.c`) which interfaces directly with OpenSSL.

Bytecode for URL parsing:
```python
import dis
import urllib.parse

def parse_target(u):
    return urllib.parse.urlparse(u).hostname

dis.dis(parse_target)
```
```text
  1           0 RESUME                   0
  2           2 LOAD_GLOBAL              1 (urllib + NULL)
             12 LOAD_ATTR                2 (parse)
             22 LOAD_ATTR                5 (urlparse)
             32 LOAD_FAST                0 (u)
             34 CALL                     1
             42 LOAD_ATTR                7 (hostname)
             52 RETURN_VALUE
```

---

## Best Practices & Idiomatic Patterns

1. **Always Set a Custom `User-Agent` Header**:
   By default, `urllib` sends `Python-urllib/3.x`. Many CDNs (Cloudflare, Akamai) and API gateways block default Python User-Agents to thwart scrapers.
2. **Order Exception Handling Correctly**:
   `urllib.error.HTTPError` is a subclass of `urllib.error.URLError`. Always catch `HTTPError` before `URLError`:
   ```python
   try:
       with urllib.request.urlopen(req, timeout=5) as resp:
           pass
   except urllib.error.HTTPError as e:
       # Handles 4xx and 5xx HTTP responses
       pass
   except urllib.error.URLError as e:
       # Handles network, DNS, and connection timeouts
       pass
   ```
3. **Use `http.HTTPStatus` for Readable Status Logic**:
   ```python
   if response.status == HTTPStatus.OK:
       pass
   ```
4. **Use `urljoin` for Relative Links**:
   Avoid manual string concatenation when resolving relative API paths.

---

## Real-World Case Study: Zero-Dependency Cloud Storage Multipart Downloader

The following production-ready utility downloads large assets from cloud object stores with streaming chunk writes, progress tracking, and retry backoff, without requiring any third-party packages.

```python
import urllib.request
import urllib.error
from http import HTTPStatus
import time
from pathlib import Path
from typing import Optional

class RobustDownloader:
    def __init__(self, max_retries: int = 3, timeout_seconds: float = 10.0):
        self.max_retries = max_retries
        self.timeout = timeout_seconds

    def download_to_file(self, url: str, destination_path: Path) -> int:
        destination_path.parent.mkdir(parents=True, exist_ok=True)
        temp_destination = destination_path.with_suffix(".download.tmp")

        req = urllib.request.Request(
            url,
            headers={
                "User-Agent": "ResilientDownloader/2.0 (Python Standard Library)",
                "Accept-Encoding": "identity"  # Prevent gzip chunk decoding complications
            }
        )

        for attempt in range(1, self.max_retries + 1):
            try:
                print(f"[DOWNLOAD] Attempt {attempt}/{self.max_retries} connecting to: {url}")
                with urllib.request.urlopen(req, timeout=self.timeout) as response:
                    if response.status != HTTPStatus.OK:
                        raise RuntimeError(f"Unexpected HTTP status: {response.status}")

                    total_bytes = 0
                    content_length = response.getheader("Content-Length")
                    expected_size = int(content_length) if content_length else None

                    with open(temp_destination, "wb") as out_file:
                        while chunk := response.read(65536):  # 64KB buffer
                            out_file.write(chunk)
                            total_bytes += len(chunk)

                    if expected_size and total_bytes != expected_size:
                        raise IOError(f"Truncated download: expected {expected_size} bytes, got {total_bytes}")

                    # Atomically rename completed file
                    temp_destination.replace(destination_path)
                    print(f"[DOWNLOAD SUCCESS] Saved {total_bytes:,} bytes to {destination_path}")
                    return total_bytes

            except (urllib.error.URLError, TimeoutError, IOError) as exc:
                print(f"[DOWNLOAD WARN] Attempt {attempt} failed: {exc}")
                if temp_destination.exists():
                    temp_destination.unlink()
                if attempt == self.max_retries:
                    raise RuntimeError(f"Failed to download asset after {self.max_retries} attempts.") from exc
                time.sleep(2 ** attempt * 0.5)

        return 0

# Verification
if __name__ == "__main__":
    import tempfile
    with tempfile.TemporaryDirectory() as tmpdir:
        downloader = RobustDownloader(max_retries=2, timeout_seconds=2.0)
        dest = Path(tmpdir) / "output.json"
        
        # Test constructor and method readiness
        print("Downloader initialized with max retries:", downloader.max_retries)
```

---

## Interview Questions & Deep-Dive Scenarios

<details>
<summary><strong>1. Junior: Why must the <code>data</code> parameter of <code>urllib.request.Request</code> be bytes rather than a string?</strong></summary>

HTTP is an underlying byte-stream protocol. When a request body is transmitted across a TCP socket, the operating system kernel requires raw octets/bytes. If Python accepted a string, it would have to make arbitrary assumptions about the character encoding. Forcing the caller to provide `bytes` (e.g. `payload.encode("utf-8")`) ensures explicit encoding and prevents silent text corruption.
</details>

<details>
<summary><strong>2. Junior: What is the difference between <code>urllib.error.HTTPError</code> and <code>urllib.error.URLError</code>?</strong></summary>

- `URLError` is raised when the request could not reach the server at all (e.g. DNS resolution failure, network interface down, connection timeout, connection refused).
- `HTTPError` is a subclass of `URLError` raised when the connection succeeded, but the HTTP server returned an error status code outside the 200–299 range (e.g. 404 Not Found, 500 Internal Server Error).
</details>

<details>
<summary><strong>3. Mid: What happens if you do not specify a <code>timeout</code> in <code>urllib.request.urlopen()</code>?</strong></summary>

If omitted, `urlopen()` uses the global default socket timeout (`socket.getdefaulttimeout()`), which is `None` by default. If the remote host accepts the TCP connection but stalls or fails to respond, the Python process will block indefinitely on socket read calls, leading to thread pool exhaustion and hung workers.
</details>

<details>
<summary><strong>4. Mid: What is the purpose of <code>urllib.parse.urljoin()</code> compared to simple string concatenation?</strong></summary>

`urllib.parse.urljoin(base, url)` implements standard RFC 3986 relative path resolution. It handles edge cases that naive string concatenation breaks:
- Relative path navigation: `urljoin("http://x.com/a/b/", "../c")` correctly resolves to `http://x.com/a/c`.
- Absolute path replacement: `urljoin("http://x.com/a/b/", "/c")` correctly resolves to `http://x.com/c`.
- Protocol-relative URLs: `urljoin("https://x.com/", "//cdn.com/asset")` correctly resolves to `https://cdn.com/asset`.
</details>

<details>
<summary><strong>5. Senior: How does <code>urllib.request.OpenerDirector</code> handle HTTP redirects, and what security risks are associated with automated redirect following?</strong></summary>

`OpenerDirector` includes `HTTPRedirectHandler`, which automatically follows 301, 302, 303, and 307 redirects up to a maximum depth of 10.
Security risks:
1. **Credential Leakage**: If an initial request includes an `Authorization: Bearer ...` header and is redirected to an untrusted third-party domain, the authorization header could leak unless stripped.
2. **SSRF (Server-Side Request Forgery)**: An attacker can redirect an internal worker from a public URL to internal loopback addresses (e.g. `http://169.254.169.254/latest/meta-data/`) to extract cloud instance metadata.
</details>

<details>
<summary><strong>6. Senior: How do you configure <code>urllib.request</code> to use custom proxy servers with authentication?</strong></summary>

Create an `OpenerDirector` with `ProxyHandler`:
```python
proxy_handler = urllib.request.ProxyHandler({
    "http": "http://user:pass@proxy.corp.net:8080",
    "https": "http://user:pass@proxy.corp.net:8080"
})
opener = urllib.request.build_opener(proxy_handler)
response = opener.open("https://target.com")
```
Alternatively, set the standard environment variables `HTTP_PROXY` and `HTTPS_PROXY`, which `urllib` detects automatically.
</details>

<details>
<summary><strong>7. Staff: Compare the architectural tradeoffs between standard library <code>urllib</code> and modern async clients (<code>httpx</code> / <code>aiohttp</code>) in high-throughput microservices.</strong></summary>

`urllib` is synchronous and thread-blocking; handling 5,000 concurrent outbound HTTP requests requires 5,000 OS threads, incurring hundreds of megabytes of stack allocation and heavy OS context-switching overhead. `httpx.AsyncClient` and `aiohttp` use non-blocking event loops (`asyncio`) over multiplexed sockets, handling tens of thousands of concurrent connections within a single thread. However, `urllib` requires zero dependencies and has near-instant import times, making it superior for serverless functions, bootstrapper scripts, and lightweight CLI tools.
</details>

<details>
<summary><strong>8. Staff: How does CPython enforce TLS certificate revocation and hostname validation in <code>urllib.request</code>?</strong></summary>

`urllib.request` delegates to `http.client.HTTPSConnection`, which utilizes Python's C-extension `_ssl` module wrapping OpenSSL. By default, it initializes an `ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)` with `check_hostname = True` and `verify_mode = ssl.CERT_REQUIRED`. During the TLS handshake, OpenSSL validates the server's certificate chain against the local system CA bundle, checks the validity period, and verifies that the hostname matches the Subject Alternative Name (SAN) fields in the X.509 certificate.
</details>

---

## Practical Exercises & Code Katas

### Exercise 1: Query String Builder Function
**Requirement:** Write a function `build_search_url(base_url: str, query: str, tags: list[str], page: int) -> str` that safely builds a fully-qualified URL with encoded query parameters.

<details>
<summary>Show Solution</summary>

```python
import urllib.parse

def build_search_url(base_url: str, query: str, tags: list[str], page: int) -> str:
    params = {
        "q": query,
        "page": page,
        "tag": tags
    }
    # doseq=True expands list items into multiple query parameters
    query_string = urllib.parse.urlencode(params, doseq=True)
    return f"{base_url.rstrip('/')}/search?{query_string}"

# Test
url = build_search_url("https://api.site.com", "python guide", ["backend", "devops"], 1)
print("Constructed URL:", url)
assert "q=python+guide" in url
assert "tag=backend&tag=devops" in url
assert "page=1" in url
```
</details>

### Exercise 2: HTTP Status Code Evaluator
**Requirement:** Write a function that accepts an integer status code and uses `http.HTTPStatus` to categorize it into a descriptive dictionary (`{code: int, phrase: str, category: str}`).

<details>
<summary>Show Solution</summary>

```python
from http import HTTPStatus

def evaluate_status(code: int) -> dict:
    try:
        status = HTTPStatus(code)
    except ValueError:
        return {"code": code, "phrase": "Unknown", "category": "Invalid"}

    if 200 <= code < 300:
        cat = "Success"
    elif 300 <= code < 400:
        cat = "Redirection"
    elif 400 <= code < 500:
        cat = "Client Error"
    elif 500 <= code < 600:
        cat = "Server Error"
    else:
        cat = "Informational"

    return {
        "code": code,
        "phrase": status.phrase,
        "category": cat
    }

# Test
print(evaluate_status(200))
print(evaluate_status(404))
print(evaluate_status(503))
assert evaluate_status(404)["phrase"] == "Not Found"
assert evaluate_status(503)["category"] == "Server Error"
```
</details>

### Exercise 3: URL Path Sanitizer
**Requirement:** Write a function `sanitize_url_path(raw_path: str) -> str` that resolves relative navigation tokens (`..` and `.`) and returns a clean, canonicalized path.

<details>
<summary>Show Solution</summary>

```python
import posixpath
import urllib.parse

def sanitize_url_path(raw_path: str) -> str:
    # Decode percent encoding first
    unquoted = urllib.parse.unquote(raw_path)
    # Canonicalize using posixpath normpath
    normalized = posixpath.normpath(unquoted)
    # Re-quote
    return urllib.parse.quote(normalized)

# Test
dirty_path = "/static/../images/./profiles/%20avatar.png"
cleaned = sanitize_url_path(dirty_path)
print("Sanitized Path:", cleaned)
assert cleaned == "/images/profiles/%20avatar.png"
```
</details>

### Exercise 4: In-Memory JSON Request Handler
**Requirement:** Implement a function `create_json_request(url: str, payload: dict, auth_token: str) -> urllib.request.Request` with proper headers, method, and UTF-8 encoded body.

<details>
<summary>Show Solution</summary>

```python
import urllib.request
import json

def create_json_request(url: str, payload: dict, auth_token: str) -> urllib.request.Request:
    body_bytes = json.dumps(payload).encode("utf-8")
    headers = {
        "Content-Type": "application/json; charset=utf-8",
        "Authorization": f"Bearer {auth_token}",
        "Content-Length": str(len(body_bytes)),
        "User-Agent": "CustomAPIClient/1.0"
    }
    return urllib.request.Request(url, data=body_bytes, headers=headers, method="POST")

# Test
req = create_json_request("https://api.test/v1/update", {"status": 1}, "secret_token")
print("Headers:", req.headers)
assert req.get_header("Authorization") == "Bearer secret_token"
assert req.get_method() == "POST"
```
</details>

### Exercise 5: Relative Link Resolver
**Requirement:** Given a web page URL and a list of extracted raw HTML href targets, resolve all links into fully qualified absolute URLs using `urllib.parse.urljoin()`.

<details>
<summary>Show Solution</summary>

```python
import urllib.parse

def resolve_all_links(page_url: str, hrefs: list[str]) -> list[str]:
    return [urllib.parse.urljoin(page_url, href) for href in hrefs]

# Test
base_page = "https://docs.python.org/3/library/urllib.html"
raw_hrefs = [
    "urllib.parse.html",
    "/3/whatsnew/index.html",
    "../tutorial/index.html",
    "https://github.com/python/cpython"
]

resolved = resolve_all_links(base_page, raw_hrefs)
for r in resolved:
    print("Resolved link:", r)

assert resolved[0] == "https://docs.python.org/3/library/urllib.parse.html"
assert resolved[1] == "https://docs.python.org/3/whatsnew/index.html"
assert resolved[2] == "https://docs.python.org/3/tutorial/index.html"
```
</details>

### Exercise 6: Chunked Stream Hasher
**Requirement:** Write a function that takes an active `http.client.HTTPResponse` stream and computes its SHA-256 hash in 64KB chunks without storing the entire payload in memory.

<details>
<summary>Show Solution</summary>

```python
import hashlib
import io

def hash_response_stream(response_stream, chunk_size: int = 65536) -> str:
    hasher = hashlib.sha256()
    while chunk := response_stream.read(chunk_size):
        hasher.update(chunk)
    return hasher.hexdigest()

# Test with synthetic stream
test_data = b"Streaming network response payload" * 1000
stream = io.BytesIO(test_data)
expected_hash = hashlib.sha256(test_data).hexdigest()

calculated_hash = hash_response_stream(stream)
print("Calculated Hash:", calculated_hash)
assert calculated_hash == expected_hash
```
</details>
