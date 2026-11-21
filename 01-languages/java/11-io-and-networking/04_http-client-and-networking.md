# Modern HTTP Client and Network Programming

> The definitive reference for `java.net.http.HttpClient`, asynchronous non-blocking requests, HTTP/2 multiplexing, reactive stream BodyHandlers/Publishers, WebSockets, and low-level socket programming.

| Metadata | Value |
|---|---|
| **Level** | Advanced |
| **Prerequisites** | [00_io-streams-readers-writers.md](00_io-streams-readers-writers.md), [09-functional-java/00_lambda-expressions.md](../09-functional-java/00_lambda-expressions.md), [06-exceptions-and-debugging/00_exception-hierarchy-and-types.md](../06-exceptions-and-debugging/00_exception-hierarchy-and-types.md) |
| **Time to Read** | 30 minutes |
| **Status** | Active |
| **Java Version** | Java 11+ (Standardized HttpClient JEP 321), Java 21+ Virtual Thread synergy |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance & Security Notes](#8-performance--security-notes)
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### The Evolution of Networking in Java
In earlier Java versions (Java 1.0 to 8), network communication relied primarily on `java.net.HttpURLConnection` or third-party libraries like Apache HttpClient and OkHttp. `HttpURLConnection` was cumbersome, lacked HTTP/2 support, lacked native asynchronous execution, had awkward stream handling, and was notoriously difficult to configure with timeouts and authentication.

### The Solution: `java.net.http.HttpClient`
Standardized in Java 11 (JEP 321), `java.net.http.HttpClient` modernizes network programming with:
- Native support for **HTTP/1.1 and HTTP/2** (including multiplexing and server push).
- First-class support for both **Synchronous (blocking)** and **Asynchronous (non-blocking CompletableFuture)** execution.
- Integration with the Reactive Streams paradigm via `HttpRequest.BodyPublishers` and `HttpResponse.BodyHandlers`.
- Built-in **WebSocket** support.
- Natural alignment with **Virtual Threads** in Java 21+, enabling high-throughput blocking idioms without thread starvation.

---

## 2. Core Concepts

### The Core Triad
1. **`HttpClient`**: The engine and connection container. Reusable, thread-safe, and responsible for managing connection pools, SSL/TLS contexts, redirect policies, and proxy configurations.
2. **`HttpRequest`**: Immutable request specification constructed via builder pattern (URI, HTTP method, headers, timeout, body publisher).
3. **`HttpResponse<T>`**: Typed response received from the server (status code, headers, URI, typed body payload).

### BodyPublishers and BodyHandlers
- **`HttpRequest.BodyPublishers`**: Implements `Flow.Publisher<ByteBuffer>`, converting application data types (String, File, byte array, InputStream) into reactive byte streams.
- **`HttpResponse.BodyHandlers<T>`**: Dictates how response bytes are processed and converted into high-level types (`String`, `byte[]`, `Path`, `InputStream`, `Stream<String>`) before passing to the `HttpResponse`.

### Synchronous vs Asynchronous
- Synchronous: `client.send(request, BodyHandlers.ofString())` blocks the calling thread until response headers and body are fully received.
- Asynchronous: `client.sendAsync(request, BodyHandlers.ofString())` returns `CompletableFuture<HttpResponse<String>>`, executing non-blockingly across JVM threads.

### Low-Level Sockets
For protocols other than HTTP/HTTPS, Java provides:
- `java.net.Socket` and `ServerSocket`: Traditional blocking TCP client/server communication.
- `java.nio.channels.SocketChannel`: Non-blocking multiplexed I/O using selectors.

---

## 3. How It Works Internally

### Connection Pooling & Multiplexing
```
+-------------------------------------------------------------+
|                         HttpClient                          |
|  +-------------------------------------------------------+  |
|  |                   Connection Pool                     |  |
|  |  +--------------------+       +--------------------+  |  |
|  |  |  HTTP/1.1 Socket   |  ...  |  HTTP/2 Multiplex  |  |  |
|  |  | (1 request at time)|       | (Streams 1, 3, 5)  |  |  |
|  |  +--------------------+       +--------------------+  |  |
|  +-------------------------------------------------------+  |
+-------------------------------------------------------------+
```

### HTTP/2 Multiplexing Mechanics
In HTTP/1.1, each concurrent request requires a separate TCP socket or suffers from Head-of-Line (HoL) blocking.
In HTTP/2, `HttpClient` opens a single persistent TCP connection to the host and assigns each concurrent request a unique 31-bit Stream ID. Binary frames (`HEADERS`, `DATA`, `WINDOW_UPDATE`) are interleaved concurrently across the single socket, drastically reducing TLS handshake overhead and connection exhaustion.

---

## 4. Syntax & Usage

### Configuring a Shared `HttpClient`
```java
import java.net.http.HttpClient;
import java.net.Authenticator;
import java.net.PasswordAuthentication;
import java.time.Duration;

HttpClient client = HttpClient.newBuilder()
    .version(HttpClient.Version.HTTP_2)
    .connectTimeout(Duration.ofSeconds(10))
    .followRedirects(HttpClient.Redirect.NORMAL)
    .build();
```

### Constructing an `HttpRequest`
```java
import java.net.URI;
import java.net.http.HttpRequest;
import java.net.http.HttpRequest.BodyPublishers;
import java.time.Duration;

HttpRequest request = HttpRequest.newBuilder()
    .uri(URI.create("https://api.github.com/zen"))
    .timeout(Duration.ofSeconds(5))
    .header("Accept", "application/vnd.github.v3+json")
    .header("User-Agent", "JavaHttpClientDemo/1.0")
    .GET()
    .build();
```

---

## 5. Practical Examples

### Example 1: Synchronous GET Request
```java
// Goal: Fetch a plain text resource synchronously
import java.net.URI;
import java.net.http.*;
import java.time.Duration;

public class SyncGetExample {
    public static void main(String[] args) throws Exception {
        HttpClient client = HttpClient.newBuilder()
            .connectTimeout(Duration.ofSeconds(5))
            .build();

        HttpRequest request = HttpRequest.newBuilder()
            .uri(URI.create("https://httpbin.org/get"))
            .GET()
            .build();

        HttpResponse<String> response = client.send(request, HttpResponse.BodyHandlers.ofString());

        System.out.println("Status Code: " + response.statusCode());
        System.out.println("HTTP Version: " + response.version());
        System.out.println("Body:\n" + response.body());
    }
}
```
*Output:*
```
Status Code: 200
HTTP Version: HTTP_2
Body:
{
  "args": {}, 
  "headers": {
    "User-Agent": "Java-http-client/..."
  }, 
  "url": "https://httpbin.org/get"
}
```
*Explanation:* Standard blocking invocation. The response body is converted to a `String` via `BodyHandlers.ofString()`.

### Example 2: Real-World Asynchronous JSON POST
```java
// Goal: Send JSON payload asynchronously and process response with CompletableFuture
import java.net.URI;
import java.net.http.*;
import java.time.Duration;
import java.util.concurrent.CompletableFuture;

public class AsyncPostExample {
    public static void main(String[] args) {
        HttpClient client = HttpClient.newHttpClient();

        String jsonPayload = """
            {
                "userId": 101,
                "action": "LOGIN",
                "timestamp": 1700000000
            }
            """;

        HttpRequest request = HttpRequest.newBuilder()
            .uri(URI.create("https://httpbin.org/post"))
            .header("Content-Type", "application/json")
            .POST(HttpRequest.BodyPublishers.ofString(jsonPayload))
            .build();

        CompletableFuture<HttpResponse<String>> future = client.sendAsync(
            request, 
            HttpResponse.BodyHandlers.ofString()
        );

        future.thenApply(res -> {
            System.out.println("Response Status: " + res.statusCode());
            return res.body();
        }).thenAccept(body -> {
            System.out.println("Payload returned:\n" + body);
        }).exceptionally(ex -> {
            System.err.println("Request failed: " + ex.getMessage());
            return null;
        }).join(); // Wait for demo completion
    }
}
```
*Explanation:* Non-blocking asynchronous submission. Processing pipelines execute seamlessly using `CompletableFuture` functional chains.

### Example 3: Edge Case: Streaming Large Files Directly to Disk
```java
// Goal: Download gigabyte files without consuming heap memory
import java.net.URI;
import java.net.http.*;
import java.nio.file.*;

public class LargeFileDownloadDemo {
    public static void download(String fileUrl, Path destination) throws Exception {
        HttpClient client = HttpClient.newHttpClient();

        HttpRequest request = HttpRequest.newBuilder()
            .uri(URI.create(fileUrl))
            .GET()
            .build();

        // Streams directly to disk! Zero heap memory accumulation
        HttpResponse<Path> response = client.send(
            request, 
            HttpResponse.BodyHandlers.ofFile(destination, StandardOpenOption.CREATE, StandardOpenOption.WRITE)
        );

        System.out.println("Downloaded file to: " + response.body() + ", status: " + response.statusCode());
    }
}
```
*Explanation:* `BodyHandlers.ofFile()` routes the incoming reactive byte buffers directly to disk channels, preventing OutOfMemoryError regardless of file size.

### Example 4: Failure Case: Missing Timeout Leading to Indefinite Thread Hangs
```java
// Goal: Demonstrate why connectTimeout and request timeout are strictly mandatory
import java.net.URI;
import java.net.http.*;

public class TimeoutHazardDemo {
    public static void badRequest() throws Exception {
        // HAZARD: Default client has no connectTimeout; default request has no timeout!
        HttpClient client = HttpClient.newHttpClient();
        HttpRequest request = HttpRequest.newBuilder()
            .uri(URI.create("https://10.255.255.1")) // Unroutable IP
            .GET()
            .build();

        // Calling send() without timeout will hang for OS TCP timeout (often 2+ minutes)!
        // client.send(request, HttpResponse.BodyHandlers.ofString());
    }
}
```
*Explanation:* Always configure both `HttpClient.newBuilder().connectTimeout()` and `HttpRequest.newBuilder().timeout()`.

### Example 5: Low-Level TCP Socket Server & Client
```java
// Goal: Minimal low-level TCP Socket echo server and client communication
import java.io.*;
import java.net.*;

public class SimpleTcpEchoServer {
    public static void runServer(int port) throws IOException {
        try (ServerSocket server = new ServerSocket(port)) {
            System.out.println("Server listening on port " + port);
            try (Socket socket = server.accept();
                 BufferedReader reader = new BufferedReader(new InputStreamReader(socket.getInputStream()));
                 PrintWriter writer = new PrintWriter(socket.getOutputStream(), true)) {
                
                String line = reader.readLine();
                writer.println("ECHO: " + line);
            }
        }
    }
}
```
*Explanation:* Demonstrates foundational TCP socket mechanics underpinning higher-level protocols.

---

## 6. Best Practices
1. **Reuse `HttpClient` as a Singleton**: Never instantiate `HttpClient` per request. It manages heavy connection pools, SSL engines, and executor threads. Reuse a single instance application-wide.
2. **Always Configure Timeouts**: Always specify both `connectTimeout()` on the client and `timeout()` on every `HttpRequest`.
3. **Stream Large Payloads**: Never load large response bodies into memory via `BodyHandlers.ofString()`. Use `BodyHandlers.ofFile()` or `BodyHandlers.ofInputStream()`.
4. **Leverage Virtual Threads**: In Java 21+, configure `HttpClient.newBuilder().executor(Executors.newVirtualThreadPerTaskExecutor())` for massive concurrent blocking throughput.
5. **Handle Non-200 Status Codes**: `send()` does not throw exceptions on 4xx or 5xx HTTP errors. Always check `response.statusCode()` explicitly.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why Wrong | Fix |
|---|---|---|
| Creating new `HttpClient` per request | Causes socket exhaustion, leak of connection pool threads, and SSL handshake overhead. | Maintain a single shared `HttpClient` instance across the application. |
| Neglecting request timeouts | Network partitions or dropped packets hang the invoking thread indefinitely. | Always call `.timeout(Duration.ofSeconds(n))` on `HttpRequest.Builder`. |
| Assuming 404/500 throws an Exception | `HttpClient` only throws `IOException` on transport failure, not on HTTP status errors. | Inspect `response.statusCode()` and throw application-level exceptions. |
| Buffering huge downloads with `ofString()` | Results in immediate `OutOfMemoryError` on large file downloads. | Use `HttpResponse.BodyHandlers.ofFile()` or `ofInputStream()`. |
| Mixing up BodyPublishers and BodyHandlers | Publishers supply outgoing request bodies; Handlers parse incoming response bodies. | Use `BodyPublishers.of...` for requests and `BodyHandlers.of...` for responses. |
| Forgetting to close WebSockets | Keeps connection open indefinitely, exhausting remote server connection limits. | Send `sendClose()` and handle closing handshakes gracefully. |

---

## 8. Performance & Security Notes

### Connection Pooling & Keep-Alive
`HttpClient` automatically reuses TCP connections via keep-alive pools. Default system properties:
- `jdk.httpclient.connectionPoolSize`: Max idle connections (default 0 = unlimited within memory limits).
- `jdk.httpclient.keepalive.timeout`: Idle socket timeout in seconds.

### TLS / HTTPS Validation
By default, `HttpClient` uses the JVM's default `SSLContext` validating certificates against the system CA keystore (`cacerts`).
**Caution**: Never bypass certificate validation in production by configuring trust-all `TrustManager` implementations. This exposes the application to Man-in-the-Middle (MITM) credential theft.

### SSRF (Server-Side Request Forgery)
If constructing `HttpRequest` URIs from user input, validate that the URI target does not point to internal metadata IP addresses (e.g., `169.254.169.254` AWS metadata) or internal loopback `127.0.0.1` endpoints.

---

## 9. Comparison / When to Use What

| Feature | `java.net.http.HttpClient` | `HttpURLConnection` | OkHttp / Apache HttpClient |
|---|---|---|---|
| **Java Version** | Java 11+ (Standard) | Java 1.0+ (Legacy) | External Dependency |
| **HTTP/2 Support** | Built-in | None (HTTP/1.1 only) | Built-in |
| **Async Execution** | Native (`CompletableFuture`) | None (Manual threads) | Callbacks / RxJava |
| **Reactive Streams** | Standard `Flow` APIs | No | Internal buffers |
| **WebSocket** | Standard built-in | No | Requires extension |
| **Virtual Thread Fit** | Flawless | Poor | Good |

---

## 10. Interview Questions

<details>
<summary>1. What are the key advantages of `java.net.http.HttpClient` over `HttpURLConnection`?</summary>
`HttpClient` supports HTTP/2 multiplexing, asynchronous non-blocking operation via `CompletableFuture`, reactive stream body handlers, built-in WebSocket support, fluent builders, and automatic connection pooling.
</details>

<details>
<summary>2. Is `HttpClient` thread-safe, and how should it be scoped?</summary>
Yes, `HttpClient` is completely immutable and thread-safe. It should be scoped as a shared singleton across the application to maximize connection pool reuse and minimize TLS handshake overhead.
</details>

<details>
<summary>3. Does `client.send()` throw an exception when the server returns HTTP 500?</summary>
No. `client.send()` only throws `IOException` when transport-level failures occur (e.g. host unreachable, connection refused, SSL handshake failure). HTTP status codes (2xx, 3xx, 4xx, 5xx) are encapsulated in `response.statusCode()`.
</details>

<details>
<summary>4. What is the difference between `BodyHandler` and `BodySubscriber`?</summary>
A `BodyHandler` inspects response status code and headers, and decides how to handle the incoming body by returning a `BodySubscriber`. The `BodySubscriber` is the reactive consumer that actually receives the `ByteBuffer` stream chunks.
</details>

<details>
<summary>5. How does HTTP/2 multiplexing work in `HttpClient`?</summary>
HTTP/2 transmits multiple bidirectional request/response streams concurrently over a single underlying TCP connection using interleaved binary frames, eliminating head-of-line blocking and reducing connection overhead.
</details>

<details>
<summary>6. How do you stream large file uploads without loading them entirely into memory?</summary>
By using `HttpRequest.BodyPublishers.ofFile(Path path)`, which streams chunks directly from disk channels through reactive byte publishers.
</details>

<details>
<summary>7. How can `HttpClient` be configured to use Virtual Threads?</summary>
By passing `Executors.newVirtualThreadPerTaskExecutor()` into `HttpClient.newBuilder().executor(...)`. This ensures asynchronous completions and internal tasks run on lightweight virtual threads.
</details>

<details>
<summary>8. What is the difference between TCP Sockets and UDP DatagramSockets?</summary>
TCP (`Socket`/`ServerSocket`) is connection-oriented, reliable, and guarantees in-order byte stream delivery with error recovery. UDP (`DatagramSocket`) is connectionless, packet-oriented, does not guarantee delivery or ordering, but offers lower latency.
</details>

---

## 11. Practice Exercises

### Exercise 1 (Easy): Basic Head Request
- **Task**: Use `HttpClient` to send an HTTP `HEAD` request to a given URL and print the `Content-Type` and `Content-Length` headers.
<details>
<summary>Solution</summary>

```java
import java.net.URI;
import java.net.http.*;

public class HeadRequestExercise {
    public static void printHeaders(String url) throws Exception {
        HttpClient client = HttpClient.newHttpClient();
        HttpRequest request = HttpRequest.newBuilder()
            .uri(URI.create(url))
            .method("HEAD", HttpRequest.BodyPublishers.noBody())
            .build();

        HttpResponse<Void> response = client.send(request, HttpResponse.BodyHandlers.discarding());
        System.out.println("Status: " + response.statusCode());
        response.headers().firstValue("content-type").ifPresent(ct -> System.out.println("Type: " + ct));
        response.headers().firstValue("content-length").ifPresent(cl -> System.out.println("Length: " + cl));
    }
}
```
</details>

### Exercise 2 (Medium): Concurrent Asynchronous Batch Fetching
- **Task**: Given a list of URLs, use `client.sendAsync()` to download all pages concurrently and return a map of `URL -> StatusCode`.
<details>
<summary>Solution</summary>

```java
import java.net.URI;
import java.net.http.*;
import java.util.*;
import java.util.concurrent.CompletableFuture;
import java.util.stream.Collectors;

public class BatchFetchExercise {
    public static Map<String, Integer> fetchAll(List<String> urls) {
        HttpClient client = HttpClient.newHttpClient();

        List<CompletableFuture<Map.Entry<String, Integer>>> futures = urls.stream()
            .map(url -> {
                HttpRequest req = HttpRequest.newBuilder().uri(URI.create(url)).GET().build();
                return client.sendAsync(req, HttpResponse.BodyHandlers.discarding())
                    .thenApply(res -> Map.entry(url, res.statusCode()))
                    .exceptionally(ex -> Map.entry(url, -1));
            })
            .toList();

        CompletableFuture.allOf(futures.toArray(new CompletableFuture[0])).join();

        return futures.stream()
            .map(CompletableFuture::join)
            .collect(Collectors.toMap(Map.Entry::getKey, Map.Entry::getValue));
    }
}
```
</details>

### Exercise 3 (Hard): Resilient HTTP Fetcher with Exponential Backoff
- **Task**: Implement a method that executes a GET request, retrying up to 3 times with exponential backoff if the response code is 503 or an `IOException` occurs.
<details>
<summary>Solution</summary>

```java
import java.io.IOException;
import java.net.URI;
import java.net.http.*;
import java.time.Duration;

public class ResilientFetcher {
    private final HttpClient client = HttpClient.newBuilder()
        .connectTimeout(Duration.ofSeconds(3))
        .build();

    public HttpResponse<String> fetchWithRetry(String url, int maxRetries) throws InterruptedException {
        HttpRequest request = HttpRequest.newBuilder()
            .uri(URI.create(url))
            .timeout(Duration.ofSeconds(3))
            .GET()
            .build();

        int attempt = 0;
        long backoffMs = 200;

        while (true) {
            attempt++;
            try {
                HttpResponse<String> res = client.send(request, HttpResponse.BodyHandlers.ofString());
                if (res.statusCode() != 503 || attempt > maxRetries) {
                    return res;
                }
            } catch (IOException e) {
                if (attempt > maxRetries) {
                    throw new RuntimeException("Max retries exceeded: " + e.getMessage(), e);
                }
            }
            Thread.sleep(backoffMs);
            backoffMs *= 2;
        }
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

```
+-----------------------------------------------------------------------------------------+
|                               HTTPCLIENT QUICK REFERENCE                                |
+-----------------------------------------------------------------------------------------+
| Client Instance         | HttpClient client = HttpClient.newHttpClient();               |
| Custom Config           | HttpClient.newBuilder().version(Version.HTTP_2).build();     |
| Synchronous Call        | HttpResponse<String> res = client.send(req, BodyHandlers...); |
| Asynchronous Call       | CompletableFuture<HttpResponse<T>> f = client.sendAsync(...); |
| Reactive Body Handlers  | BodyHandlers.ofString(), ofFile(path), ofInputStream()        |
| Reactive Body Publishers| BodyPublishers.ofString(s), ofFile(p), noBody()               |
| WebSocket Connection    | client.newWebSocketBuilder().buildAsync(uri, listener);       |
| Virtual Threads         | .executor(Executors.newVirtualThreadPerTaskExecutor())        |
+-----------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [JEP 321: HTTP Client (Standardized in Java 11)](https://openjdk.org/jeps/321)
- [Official Javadoc: java.net.http Package](https://docs.oracle.com/en/java/javase/21/docs/api/java.net.http/java/net/http/package-summary.html)
- RFC 7540: Hypertext Transfer Protocol Version 2 (HTTP/2)
- RFC 6455: The WebSocket Protocol
