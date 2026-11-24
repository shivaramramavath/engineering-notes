# Static Content & HTTP Caching in Spring Boot

| Metadata | Value |
| :--- | :--- |
| **Level** | Intermediate to Advanced |
| **Prerequisites** | HTTP RFC 9111 Caching, Spring MVC Resource Handlers |
| **Estimated Reading Time** | 22 minutes |
| **Status** | Production-Ready |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Baseline** | OpenJDK 21 LTS / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | Tomcat 10.1, OpenJDK 21.0.2 |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Configuration](#4-syntax--configuration)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance & Security Considerations](#8-performance--security-considerations)
9. [Comparison: Cache-Control Directives](#9-comparison-cache-control-directives)
10. [Interview Questions & Deep Dives](#10-interview-questions--deep-dives)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference](#12-summary--quick-reference)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

Static web assets (JavaScript bundles, CSS stylesheets, web fonts, brand logos, OpenAPI documentation) make up over 70% of network payloads transferred in typical web applications.

### The Problem
Improperly served static assets cause:
1. **Network Congestion**: Browsers downloading identical multi-megabyte JavaScript bundles on every page refresh.
2. **Stale Asset Glitches**: Deploying new frontend code while users' browsers retain cached old JavaScript bundles, causing runtime application crashes.
3. **Application Server Overload**: Serving static images through JVM application threads instead of leveraging CDN edge caches and HTTP 304 Not Modified headers.

### The Solution: Spring MVC Resource Handling & HTTP Caching
Spring Boot provides auto-configured static resource resolution across standard classpath locations (`/static`, `/public`, `/resources`, `/META-INF/resources`). Combined with `ResourceHttpRequestHandler`, content versioning (`VersionResourceResolver`), and declarative HTTP caching directives (`Cache-Control`, `ETag`, `Last-Modified`), Spring Boot enables immutable asset caching with automated cache-busting.

---

## 2. Core Concepts

### Mental Model & Asset Resolution Pipeline
```
                    Client requests: GET /css/app-a8f3b2.css
                                      │
                                      ▼
                      ResourceHttpRequestHandler
                                      │
           ┌──────────────────────────┴──────────────────────────┐
           ▼                                                     ▼
  VersionResourceResolver                               EncodedResourceResolver
(Translates fingerprinted URL                         (Checks for pre-compressed
 /css/app-a8f3b2.css -> /css/app.css)                   GZIP / Brotli assets: app.css.br)
           │                                                     │
           └──────────────────────────┬──────────────────────────┘
                                      ▼
                      Resolves Static Resource on Classpath
                     (src/main/resources/static/css/app.css)
                                      │
                                      ▼
                      Computes HTTP Cache Headers
                      • Cache-Control: max-age=31536000, immutable
                      • ETag: "01a4bc882f"
                      • Content-Encoding: br (if Brotli enabled)
                                      │
                                      ▼
                      Dispatches Asset Stream to Client
```

### Invariants
1. **Default Classpath Locations**: By default, Spring Boot scans:
   - `classpath:/static/`
   - `classpath:/public/`
   - `classpath:/resources/`
   - `classpath:/META-INF/resources/`
2. **Cache-Busting Invariant**: Fingerprinted assets (`app-<hash>.js`) can be safely cached with `max-age=31536000, immutable` (1 year). When source code changes, the hash changes, forcing immediate browser cache re-validation.
3. **Disabled for Pure REST APIs**: In pure backend microservices that do not serve frontend assets, static resource handling can be disabled (`spring.web.resources.add-mappings: false`) to shave route-matching overhead.

---

## 3. How It Works Internally

### Resource Chain Architecture
`ResourceHttpRequestHandler` delegates lookups to a configured chain of `ResourceResolver` and `ResourceTransformer` beans:
1. **`VersionResourceResolver`**: Extracts MD5/SHA content hashes from URLs or app versions.
2. **`EncodedResourceResolver`**: Checks whether a pre-compressed version (e.g. `file.js.br` or `file.js.gz`) exists on disk, serving compressed bytes directly without on-the-fly CPU compression.
3. **`PathResourceResolver`**: Ensures the resolved path resides strictly within the authorized base directory, preventing Directory Traversal attacks.
4. **`CssLinkResourceTransformer`**: Parses CSS files and rewrites `@import` and `url(...)` links to include version fingerprints.

---

## 4. Syntax & Configuration

### Standard `application.yaml` Resource & Caching Configuration
```yaml
spring:
  web:
    resources:
      static-locations:
        - classpath:/static/
        - classpath:/public/
      cache:
        period: 3600 # Fallback cache period in seconds
        cachecontrol:
          max-age: 86400 # 24 hours
          must-revalidate: true
      chain:
        enabled: true
        cache: true # Cache resolved resource metadata in memory
        strategy:
          content:
            enabled: true # Appends content-hash fingerprints: main-d41d8cd9.css
            paths: /**
```

---

## 5. Practical Examples

### Example 1: Programmatic Resource Handler with Immutable Caching & Fingerprinting

```java
package com.example.web.config;

import org.springframework.context.annotation.Configuration;
import org.springframework.http.CacheControl;
import org.springframework.web.servlet.config.annotation.ResourceHandlerRegistry;
import org.springframework.web.servlet.config.annotation.WebMvcConfigurer;
import org.springframework.web.servlet.resource.EncodedResourceResolver;
import org.springframework.web.servlet.resource.VersionResourceResolver;

import java.time.Duration;

@Configuration(proxyBeanMethods = false)
public class StaticAssetWebMvcConfig implements WebMvcConfigurer {

    @Override
    public void addResourceHandlers(ResourceHandlerRegistry registry) {
        // Fingerprinted static assets: safe to cache for 1 year with immutable directive
        registry.addResourceHandler("/assets/**")
                .addResourceLocations("classpath:/static/assets/")
                .setCacheControl(CacheControl.maxAge(Duration.ofDays(365)).cachePublic().immutable())
                .resourceChain(true)
                .addResolver(new EncodedResourceResolver()) // Serves .br/.gz if present
                .addResolver(new VersionResourceResolver().addContentVersionStrategy("/**"));

        // HTML pages or unversioned files: must revalidate with server on every hit
        registry.addResourceHandler("/*.html")
                .addResourceLocations("classpath:/static/")
                .setCacheControl(CacheControl.noCache().mustRevalidate());
    }
}
```

### Example 2: Programmatic Controller-Level HTTP Caching (`CacheControl` & `ETag`)

```java
package com.example.web.controller;

import org.springframework.http.CacheControl;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;
import org.springframework.web.context.request.WebRequest;

import java.time.Duration;
import java.util.UUID;

public record AppThemeConfig(UUID themeId, String primaryColor, String fontFamily, int version) {}

@RestController
@RequestMapping("/api/v1/themes")
public class ThemeConfigController {

    @GetMapping("/active")
    public ResponseEntity<AppThemeConfig> getActiveTheme(WebRequest request) {
        AppThemeConfig theme = new AppThemeConfig(
                UUID.fromString("11111111-2222-3333-4444-555555555555"),
                "#0055FF",
                "Inter, sans-serif",
                42
        );

        String etag = "\"theme-v" + theme.version() + "\"";

        // If client sends matching If-None-Match header, immediately return 304 Not Modified
        if (request.checkNotModified(etag)) {
            return null; // Spring automatically flushes HTTP 304 with no body
        }

        return ResponseEntity.ok()
                .cacheControl(CacheControl.maxAge(Duration.ofHours(6)).mustRevalidate())
                .eTag(etag)
                .body(theme);
    }
}
```

### Example 3: Edge Case — Shallow ETag Filter for Dynamic JSON APIs

When you want automatic ETag generation across all REST controllers without manually writing `checkNotModified()` in every endpoint:

```java
package com.example.web.config;

import org.springframework.boot.web.servlet.FilterRegistrationBean;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.web.filter.ShallowEtagHeaderFilter;

@Configuration(proxyBeanMethods = false)
public class EtagFilterConfig {

    /**
     * ShallowEtagHeaderFilter hashes the response body (MD5) and appends an ETag header.
     * If the client's If-None-Match matches, it discards the response body and sends 304.
     */
    @Bean
    public FilterRegistrationBean<ShallowEtagHeaderFilter> shallowEtagHeaderFilter() {
        FilterRegistrationBean<ShallowEtagHeaderFilter> filterBean = new FilterRegistrationBean<>(new ShallowEtagHeaderFilter());
        filterBean.addUrlPatterns("/api/*");
        filterBean.setName("shallowEtagFilter");
        return filterBean;
    }
}
```

### Example 4: Anti-Pattern vs Production Fix

#### Anti-Pattern: Using Long Caching on Non-Fingerprinted Static Files
```yaml
# ANTI-PATTERN: Long cache without fingerprinted URLs
spring:
  web:
    resources:
      cache:
        cachecontrol:
          max-age: 31536000 # 1 year cache on raw /app.js!
          # When developers deploy updates, users retain broken old JS for 1 year!
```

#### Production Fix: Content-Hashing Fingerprint Strategy
```yaml
# PRODUCTION FIX: Combine long cache with content-based versioning
spring:
  web:
    resources:
      chain:
        strategy:
          content:
            enabled: true # HTML rewrites /app.js -> /app-8dfa3bc1.js
      cache:
        cachecontrol:
          max-age: 31536000
          immutable: true # Fully safe because hash changes on modification!
```

### Example 5: High-Performance Pre-Compressed Asset Serving (Brotli & Gzip)

```bash
# Production Build Step (e.g. Webpack / Vite / Vite):
# Pre-compress static assets during CI build:
# brotli -k -9 static/assets/bundle.js -> static/assets/bundle.js.br
# gzip -k -9 static/assets/bundle.js   -> static/assets/bundle.js.gz
```

```java
// Spring MVC automatically serves pre-compressed assets when EncodedResourceResolver is registered:
@Override
public void addResourceHandlers(ResourceHandlerRegistry registry) {
    registry.addResourceHandler("/dist/**")
            .addResourceLocations("classpath:/static/dist/")
            .resourceChain(true)
            .addResolver(new EncodedResourceResolver()); // Zero CPU compression overhead at runtime!
}
```

---

## 6. Best Practices

1. **Use `immutable` with Fingerprinted Files**: When asset paths include a content hash (`bundle-a8f3b2.js`), use `Cache-Control: public, max-age=31536000, immutable`.
2. **Never Cache `index.html`**: Use `Cache-Control: no-cache, must-revalidate` for `index.html` so browsers always fetch the newest root HTML pointing to latest hashed asset bundles.
3. **Register `ShallowEtagHeaderFilter` for REST APIs**: Save downstream bandwidth by automatically returning HTTP 304 for unchanged JSON payloads.
4. **Use Pre-Compressed Assets**: Use `EncodedResourceResolver` to serve pre-compressed `.br` or `.gz` files generated during the build, eliminating runtime CPU compression overhead.
5. **Disable Static Mappings in Pure APIs**: Set `spring.web.resources.add-mappings: false` if your Spring Boot service is exclusively a REST API, avoiding unnecessary static resource lookups on 404s.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Caching `index.html` with Long Max-Age** | Users receive stale references to old, deleted JavaScript bundle hashes. | Set `no-cache` on `index.html`. |
| **Omitting Fingerprints on Long-Cached Assets** | Updates to CSS/JS are ignored by browser caches until `max-age` expires. | Enable `spring.web.resources.chain.strategy.content.enabled=true`. |
| **Confusing `no-cache` and `no-store`** | `no-cache` allows caching (requires server revalidation); `no-store` forbids caching entirely. | Use `no-store` for sensitive financial/PII data. |
| **Path Traversal via Custom Resource Handlers** | Poorly configured resource locations allow accessing `/etc/passwd`. | Always use `PathResourceResolver` (enabled by default). |
| **High CPU Overhead with Shallow ETag** | `ShallowEtagHeaderFilter` buffers the complete response in memory and computes MD5 hashes. | For massive file downloads, use `Last-Modified` timestamps instead. |
| **Missing `Vary: Accept-Encoding`** | Proxies serve Gzip bytes to clients that don't support compression. | Ensure `Vary: Accept-Encoding` is present when serving compressed content. |

---

## 8. Performance & Security Considerations

### Directory Traversal Attacks (CVE-2014-3625)
Spring's `PathResourceResolver` strictly checks that resolved resources are descendants of the configured static directories. Never disable path normalization or manually map unverified user-supplied paths directly to root directories.

### Shallow ETag vs Deep ETag
- **Shallow ETag**: Executes full controller business logic and database queries, then hashes the resulting bytes. Saves network bandwidth (returns 304), but does **not** save database or CPU time.
- **Deep ETag**: Queries entity `version` or `updated_at` before running heavy queries. If version matches, returns 304 immediately, saving database I/O and CPU.

---

## 9. Comparison: Cache-Control Directives

| Directive | Browser Behavior | Intermediate Proxy / CDN | Best Used For |
| :--- | :--- | :--- | :--- |
| **`public, max-age=31536000, immutable`** | Caches for 1 year; never revalidates | Caches aggressively | Fingerprinted assets (`app-hash.js`) |
| **`no-cache, must-revalidate`** | Caches locally, but must check server (`If-None-Match`) before using | Must revalidate with server | `index.html`, dynamic JSON APIs |
| **`no-store`** | Never caches anything on disk or memory | Never caches | Banking details, credentials, PII |
| **`private, max-age=3600`** | Caches in browser only | Never caches | Per-user personalized dashboards |

---

## 10. Interview Questions & Deep Dives

<details>
<summary><strong>Q1: What is the difference between `Cache-Control: no-cache` and `Cache-Control: no-store`?</strong></summary>

**Answer:**
- `no-cache`: The browser **can** store the response in its cache, but it **must** revalidate with the origin server (using `If-None-Match` or `If-Modified-Since`) before using it. If the server responds with 304 Not Modified, the cached version is used.
- `no-store`: The browser and all intermediate proxies are strictly forbidden from storing any part of the request or response in cache or on disk. Used for sensitive data (passwords, tokens, payment cards).
</details>

<details>
<summary><strong>Q2: How does content-based cache busting work in Spring Boot?</strong></summary>

**Answer:**
When `spring.web.resources.chain.strategy.content.enabled=true` is set, `VersionResourceResolver` calculates an MD5 content hash of static files at build or boot time and inserts it into the URL (e.g., `/css/style.css` becomes `/css/style-d41d8cd98f.css`). When templates link to `@{/css/style.css}`, Thymeleaf rewrites the path to the fingerprinted URL. Because the URL changes whenever the file content changes, the asset can be safely cached with `max-age=31536000, immutable`.
</details>

<details>
<summary><strong>Q3: What is the performance trade-off of `ShallowEtagHeaderFilter`?</strong></summary>

**Answer:**
`ShallowEtagHeaderFilter` buffers the entire HTTP response in memory and computes an MD5 checksum over the byte array. If the client sent a matching `If-None-Match` header, it returns `304 Not Modified` with zero body bytes, saving network bandwidth. However, it does not reduce server-side execution cost because the controller method and database queries still execute in full.
</details>

<details>
<summary><strong>Q4: Why was the `immutable` Cache-Control directive introduced?</strong></summary>

**Answer:**
Historically, when a user clicked the browser refresh button, browsers would revalidate all cached assets with conditional `If-None-Match` requests even if their `max-age` had not expired. The `immutable` directive tells the browser that the file content will never change during its lifetime, preventing browsers from sending redundant revalidation requests during manual reloads.
</details>

<details>
<summary><strong>Q5: What are the default static resource directories in Spring Boot?</strong></summary>

**Answer:**
1. `classpath:/static/`
2. `classpath:/public/`
3. `classpath:/resources/`
4. `classpath:/META-INF/resources/`
</details>

<details>
<summary><strong>Q6: How does `EncodedResourceResolver` improve server throughput?</strong></summary>

**Answer:**
Compressing assets on the fly (dynamic GZIP/Brotli) consumes significant CPU under high traffic. `EncodedResourceResolver` allows serving pre-compressed assets (e.g. `main.js.br` or `main.js.gz`) that were generated during the project build phase, streaming pre-compressed bytes directly to the network socket without runtime CPU cost.
</details>

<details>
<summary><strong>Q7: How do you completely disable static resource mapping in a pure REST API application?</strong></summary>

**Answer:**
Set `spring.web.resources.add-mappings: false` in `application.yaml`. This prevents Spring from querying the filesystem/classpath for non-existent routes, speeding up 404 handling.
</details>

<details>
<summary><strong>Q8: What is the purpose of the `Vary: Accept-Encoding` HTTP response header?</strong></summary>

**Answer:**
It informs intermediate caches and CDNs that the response representation varies based on the client's `Accept-Encoding` request header (e.g., `gzip`, `br`, or uncompressed). This prevents a cache from serving a compressed Brotli response to a legacy client that only supports raw text.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Disable Static Resource Mapping</strong></summary>

**Task**: Write the `application.yaml` property to disable static resource mappings in a pure backend REST service.

```yaml
# Solution:
spring:
  web:
    resources:
      add-mappings: false
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Controller Response with `CacheControl.noStore()`</strong></summary>

**Task**: Write an endpoint `/api/balance` returning account balance with headers forbidding all browser and proxy caching.

```java
// Solution:
@GetMapping("/api/balance")
public ResponseEntity<String> getBalance() {
    return ResponseEntity.ok()
            .cacheControl(CacheControl.noStore())
            .body("Balance: $4,500.00");
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Configure 1-Year Immutable Caching for `/static/media/`</strong></summary>

**Task**: Register a `WebMvcConfigurer` resource handler mapping `/media/**` to `classpath:/static/media/` with 1-year immutable caching.

```java
// Solution:
@Configuration(proxyBeanMethods = false)
public class MediaResourceConfig implements WebMvcConfigurer {
    @Override
    public void addResourceHandlers(ResourceHandlerRegistry registry) {
        registry.addResourceHandler("/media/**")
                .addResourceLocations("classpath:/static/media/")
                .setCacheControl(CacheControl.maxAge(Duration.ofDays(365)).immutable().cachePublic());
    }
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Deep ETag Validation via Entity Version</strong></summary>

**Task**: Implement an endpoint that checks `If-None-Match` against a document's version number, short-circuiting before loading the heavy document body from the database.

```java
// Solution:
@GetMapping("/api/documents/{id}")
public ResponseEntity<String> getDocument(@PathVariable Long id, WebRequest request) {
    int version = documentService.getVersionOnly(id); // Fast cheap query
    String etag = "\"v" + version + "\"";
    if (request.checkNotModified(etag)) {
        return null; // Returns 304 without querying heavy content!
    }
    String heavyContent = documentService.getFullContent(id);
    return ResponseEntity.ok().eTag(etag).body(heavyContent);
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Dynamic Content Versioning Resource Resolver</strong></summary>

**Task**: Configure a resource chain with `VersionResourceResolver` that fingerprints all CSS and JavaScript files while maintaining a 5-minute memory cache of resolved resource locations.
</details>

<details>
<summary><strong>Exercise 6 (Hard): Custom Cache-Control Annotation and Interceptor</strong></summary>

**Task**: Create a custom annotation `@CachePolicy(maxAge = 3600, isPublic = true)` and a `HandlerInterceptor` that inspects the annotation on controller methods and automatically attaches the corresponding `Cache-Control` header to the response.
</details>

---

## 12. Summary & Quick Reference

- **Default Paths**: `/static`, `/public`, `/resources`, `/META-INF/resources`.
- **Hashed Assets**: Cache for 1 year with `CacheControl.maxAge(...).immutable()`.
- **Root `index.html`**: Always use `CacheControl.noCache().mustRevalidate()`.
- **Global ETag Filter**: Register `ShallowEtagHeaderFilter`.
- **REST API Optimization**: Set `spring.web.resources.add-mappings: false`.

---

## 13. Further Reading

- [Spring Framework Reference: Static Resources](https://docs.spring.io/spring-framework/reference/web/webmvc/mvc-config/static-resources.html)
- [RFC 9111: HTTP Caching](https://www.rfc-editor.org/rfc/rfc9111.html)
- [MDN Web Docs: HTTP Caching Overview](https://developer.mozilla.org/en-US/docs/Web/HTTP/Caching)
- [Google Web Fundamentals: HTTP Caching Best Practices](https://web.dev/http-cache/)
