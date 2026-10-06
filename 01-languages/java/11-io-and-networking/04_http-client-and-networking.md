# HTTP Client and Networking

Most Java programs talk to other systems over the network, and most of those conversations are HTTP. Since **Java 11**, the JDK ships a modern, standard HTTP client in `java.net.http` (it was an incubator module in 9 and 10). It supports HTTP/1.1 and HTTP/2, synchronous and asynchronous calls, and needs no third-party dependency. Underneath HTTP sits TCP, which Java exposes through `Socket` and `ServerSocket`.

**Prerequisites:** [I/O Streams, Readers and Writers](00_io-streams-readers-writers.md), [Duration](../10-date-and-time/03_duration-and-period.md). Section 4 uses [CompletableFuture](../14-concurrency/12_completablefuture.md).

---

## 1. A first request

```java
HttpClient client = HttpClient.newHttpClient();

HttpRequest request = HttpRequest.newBuilder(URI.create("https://example.com/api/users/42"))
        .GET()
        .build();

HttpResponse<String> response = client.send(request, HttpResponse.BodyHandlers.ofString());

System.out.println(response.statusCode());                    // 200
System.out.println(response.headers().firstValue("Content-Type").orElse("?"));
System.out.println(response.body());
```

Three objects, three roles:

```text
HttpClient   ── configuration + connection pool (long-lived, shared)
HttpRequest  ── URI, method, headers, body, timeout (immutable, per call)
HttpResponse ── status, headers, body (type chosen by the BodyHandler)
```

`send` throws `IOException` (network problems, timeouts) and `InterruptedException` (see section 5).

---

## 2. Configuring the client

```java
HttpClient client = HttpClient.newBuilder()
        .version(HttpClient.Version.HTTP_2)                 // default; falls back to HTTP/1.1 if the server can't
        .connectTimeout(Duration.ofSeconds(5))
        .followRedirects(HttpClient.Redirect.NORMAL)        // default is NEVER
        .build();
```

Defaults that matter in production:

| Setting | Default | Consequence |
|---|---|---|
| Connect timeout | **none** | A dead host can block a thread for a very long time |
| Request timeout | **none** | Per-request, set on `HttpRequest.Builder.timeout(...)` |
| Redirects | `NEVER` | A `301`/`302` is returned to you as a response |
| Cookies | none | Set a `CookieHandler` if you need a session |

**Always set both a connect timeout (client) and a request timeout (request).** A missing timeout is the most common cause of threads piling up when a downstream service hangs.

### Reuse the client

An `HttpClient` is **immutable and thread-safe**, with its own connection pool and threads. Create **one** (per configuration) and share it. Creating a client per request throws away connection reuse and wastes resources. Since Java 21 it also implements `AutoCloseable` (`close()`, `shutdown()`), which is useful for short-lived programs and tests.

Other options on the builder: `executor(...)` (for async callbacks), `proxy(ProxySelector)`, `authenticator(...)`, `sslContext(...)`.

---

## 3. Building requests

```java
String json = """
        {"name": "Asha", "role": "admin"}
        """;

HttpRequest post = HttpRequest.newBuilder(URI.create("https://example.com/api/users"))
        .timeout(Duration.ofSeconds(10))
        .header("Content-Type", "application/json")
        .header("Accept", "application/json")
        .header("Authorization", "Bearer " + token)
        .POST(HttpRequest.BodyPublishers.ofString(json))
        .build();
```

| Method | Builder call |
|---|---|
| GET | `.GET()` |
| POST / PUT | `.POST(publisher)` / `.PUT(publisher)` |
| DELETE | `.DELETE()` |
| Anything else | `.method("PATCH", publisher)` |

Body publishers: `ofString`, `ofByteArray`, `ofFile(Path)`, `ofInputStream(supplier)`, `noBody()`.

### Query strings and URL encoding

The client does not build query strings for you. Encode values yourself:

```java
String q = URLEncoder.encode("café & tea", StandardCharsets.UTF_8);    // "caf%C3%A9+%26+tea"  (Java 10+ overload)
URI uri = URI.create("https://example.com/search?q=" + q + "&page=2");
```

`URLEncoder` implements HTML **form** encoding (a space becomes `+`), which suits query values but **not path segments**. Never concatenate unencoded user input into a URI.

### `URI` vs `URL`

Prefer `URI` (pure parsing and syntax). `URL` has historical baggage: `equals`/`hashCode` can trigger DNS lookups, and its constructors are deprecated since Java 20. Convert when an API needs one: `uri.toURL()`.

---

## 4. Handling responses

### Body handlers

| Handler | Body type | Use |
|---|---|---|
| `ofString()` | `String` | Small text/JSON |
| `ofByteArray()` | `byte[]` | Small binary |
| `ofFile(Path)` | `Path` | **Large downloads**, written directly to disk |
| `ofInputStream()` | `InputStream` | Streaming (you must close it) |
| `ofLines()` | `Stream<String>` | Line-oriented streaming (close it) |
| `discarding()` | `Void` | You only care about status |

```java
// Download a large file without holding it in memory
client.send(request, HttpResponse.BodyHandlers.ofFile(Path.of("download.zip")));
```

### Check the status yourself

**HTTP error statuses do not throw.** A `404` or `500` is a normal `HttpResponse`:

```java
HttpResponse<String> r = client.send(request, HttpResponse.BodyHandlers.ofString());

if (r.statusCode() / 100 == 2) {
    return parse(r.body());
} else if (r.statusCode() == 404) {
    return Optional.empty();
} else {
    throw new ApiException(r.statusCode(), r.body());
}
```

### JSON

The JDK has no JSON support. Deserialize the body with Jackson or Gson ([Jackson](../17-json-and-data-formats/01_jackson.md)):

```java
User user = mapper.readValue(r.body(), User.class);
```

### Asynchronous calls

`sendAsync` returns a `CompletableFuture<HttpResponse<T>>` immediately:

```java
CompletableFuture<String> body = client
        .sendAsync(request, HttpResponse.BodyHandlers.ofString())
        .thenApply(HttpResponse::body);

// Fan out several calls in parallel
List<CompletableFuture<HttpResponse<String>>> calls = urls.stream()
        .map(u -> HttpRequest.newBuilder(URI.create(u)).timeout(Duration.ofSeconds(5)).build())
        .map(req -> client.sendAsync(req, HttpResponse.BodyHandlers.ofString()))
        .toList();

CompletableFuture.allOf(calls.toArray(CompletableFuture[]::new)).join();
```

Failures arrive as exceptions inside the future (often wrapped in `CompletionException`). Handle them with `exceptionally`/`handle` ([CompletableFuture](../14-concurrency/12_completablefuture.md)).

On Java 21+ with virtual threads, plain blocking `send` per task is also a simple, scalable option ([Virtual Threads](../14-concurrency/13_virtual-threads-and-structured-concurrency.md)).

---

## 5. Errors, timeouts, retries

| Situation | What you see |
|---|---|
| Can't connect / connection reset | `IOException` (e.g. `ConnectException`) |
| Connect timeout exceeded | `HttpConnectTimeoutException` (an `IOException`) |
| Request timeout exceeded | `HttpTimeoutException` (an `IOException`) |
| Server returns 4xx/5xx | Normal response: check `statusCode()` |
| Thread interrupted while waiting | `InterruptedException` |

Always handle `InterruptedException` properly: restore the flag and stop:

```java
try {
    return client.send(request, BodyHandlers.ofString());
} catch (InterruptedException e) {
    Thread.currentThread().interrupt();
    throw new IllegalStateException("Interrupted while calling API", e);
}
```

**Retries are not built in.** If you add them:

- retry only **idempotent** requests (GET, PUT, DELETE) or POSTs protected by an idempotency key ([Idempotency](../25-real-world-patterns/04_idempotency.md)),
- retry only on timeouts, connection errors, `429`, and `5xx`, never on other `4xx`,
- use exponential backoff with jitter ([Retry and Backoff](../25-real-world-patterns/02_retry-and-backoff.md)),
- consider a circuit breaker for a failing dependency ([Circuit Breaker](../25-real-world-patterns/06_circuit-breaker-and-resilience.md)).

---

## 6. Security basics

- Use `https://`. The default `SSLContext` validates certificates and host names. **Never** "fix" a TLS error by installing a trust-everything `TrustManager`. Fix the truststore instead.
- Don't log `Authorization` headers, tokens, or full request bodies containing secrets.
- If the URL comes from user input, validate the scheme and host against an allow-list. Otherwise your server can be tricked into calling internal addresses (SSRF). See [Input Validation and Injection](../21-security/00_input-validation-and-injection.md).
- Keep secrets out of source code ([Secrets Management](../21-security/03_secrets-management.md)).

---

## 7. Beneath HTTP: TCP sockets

HTTP is a protocol on top of TCP. When you need a custom protocol, `Socket` (client) and `ServerSocket` (server) give you a connected pair of byte streams: the same `InputStream`/`OutputStream` model as files ([streams](00_io-streams-readers-writers.md)).

### Client

```java
try (Socket socket = new Socket()) {
    socket.connect(new InetSocketAddress("localhost", 9090), 3_000);   // connect timeout (ms)
    socket.setSoTimeout(5_000);                                        // read timeout (ms)

    PrintWriter out = new PrintWriter(new OutputStreamWriter(socket.getOutputStream(), StandardCharsets.UTF_8), true);
    BufferedReader in = new BufferedReader(new InputStreamReader(socket.getInputStream(), StandardCharsets.UTF_8));

    out.println("hello");
    System.out.println(in.readLine());      // throws SocketTimeoutException after 5 s of silence
}
```

`new Socket(host, port)` connects with **no** connect timeout, so use `connect(address, timeoutMs)`. Without `setSoTimeout`, `read` blocks forever.

### Server (echo) with one virtual thread per connection

```java
try (ServerSocket server = new ServerSocket(9090);
     ExecutorService pool = Executors.newVirtualThreadPerTaskExecutor()) {      // Java 21+
    while (true) {
        Socket client = server.accept();               // blocks until a client connects
        pool.submit(() -> handle(client));
    }
}

static void handle(Socket client) {
    try (client;
         BufferedReader in = new BufferedReader(new InputStreamReader(client.getInputStream(), StandardCharsets.UTF_8));
         PrintWriter out = new PrintWriter(new OutputStreamWriter(client.getOutputStream(), StandardCharsets.UTF_8), true)) {

        String line;
        while ((line = in.readLine()) != null) {
            out.println("echo: " + line);
        }
    } catch (IOException e) {
        // log; a dropped connection is normal
    }
}
```

Points to know about raw sockets:

- **TCP is a byte stream, not messages.** One `write` doesn't map to one `read`. You must define framing yourself (newline-delimited lines, length prefixes).
- Always set timeouts and close sockets (try-with-resources).
- Before virtual threads, thread-per-connection didn't scale, which is why NIO selectors (`ServerSocketChannel`, `Selector`) and frameworks such as Netty exist. For most applications, use HTTP or a framework instead of hand-written protocols.
- UDP exists too (`DatagramSocket`) for connectionless, unreliable messaging.
- `InetAddress.getByName(host)` does DNS resolution, which can be slow and blocking. The JVM caches results (see the `networkaddress.cache.ttl` security property if DNS changes matter to you).

---

## 8. Other HTTP options

| Option | Notes |
|---|---|
| `java.net.http.HttpClient` | Built in (11+), good default; also provides a `WebSocket` API |
| `HttpURLConnection` | Legacy, verbose, avoid in new code |
| Apache HttpClient, OkHttp | Mature libraries with richer features (interceptors, connection tuning) |
| Spring `RestClient`/`WebClient`, Feign, Retrofit | Higher-level declarative clients built on top of the above |

For testing code that makes HTTP calls, run a local fake server (WireMock, OkHttp `MockWebServer`) instead of calling real services ([Integration Testing](../19-testing/03_integration-testing.md)).

---

## Common mistakes

| Mistake | Fix |
|---|---|
| No connect/request timeout | Set `connectTimeout` on the client and `timeout` on every request |
| New `HttpClient` per request | Share one instance |
| Assuming a 404/500 throws | Check `statusCode()` |
| Reading a huge response with `ofString()` | `ofFile` or `ofInputStream`, and close it |
| Ignoring `InterruptedException` | Restore the interrupt flag, then stop |
| Redirects "not working" | `followRedirects(NORMAL)`; the default is `NEVER` |
| Concatenating unencoded values into URIs | `URLEncoder.encode(value, UTF_8)` for query values |
| Blind retries of POSTs | Idempotency key, or don't retry |
| Disabling TLS validation to get past an error | Fix the truststore/certificate |
| Raw sockets with no timeout, no framing | `setSoTimeout`, a defined message format |
| Blocking calls on a thread you can't afford to block | Use `sendAsync` or virtual threads |

### Debugging

- Enable JDK client logging by setting `-Djdk.httpclient.HttpClient.log=errors,requests,headers` (other values include `content`, `frames`, `ssl`, `trace`).
- `HttpTimeoutException` → either the server is slow or your timeout is too low. Log the URL and elapsed time.
- `SSLHandshakeException` / `PKIX path building failed` → the server's certificate isn't trusted by your JVM's truststore (private CA, expired cert, wrong host name).
- `ConnectException: Connection refused` → nothing is listening on that host and port (wrong port, service down, firewall).
- Different behavior from `curl` → compare redirects, `Accept`/`Content-Type` headers, HTTP version, and proxy settings.
- `java.net.BindException: Address already in use` (server) → another process holds the port (`lsof -i :9090` on Unix-like systems).

---

## Quick Summary

- Use `java.net.http.HttpClient` (Java 11+): **one shared client**, immutable `HttpRequest`s, a `BodyHandler` to choose the response type.
- **Set timeouts** (connect on the client, request on each request). Defaults are infinite.
- HTTP error statuses are normal responses. Check `statusCode()`. Redirects are off by default.
- `sendAsync` gives `CompletableFuture`s. On Java 21+, blocking `send` on virtual threads is also fine.
- URL-encode query values with `URLEncoder.encode(v, UTF_8)`. Prefer `URI` over `URL`.
- Retries need idempotency, backoff, and limits. They're not built in.
- Raw sockets are byte streams: set timeouts, define message framing, and close them.

**Next module:** [Modern Java](../12-modern-java/00_java-release-timeline.md)
