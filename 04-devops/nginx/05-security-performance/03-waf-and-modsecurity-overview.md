---
title: "Web Application Firewall (WAF) Integration & ModSecurity"
description: "Comprehensive guide to integrating Web Application Firewalls into NGINX, covering ModSecurity v3 (libmodsecurity), the OWASP Core Rule Set (CRS), Coraza WAF, anomaly scoring, and tuning false positives."
module: "05-security-performance"
topic: "03-waf-and-modsecurity-overview"
status: "completed"
tags: ["nginx", "waf", "modsecurity", "owasp", "crs", "coraza", "security", "firewall", "sqli", "xss"]
prerequisites: ["05-security-performance/02-security-hardening.md", "04-internals/05-modules-and-dynamic-modules.md"]
estimated_read_time: "35 min"
---

# Web Application Firewall (WAF) Integration & ModSecurity

```
+-------------------------------------------------------------------------------+
| NGINX + WAF Inspection Pipeline                                               |
|                                                                               |
|  Incoming Client Request                                                      |
|        │                                                                      |
|        ▼                                                                      |
|  [NGINX Core Event Loop]                                                      |
|        │  Parses request line, headers, and body                              |
|        ▼                                                                      |
|  [ModSecurity v3 / Coraza Connector Module]                                   |
|        │  Passes transaction to libmodsecurity C API                          |
|        ▼                                                                      |
|  [OWASP Core Rule Set (CRS) Engine]                                           |
|        ├─ Phase 1: Request Headers (Check User-Agent, Host, Protocol)        |
|        ├─ Phase 2: Request Body (Inspect SQLi, XSS, RCE, Path Traversal)     |
|        ├─ Anomaly Scoring Calculation (Inbound Score >= Threshold?)           |
|        ▼                                                                      |
|  Decision:                                                                    |
|  - Score Exceeded --> Intercept: Return 403 Forbidden                         |
|  - Clean Request  --> Pass to NGINX Content Phase -> Upstream Backend        |
|        │                                                                      |
|        ▼                                                                      |
|  [Phase 4: Response Inspection] (Optional: scan for data leakage / credit cards)|
+-------------------------------------------------------------------------------+
```

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts & Architecture](#2-core-concepts--architecture)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Directives Reference](#4-syntax--directives-reference)
5. [Practical Implementation Examples](#5-practical-implementation-examples)
6. [Tuning, Rule Exclusions & Managing False Positives](#6-tuning-rule-exclusions--managing-false-positives)
7. [Common Pitfalls & Anti-Patterns](#7-common-pitfalls--anti-patterns)
8. [Performance & Latency Overhead Analysis](#8-performance--latency-overhead-analysis)
9. [Architectural Comparison: ModSecurity vs Coraza vs Cloud WAF](#9-architectural-comparison-modsecurity-vs-coraza-vs-cloud-waf)
10. [Technical Interview Questions & Scenarios](#10-technical-interview-questions--scenarios)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference](#12-summary--quick-reference)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

Traditional network firewalls and Layer 4 load balancers inspect IP addresses and port numbers, but they are entirely blind to application-layer attacks delivered over standard HTTPS channels. A Web Application Firewall (WAF) operates at Layer 7:
- **Zero-Day & Injection Defense**: Detects and neutralizes SQL Injection (SQLi), Cross-Site Scripting (XSS), Remote Code Execution (RCE), Server-Side Request Forgery (SSRF), and Local File Inclusion (LFI) before malicious payloads reach application runtimes.
- **Virtual Patching**: When a high-severity vulnerability (such as Log4Shell or Spring4Shell) is disclosed, developing and redeploying backend application code takes days or weeks. A WAF enables security teams to deploy an edge virtual patch in minutes.
- **OWASP Top 10 Compliance**: Regulatory standards such as PCI-DSS requirement 6.6 mandate either frequent code reviews or an automated Layer 7 firewall inspecting all public-facing traffic.

Integrating ModSecurity or Coraza directly into NGINX creates an inline security filter executing at native C or Go speeds.

---

## 2. Core Concepts & Architecture

### ModSecurity v3 (libmodsecurity)
Historically, ModSecurity v2 was tightly coupled with the Apache HTTP Server internals, requiring fragile emulation layers in NGINX. ModSecurity v3 represents a complete architectural rewrite:
- **`libmodsecurity`**: A standalone, thread-safe, protocol-agnostic C++ library providing the core rule execution and regex engines.
- **`ModSecurity-nginx`**: A lightweight dynamic connector module that hooks into NGINX HTTP request phases and bridges data between NGINX and `libmodsecurity`.

### OWASP Core Rule Set (CRS)
ModSecurity is an execution engine; the security intelligence comes from rule sets. The OWASP CRS is the industry standard open-source rule suite:
- **Anomaly Scoring vs Traditional Reaction**: Rather than blocking on the first matched pattern, CRS tallies an anomaly score based on severity (Critical: 5, Error: 4, Warning: 3, Notice: 2). If the cumulative score exceeds a defined threshold (typically 5 for inbound requests), the request is blocked.
- **Paranoia Levels (PL1 to PL4)**:
  - *PL1*: Baseline protection with minimal false positives (recommended for general production).
  - *PL2*: Enhanced protection (includes advanced regex, strict character sets).
  - *PL3*: Strict enterprise defense (demands rule tuning for custom applications).
  - *PL4*: Maximum paranoia (blocks nearly all non-alphanumeric parameters; high tuning required).

---

## 3. How It Works Internally

### The 5 ModSecurity Processing Phases in NGINX

```
  1. Connection Phase (Phase 1)
     - Executed in NGINX NGX_HTTP_REWRITE_PHASE
     - Inspects Request Line, URI, Request Headers, User-Agent, Cookies
     - Fast regex checks; no request body required

  2. Request Body Phase (Phase 2)
     - Executed in NGINX NGX_HTTP_PRECONTENT_PHASE / CONTENT_PHASE
     - NGINX buffers body into RAM/disk; passes stream to libmodsecurity
     - Deep payload inspection: JSON, XML, Multipart form-data, SQLi/XSS decoders

  3. Response Headers Phase (Phase 3)
     - Executed in NGINX Header Filter
     - Inspects upstream HTTP response status and response headers

  4. Response Body Phase (Phase 4)
     - Executed in NGINX Body Filter
     - Buffers downstream output; inspects for sensitive data leakage (PII, SSNs)

  5. Logging Phase (Phase 5)
     - Executed in NGINX NGX_HTTP_LOG_PHASE
     - Writes detailed audit logs to modsec_audit.log asynchronously
```

---

## 4. Syntax & Directives Reference

The `ModSecurity-nginx` connector introduces two primary NGINX directives:

### Directives
- `modsecurity on | off;`: Enables or disables the ModSecurity processing engine within the current context (`http`, `server`, or `location`).
- `modsecurity_rules_file <path>;`: Specifies the absolute filesystem path to the ModSecurity configuration file containing rule definitions and CRS includes.
- `modsecurity_rules '<inline_rules>';`: Allows specifying inline SecRule definitions directly within `nginx.conf`.

### Key SecRule Engine Directives (inside `modsecurity.conf`)
- `SecRuleEngine On | Off | DetectionOnly`:
  - `On`: Full enforcement (blocks offending requests with 403 Forbidden).
  - `DetectionOnly`: Audits and logs matches without interrupting traffic (essential for pre-production staging).
- `SecRequestBodyAccess On | Off`: Required for inspecting POST/PUT/PATCH bodies.
- `SecResponseBodyAccess On | Off`: Enables inspection of outbound responses (often disabled in high-traffic APIs to conserve CPU).
- `SecAuditEngine RelevantOnly | On | Off`: Governs when transactions are written to the audit log.

---

## 5. Practical Implementation Examples

### Example 1: Loading the ModSecurity Dynamic Module in `nginx.conf`

```nginx
# /etc/nginx/nginx.conf (Top of file)
load_module modules/ngx_http_modsecurity_module.so;

user www-data;
worker_processes auto;
pid /run/nginx.pid;

events {
    worker_connections 4096;
}

http {
    include /etc/nginx/mime.types;
    default_type application/octet-stream;

    # Enable ModSecurity globally in DetectionOnly or On mode
    modsecurity on;
    modsecurity_rules_file /etc/nginx/modsec/main.conf;

    include /etc/nginx/conf.d/*.conf;
}
```

### Example 2: Configuring the Master ModSecurity Rules File (`main.conf`)

```apache
# /etc/nginx/modsec/main.conf

# 1. Base ModSecurity configuration
Include /etc/nginx/modsec/modsecurity.conf

# 2. OWASP CRS Setup (Defines anomaly score thresholds and paranoia levels)
Include /etc/nginx/modsec/coreruleset/crs-setup.conf

# 3. Custom Application Rule Exclusions (Loaded before rules to whitelist paths)
Include /etc/nginx/modsec/custom-exclusions.conf

# 4. OWASP Core Rule Set Rules
Include /etc/nginx/modsec/coreruleset/rules/*.conf

# 5. Post-Rule Custom Blocks
Include /etc/nginx/modsec/post-rules.conf
```

### Example 3: Core Engine Configuration (`modsecurity.conf`)

Essential production tuning parameters for `libmodsecurity`:

```apache
# /etc/nginx/modsec/modsecurity.conf

# Set to DetectionOnly when testing; switch to On for production blocking
SecRuleEngine On

# Request Body Handling
SecRequestBodyAccess On
SecRequestBodyLimit 13107200           # 12.5 MB maximum body size
SecRequestBodyNoFilesLimit 131072      # 128 KB for non-file POST data
SecRequestBodyInMemoryLimit 131072     # Buffer up to 128 KB in RAM before disk spill
SecRequestBodyLimitAction Reject

# Response Body Handling (Disabled for performance unless PII leak checks needed)
SecResponseBodyAccess Off

# File Upload Storage
SecTmpDir /tmp/
SecDataDir /tmp/

# Audit Logging Configuration
SecAuditEngine RelevantOnly
SecAuditLogRelevantStatus "^(?:5|4(?!04))"
SecAuditLogParts ABIJDEFHZ
SecAuditLogType Serial
SecAuditLog /var/log/nginx/modsec_audit.log
SecAuditLogStorageDir /var/log/nginx/modsec_audit/

# Default Denial Action
SecDefaultAction "phase:2,deny,log,status:403"
```

### Example 4: Tuning Anomaly Score Thresholds in `crs-setup.conf`

```apache
# /etc/nginx/modsec/coreruleset/crs-setup.conf

# Paranoia Level 1 (Default: safe baseline)
SecAction \
  "id:900000,\
   phase:1,\
   nolog,\
   pass,\
   t:none,\
   setvar:tx.paranoia_level=1"

# Inbound Anomaly Threshold (5 is standard; raise to 10 if experiencing false positives)
SecAction \
  "id:900110,\
   phase:1,\
   nolog,\
   pass,\
   t:none,\
   setvar:tx.inbound_anomaly_score_threshold=5,\
   setvar:tx.outbound_anomaly_score_threshold=4"
```

### Example 5: Virtual Host with Granular WAF Bypass for Webhooks

Disable WAF on high-throughput, cryptographically signed webhooks (e.g. Stripe, GitHub) to avoid false positives and reduce CPU overhead:

```nginx
# /etc/nginx/conf.d/api.example.com.conf

server {
    listen 443 ssl;
    server_name api.example.com;

    ssl_certificate /etc/letsencrypt/live/api.example.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/api.example.com/privkey.pem;

    # WAF enabled by default for public API
    modsecurity on;
    modsecurity_rules_file /etc/nginx/modsec/main.conf;

    location / {
        proxy_pass http://127.0.0.1:8000;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
    }

    # Bypass WAF on signed webhook endpoint
    location /webhooks/stripe {
        modsecurity off; # Disable WAF inspection on this path
        proxy_pass http://127.0.0.1:8000;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
    }
}
```

---

## 6. Tuning, Rule Exclusions & Managing False Positives

A WAF without tuning will break legitimate production traffic. Managing false positives requires structured exclusion rules rather than disabling the engine:

### 1. Identifying the Offending Rule ID
Inspect `/var/log/nginx/modsec_audit.log` when a legitimate client receives HTTP 403:
```json
{
  "transaction": {
    "client_ip": "203.0.113.10",
    "uri": "/api/v1/posts/update",
    "messages": [
      {
        "message": "SQL Injection Attack Detected via libinjection",
        "data": "Matched Data: 1' OR '1'='1 found within ARGS:content",
        "ruleId": 942100
      }
    ]
  }
}
```

### 2. Writing Targeted Rule Exclusions (`custom-exclusions.conf`)

Never disable Rule `942100` globally! Instead, whitelist only the specific variable on that specific URI:

```apache
# /etc/nginx/modsec/custom-exclusions.conf

# Example 1: Exclude a specific parameter on a specific path
SecRule REQUEST_URI "@streq /api/v1/posts/update" \
    "id:10001,\
     phase:1,\
     pass,\
     nolog,\
     ctl:ruleRemoveTargetById=942100;ARGS:content"

# Example 2: Whitelist internal IP range from WAF rules entirely
SecRule REMOTE_ADDR "@ipMatch 10.0.0.0/8" \
    "id:10002,\
     phase:1,\
     pass,\
     nolog,\
     ctl:ruleEngine=Off"

# Example 3: Remove false positive on administrative dashboard path
SecRule REQUEST_URI "@beginsWith /admin/" \
    "id:10003,\
     phase:1,\
     pass,\
     nolog,\
     ctl:ruleRemoveById=920272"
```

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern / Mistake | Root Cause | Observable Production Impact | Correct Solution |
| :--- | :--- | :--- | :--- |
| Deploying with `SecRuleEngine On` on Day 1 | Enforcing blocking before baseline traffic analysis | Legitimate user requests blocked; API outages | Start in `DetectionOnly` for 2–4 weeks, audit logs, then switch to `On` |
| Disabling Rule IDs Globally | Disabling e.g. `SecRuleRemoveById 942100` in global conf | Leaves the entire application vulnerable to SQLi on all routes | Use `ctl:ruleRemoveTargetById` scoped to specific URIs and parameters |
| Enabling `SecResponseBodyAccess On` on API gateways | Buffering large JSON/media responses for regex inspection | Severe memory consumption and $p99$ latency spikes | Keep `SecResponseBodyAccess Off` unless auditing credit card numbers |
| Neglecting Body Size Limits | Leaving `SecRequestBodyInMemoryLimit` at tiny defaults (8KB) | Large JSON payloads spill to `/tmp/`, causing disk I/O thrashing | Tune `SecRequestBodyInMemoryLimit` to match standard API payload distributions |
| Missing `phase:1` in Exclusions | Placing exclusion rules that run in phase 2 after rules have triggered | Exclusions fail to prevent anomaly score accumulation | Always define rule target exclusions in `phase:1` or `phase:2` appropriately |
| Forgetting Multipart Temp File Cleanup | ModSecurity creates temp files for file uploads without purging | `/tmp` fills to 100%, causing server-wide system failure | Configure tmpfs for `SecTmpDir` and verify cleanup directives |

---

## 8. Performance & Latency Overhead Analysis

Adding deep packet and regex inspection incurs quantifiable compute costs:
- **Latency Impact**: An untuned OWASP CRS evaluates 200+ regular expressions per request. Typical added latency ranges from **1.5ms to 8ms** per transaction depending on CPU speed and payload size.
- **CPU Overhead**: CPU utilization typically increases by **20% to 40%** under heavy WAF inspection compared to raw NGINX proxying.
- **Memory Footprint**: `libmodsecurity` requires approximately **50MB to 100MB** of resident memory per worker process for compiled regex tables and transaction contexts.

---

## 9. Architectural Comparison: ModSecurity vs Coraza vs Cloud WAF

| Dimension | ModSecurity v3 (libmodsecurity) | OWASP Coraza WAF | Cloud WAF (Cloudflare / AWS WAF) |
| :--- | :--- | :--- | :--- |
| **Implementation** | C++ standalone library | Pure Go (compatible with SecLang) | Proprietary distributed edge proxy |
| **Integration** | Dynamic C connector module | Go-based or CGO/WASM connector | DNS or Anycast IP routing |
| **Maintenance** | Trustwave handed maintenance to OWASP | Active OWASP flagship project | Managed by cloud provider |
| **Performance** | High native C++ execution speed | High Go concurrency; low memory footprint | Minimal origin impact; offloaded to edge |
| **Rule Compatibility**| Full SecLang / OWASP CRS v3 & v4 | Full SecLang / OWASP CRS v3 & v4 | Proprietary rules + managed rule sets |
| **On-Premise / Airgap**| Fully supported | Fully supported | Impossible (requires public internet) |

---

## 10. Technical Interview Questions & Scenarios

<details>
<summary><strong>1. Explain the difference between "Traditional Rule Matching" and "Anomaly Scoring" in the OWASP Core Rule Set.</strong></summary>

In traditional rule matching, a rule immediately executes a disruptive action (such as `deny` with status 403) the moment any condition or regex matches. This produces high rates of false positives because a single innocent word in a search field matching an SQL keyword will instantly drop the connection. In contrast, Anomaly Scoring assigns points to matched rules based on severity (e.g. Critical = 5 points, Warning = 3 points). Matched rules do not block immediately; instead, they increment an internal variable (`tx.inbound_anomaly_score`). At the end of the request inspection phase, an evaluation rule checks if the accumulated score exceeds `tx.inbound_anomaly_score_threshold` (e.g., 5). This allows a transaction to trigger a benign warning without interruption, while transactions showing multiple concurrent suspicious patterns are blocked.
</details>

<details>
<summary><strong>2. How does ModSecurity v3 differ architecturally from ModSecurity v2?</strong></summary>

ModSecurity v2 was written as an Apache HTTP Server module (`mod_security2.c`). Porting it to NGINX required an emulation wrapper that translated Apache internal data structures and hooks, creating stability issues, performance bottlenecks, and maintenance challenges. ModSecurity v3 (`libmodsecurity`) was completely rewritten from scratch in C++ as a clean, standalone, protocol-agnostic library. It has zero dependencies on Apache. Web servers like NGINX connect to it using a lightweight connector (`ModSecurity-nginx`), significantly reducing memory overhead and decoupling rule execution from web server lifecycles.
</details>

<details>
<summary><strong>3. What is "Virtual Patching" and how is it implemented in NGINX using SecRule?</strong></summary>

Virtual patching is the rapid deployment of a security policy at the reverse proxy layer to prevent the exploitation of a known vulnerability without modifying the underlying application source code. For example, during the Log4Shell vulnerability (CVE-2021-44228), organizations wrote an immediate ModSecurity rule:
```apache
SecRule REQUEST_HEADERS|REQUEST_BODY|REQUEST_URI "@rx \$\{jndi:(?:ldap|rmi|dns)" \
    "id:999999,phase:2,deny,status:403,log,msg:'Log4Shell Exploit Attempt'"
```
This intercepted the exploit strings at the edge within minutes of disclosure, protecting unpatched application backends while engineering teams spent weeks upgrading dependencies.
</details>

<details>
<summary><strong>4. Why should `SecResponseBodyAccess` generally be disabled on high-throughput API gateways?</strong></summary>

When `SecResponseBodyAccess` is set to `On`, NGINX cannot stream responses from the upstream server directly to the downstream client. Instead, it must buffer the entire response body in memory or on disk until the upstream finishes, pass the complete payload through the ModSecurity regex engine (Phase 4), and only then release it. This destroys streaming endpoints (SSE, video, large JSON payloads), inflates time-to-first-byte (TTFB), and dramatically increases RAM usage under high concurrency.
</details>

<details>
<summary><strong>5. How do you safely tune a WAF in a production environment without causing outages?</strong></summary>

1. Deploy NGINX with `SecRuleEngine DetectionOnly;`.
2. Direct 100% of production traffic through the instance. Offending requests are flagged in `modsec_audit.log` but never blocked.
3. Ingest audit logs into an aggregation platform (e.g., Elasticsearch, Datadog) for 2 to 4 weeks.
4. Identify all false positives triggered by legitimate business workflows.
5. Author targeted `ctl:ruleRemoveTargetById` exclusions in `custom-exclusions.conf`.
6. Once false positive rates drop to zero, switch `SecRuleEngine On;` in staging, verify, and then enable in production.
</details>

<details>
<summary><strong>6. What is Paranoia Level in CRS and what are the trade-offs of increasing it?</strong></summary>

Paranoia Level (PL1 to PL4) controls the aggressiveness of the OWASP Core Rule Set. PL1 contains baseline rules designed to have virtually no false positives on modern web applications. As the paranoia level increases, rules introduce stricter constraints, such as enforcing strict ASCII character sets on arguments, banning standard punctuation, and flagging unusual HTTP header combinations. While PL4 provides defense against sophisticated evasion attacks, it will break standard REST APIs, rich text editors, and base64 payloads unless heavily tuned with custom exclusions.
</details>

<details>
<summary><strong>7. How can an attacker bypass a poorly written WAF regex using character encoding?</strong></summary>

Attackers use multiple layers of encoding (e.g., double URL encoding `%2527`, mixed hex/unicode, or comment splitting in SQL `UN/**/ION`) to disguise signatures. ModSecurity counters this through transformation pipelines (`t:none`, `t:urlDecodeUni`, `t:lowercase`, `t:removeNulls`). If a rule author writes a regex without applying appropriate transformation functions, the raw encoded characters bypass the regex but are later decoded by the application runtime.
</details>

<details>
<summary><strong>8. What is OWASP Coraza and why are teams evaluating it as an alternative to ModSecurity?</strong></summary>

OWASP Coraza is a modern, enterprise-ready Web Application Firewall written in pure Go. Because ModSecurity v3 has experienced slow development velocity after Trustwave ended commercial support, Coraza was built to provide full SecLang and CRS compatibility with memory safety (preventing C++ memory corruption exploits), native WASM (WebAssembly) support for Envoy and Istio, and direct integration into modern cloud-native architectures.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Verify ModSecurity Dynamic Module Loading</strong></summary>

**Goal**: Confirm the connector module is active in NGINX.
1. Run `nginx -V 2>&1 | grep modsecurity` to verify build flags.
2. In `nginx.conf`, ensure `load_module modules/ngx_http_modsecurity_module.so;` is present.
3. Test configuration syntax: `sudo nginx -t`.
</details>

<details>
<summary><strong>Exercise 2: Trigger a Synthetic SQL Injection and Inspect Audit Logs</strong></summary>

**Goal**: Validate that CRS intercepts an injection payload.
1. Configure `SecRuleEngine On`.
2. Send a malicious payload via `curl`:
```bash
curl -i "http://localhost/api/users?id=1%20UNION%20SELECT%20username,%20password%20FROM%20users"
```
3. Confirm NGINX returns `HTTP/1.1 403 Forbidden`.
4. Inspect `/var/log/nginx/modsec_audit.log` and identify the matched `ruleId` (e.g., 942100).
</details>

<details>
<summary><strong>Exercise 3: Write a Granular Parameter Exclusion</strong></summary>

**Goal**: Allow a specific parameter containing SQL-like syntax without disabling the rule globally.
1. Create a dummy search endpoint `/search`.
2. Add an exclusion rule in `custom-exclusions.conf`:
```apache
SecRule REQUEST_URI "@streq /search" \
    "id:20001,phase:1,pass,nolog,ctl:ruleRemoveTargetById=942100;ARGS:query"
```
3. Reload NGINX and verify `curl "http://localhost/search?query=1%20OR%201=1"` returns HTTP 200 while `/api/users` still returns HTTP 403.
</details>

<details>
<summary><strong>Exercise 4: Test Path Traversal and Command Injection Shields</strong></summary>

**Goal**: Verify protection against LFI and RCE attempts.
1. Test LFI attempt:
```bash
curl -i "http://localhost/download?file=../../../../etc/passwd"
```
2. Test RCE attempt:
```bash
curl -i "http://localhost/ping?host=127.0.0.1;cat%20/etc/shadow"
```
3. Verify both attempts are rejected with HTTP 403.
</details>

<details>
<summary><strong>Exercise 5: Benchmark Latency Overhead with `wrk`</strong></summary>

**Goal**: Quantify the latency added by WAF regex evaluation.
1. Test throughput with WAF off: `modsecurity off;` -> run `wrk -t4 -c50 -d10s http://localhost/`.
2. Enable WAF: `modsecurity on;` -> re-run `wrk`.
3. Compare requests/second and $p99$ latency figures.
</details>

<details>
<summary><strong>Exercise 6: Deploy a Virtual Patch for a Mock CVE</strong></summary>

**Goal**: Block an arbitrary attack vector immediately using an inline rule.
1. Add an inline rule in `server {}`:
```nginx
modsecurity_rules '
    SecRule ARGS:cve_test "@streq exploit" "id:999001,phase:2,deny,status:403,log,msg:\'Mock CVE Exploit Blocked\'"
';
```
2. Reload NGINX and verify `curl http://localhost/?cve_test=exploit` is intercepted.
</details>

---

## 12. Summary & Quick Reference

```
+--------------------------------------------------------------------------------+
| WAF / ModSecurity Architecture & Directive Cheat Sheet                         |
|                                                                                |
| Directive                     | Context         | Purpose                      |
| :---------------------------- | :-------------- | :--------------------------- |
| modsecurity on;               | http, srv, loc  | Enables WAF engine           |
| modsecurity_rules_file <file>;| http, srv, loc  | Includes master ruleset      |
| SecRuleEngine On              | modsec conf     | Enforces blocking (403)      |
| SecRuleEngine DetectionOnly   | modsec conf     | Logs only (no blocking)      |
| SecRequestBodyAccess On       | modsec conf     | Enables POST/PUT inspection  |
| SecResponseBodyAccess Off     | modsec conf     | Disables output inspection   |
| ctl:ruleRemoveTargetById=N;VAR| custom rule     | Excludes single variable     |
| ctl:ruleEngine=Off            | custom rule     | Disables WAF for matching IP |
+--------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [OWASP ModSecurity Core Rule Set Documentation](https://coreruleset.org/)
- [ModSecurity Official Reference Manual](https://github.com/owasp-modsecurity/ModSecurity/wiki/Reference-Manual-(v3.x))
- [OWASP Coraza Web Application Firewall](https://coraza.io/)
- [Next Concept: DDoS Mitigation & Rate Limiting Architecture](04-ddos-mitigation-patterns.md)

---

[← Previous Note: 02-security-hardening.md](02-security-hardening.md) | [Back to Module Index](README.md) | [Next Note: 04-ddos-mitigation-patterns.md →](04-ddos-mitigation-patterns.md)
