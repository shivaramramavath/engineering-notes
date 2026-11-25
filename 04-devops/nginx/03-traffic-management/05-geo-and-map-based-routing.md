---
title: "GeoIP and Map-Based Dynamic Routing in NGINX"
description: "Production guide to geographic traffic steering: CIDR matching with geo, MaxMind GeoIP2/GeoLite2 databases, GDPR data sovereignty routing, and multi-variable map compositions."
module: "03-traffic-management"
track: "devops-nginx"
order: 5
difficulty: "Intermediate to Advanced"
prerequisites:
  - "01-fundamentals/03-server-and-location"
  - "02-core-features/04-variables-and-rewrites"
  - "03-traffic-management/02-load-balancing"
tags:
  - nginx
  - geoip
  - geo
  - map
  - routing
  - gdpr
  - traffic-steering
last_updated: "2026-03-30"
---

# GeoIP and Map-Based Dynamic Routing in NGINX

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Directives Reference](#4-syntax--directives-reference)
- [5. Practical Implementation Examples](#5-practical-implementation-examples)
  - [Example 1: Internal Subnet Classification via `geo`](#example-1-internal-subnet-classification-via-geo)
  - [Example 2: Regional Upstream Steering with MaxMind GeoIP2](#example-2-regional-upstream-steering-with-maxmind-geoip2)
  - [Example 3: GDPR Data Sovereignty Routing to EU Datacenters](#example-3-gdpr-data-sovereignty-routing-to-eu-datacenters)
  - [Example 4: Geofencing & Country Blocking (Sanctions Compliance)](#example-4-geofencing--country-blocking-sanctions-compliance)
  - [Example 5: Composite Multi-Variable Routing (`$country:$language`)](#example-5-composite-multi-variable-routing-countrylanguage)
- [6. Production Best Practices](#6-production-best-practices)
- [7. Common Pitfalls & Anti-Patterns](#7-common-pitfalls--anti-patterns)
- [8. Performance, Security & Troubleshooting](#8-performance-security--troubleshooting)
- [9. Architectural Comparison Table](#9-architectural-comparison-table)
- [10. Technical Interview Questions](#10-technical-interview-questions)
- [11. Hands-On Practice Exercises](#11-hands-on-practice-exercises)
- [12. Summary & Quick Reference](#12-summary--quick-reference)
- [13. Further Reading & Official Documentation](#13-further-reading--official-documentation)

---

## 1. What & Why

In global web architectures, treating all client requests identically regardless of physical origin creates latency bottlenecks and regulatory compliance violations. Global platforms must adapt to client geography to:
1. **Optimize Network Latency**: Route clients dynamically to the nearest regional datacenter (e.g., US West, Europe Central, Asia Pacific) to minimize latency.
2. **Enforce Legal & Regulatory Sovereignty**: Under GDPR, European user data must reside within EU boundaries; under international trade sanctions (OFAC), access from sanctioned jurisdictions must be blocked.
3. **Localize User Experiences**: Redirect users to localized storefronts (e.g., `/en-gb/`, `/ja-jp/`) or default currencies based on detected country and browser language.
4. **Distinguish Corporate vs Public Traffic**: Classify requests originating from internal corporate subnets or cloud VPC CIDRs to grant elevated administrative privileges or bypass rate limits.

NGINX provides two core engines for this purpose:
- **`ngx_http_geo_module`**: A built-in, lightning-fast CIDR network matching module that maps IP blocks to custom variable values using a compiled radix tree.
- **`ngx_http_geoip2_module`**: A modern dynamic module that reads binary MaxMind GeoIP2 / GeoLite2 (`.mmdb`) databases to resolve real-world country, city, continent, and Autonomous System Numbers (ASN).

---

## 2. Core Concepts

### The `geo` Directive (CIDR Range Matching)
The `geo` directive is declared exclusively in the `http {}` context. It takes an input IP address (defaulting to `$remote_addr`) and outputs a new user-defined variable based on CIDR block matches:
```nginx
geo $network_class {
    default        external;
    127.0.0.1/32   loopback;
    10.0.0.0/8     internal_vpc;
    192.168.0.0/16 corporate_lan;
}
```
NGINX compiles the entries into a **radix tree (patricia trie)** at startup, ensuring that resolving an IP address takes $O(k)$ bitwise operations (where $k=32$ for IPv4 and $128$ for IPv6) regardless of whether the table contains 10 or 100,000 subnets.

### The MaxMind GeoIP2 Engine (`.mmdb`)
Historically, NGINX used the legacy GeoIP module (`.dat` files), which MaxMind deprecated. Modern deployments utilize **GeoIP2**, which parses `.mmdb` (MaxMind DB) binary databases via `libmaxminddb`.

The `.mmdb` database is memory-mapped (`mmap`) directly into the NGINX worker process address space. Lookups extract fine-grained geographic properties:
- Country Code: `country iso_code` (e.g., `US`, `DE`, `JP`, `GB`)
- Continent Code: `continent code` (e.g., `NA`, `EU`, `AS`)
- City Name: `city names en` (e.g., `London`, `Tokyo`, `Frankfurt`)
- ASN: `autonomous_system_number` (e.g., `AS15169` Google, `AS16509` Amazon)

### Multi-Variable Map Compositions
While `geo` maps an IP to a single variable, combining multiple dimensions (e.g., Geographic Location + HTTP Method + User-Agent) is achieved by concatenating variables into a single composite key inside a `map`:
```nginx
map "$client_country:$request_method" $routing_policy {
    default           "standard_backend";
    "EU:POST"         "eu_strict_storage";
    "~^(US|CA):GET"   "us_cached_read";
}
```

---

## 3. How It Works Internally

### Radix Tree Bitwise Lookup in `ngx_http_geo_module`

```
Client IP arrives: 10.150.2.45
Binary representation: 00001010 10010110 00000010 00101101
                          |
                          v
         Traverse Radix Tree Bit-by-Bit
                     (Root)
                    /      \
                   0        1
                  /
                 0
                /
               00001010... -> Matches 10.0.0.0/8!
                          |
                          v
        Assigned Value: $network_class = "internal_vpc"
        Execution Time: < 0.001 ms (Constant Time)
```

### Dynamic Multi-Datacenter Routing Flow

```
Public Client (London, UK)
           |
           v
[NGINX Edge Router]
           |
           +---> 1. Query GeoIP2 MMDB: Country = "GB", Continent = "EU"
           +---> 2. Evaluate map: $target_datacenter = "eu_west_cluster"
           |
           v
+------------------------------------+
| proxy_pass http://$target_cluster  |
+------------------+-----------------+
                   |
     +-------------+-------------+
     |                           |
     v                           v
[US-East Datacenter]       [EU-West Datacenter (Ireland)]
(High Latency: 120ms)      (Low Latency: 15ms)
                             <=== Traffic Routed Here!
```

---

## 4. Syntax & Directives Reference

### `ngx_http_geo_module` Directives
| Directive | Context | Default | Purpose / Details |
| :--- | :--- | :--- | :--- |
| `geo [$address] $variable { ... }` | `http` | None | Maps an IP address (default `$remote_addr`) to a variable via CIDR blocks. |
| `ranges` (parameter inside `geo`) | `geo block` | None | Indicates that entries are specified as IP ranges (`192.168.0.1-192.168.0.254`) rather than CIDR. |
| `include file` | `geo block` | None | Includes an external file containing IP mappings. |
| `proxy address` | `geo block` | None | Defines trusted proxy IP; NGINX inspects `X-Forwarded-For` address instead. |
| `proxy_recursive` | `geo block` | None | Enables recursive searching through `X-Forwarded-For` proxy chains. |

### `ngx_http_geoip2_module` Directives
| Directive | Context | Default | Purpose / Details |
| :--- | :--- | :--- | :--- |
| `geoip2 file { $variable source ...; }` | `http` | None | Binds an `.mmdb` database and maps MaxMind schema paths to NGINX variables. |

---

## 5. Practical Implementation Examples

### Example 1: Internal Subnet Classification via `geo`

Classify traffic into development, corporate, cloud, and public tiers, selectively enabling administrative debugging endpoints.

```nginx
# /etc/nginx/conf.d/geo_classification.conf

# Map client IP to environment tier
geo $client_tier {
    default          "public_internet";
    127.0.0.1/32      "local";
    10.0.0.0/8        "internal_vpc";
    172.16.0.0/12     "container_mesh";
    192.168.1.0/24    "hq_office";
    198.51.100.10/32  "lead_architect_vpn";
}

server {
    listen 80;
    server_name portal.example.com;

    # Expose tier classification in response header
    add_header X-Client-Tier $client_tier always;

    location / {
        proxy_pass http://app_upstream;
        proxy_set_header Host $host;
    }

    # Administrative debugging route: Strictly restricted to trusted tiers
    location /debug/metrics {
        if ($client_tier = "public_internet") {
            return 403 "Access Denied: Internal Networks Only\n";
        }

        proxy_pass http://metrics_upstream;
    }
}
```

### Example 2: Regional Upstream Steering with MaxMind GeoIP2

Steer clients to the geographically closest origin server based on country code.

```nginx
# /etc/nginx/conf.d/regional_steering.conf

# 1. Load GeoIP2 City Database
geoip2 /usr/share/GeoIP/GeoLite2-Country.mmdb {
    auto_reload 1h; # Automatically re-read mmdb on disk update
    $geoip2_country_code default=US source=$remote_addr country iso_code;
    $geoip2_continent    default=NA source=$remote_addr continent code;
}

# 2. Map Continent to Regional Upstream Pool
map $geoip2_continent $regional_upstream {
    default   us_east_origin;
    "NA"      us_east_origin;      # North America
    "EU"      eu_central_origin;   # Europe
    "AS"      ap_southeast_origin; # Asia
    "OC"      ap_southeast_origin; # Oceania
    "SA"      sa_east_origin;      # South America
}

upstream us_east_origin {
    server 10.0.1.10:8000;
}

upstream eu_central_origin {
    server 10.0.2.10:8000;
}

upstream ap_southeast_origin {
    server 10.0.3.10:8000;
}

upstream sa_east_origin {
    server 10.0.4.10:8000;
}

server {
    listen 443 ssl;
    http2 on;
    server_name api.global-platform.net;

    ssl_certificate     /etc/ssl/certs/global.crt;
    ssl_certificate_key /etc/ssl/private/global.key;

    location / {
        proxy_pass http://$regional_upstream;
        proxy_set_header Host $host;
        proxy_set_header X-Client-Country $geoip2_country_code;
        proxy_set_header X-Client-Continent $geoip2_continent;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;

        add_header X-Routed-Datacenter $regional_upstream always;
    }
}
```

### Example 3: GDPR Data Sovereignty Routing to EU Datacenters

Enforces strict European data sovereignty: any transactional write (`POST`, `PUT`, `DELETE`) originating from an EU country is guaranteed to execute exclusively on European infrastructure.

```nginx
# /etc/nginx/conf.d/gdpr_compliance.conf

# Map ISO Country Code to GDPR Scope
map $geoip2_country_code $is_gdpr_country {
    default 0;
    # European Union Member States + EEA
    "~^(AT|BE|BG|HR|CY|CZ|DK|EE|FI|FR|DE|GR|HU|IE|IT|LV|LT|LU|MT|NL|PL|PT|RO|SK|SI|ES|SE|NO|IS|LI|GB)$" 1;
}

# Combine GDPR flag and HTTP Method
map "$is_gdpr_country:$request_method" $target_cluster {
    default             us_cluster;
    "1:POST"            eu_secure_database_cluster;
    "1:PUT"             eu_secure_database_cluster;
    "1:DELETE"          eu_secure_database_cluster;
    "1:PATCH"           eu_secure_database_cluster;
    "1:GET"             eu_read_replica_cluster;
}

upstream us_cluster {
    server 10.10.1.10:8080;
}

upstream eu_secure_database_cluster {
    server 10.20.1.10:8080;
}

upstream eu_read_replica_cluster {
    server 10.20.2.10:8080;
}

server {
    listen 80;
    server_name store.example.com;

    location /api/ {
        proxy_pass http://$target_cluster;
        proxy_set_header Host $host;
        add_header X-GDPR-Enforced $is_gdpr_country always;
    }
}
```

### Example 4: Geofencing & Country Blocking (Sanctions Compliance)

Blocking connections from OFAC-sanctioned jurisdictions or countries where your service has no licensing, returning HTTP 451 Unavailable For Legal Reasons.

```nginx
# /etc/nginx/conf.d/sanctions_geofence.conf

geoip2 /usr/share/GeoIP/GeoLite2-Country.mmdb {
    $client_country default=UNKNOWN source=$remote_addr country iso_code;
}

map $client_country $is_blocked_jurisdiction {
    default 0;
    # Sanctioned country codes (Example: CU, IR, KP, SY)
    "CU" 1; # Cuba
    "IR" 1; # Iran
    "KP" 1; # North Korea
    "SY" 1; # Syria
}

server {
    listen 80;
    server_name cloud.example.com;

    location / {
        if ($is_blocked_jurisdiction = 1) {
            # RFC 7725: Unavailable For Legal Reasons
            return 451 '{"error": 451, "message": "Service unavailable in your jurisdiction due to legal/regulatory compliance."}\n';
        }

        proxy_pass http://cloud_backend;
        proxy_set_header Host $host;
    }
}
```

### Example 5: Composite Multi-Variable Routing (`$country:$language`)

Combines detected country and client language preference to redirect visitors to localized homepages cleanly.

```nginx
# /etc/nginx/conf.d/localized_routing.conf

# Extract primary preferred language from Accept-Language header
map $http_accept_language $preferred_lang {
    default           "en";
    "~*^ja"           "ja";
    "~*^de"           "de";
    "~*^fr"           "fr";
    "~*^es"           "es";
}

# Combine Country and Language into a clean redirection target
map "$geoip2_country_code:$preferred_lang" $localized_path {
    default           "/en/";
    "JP:ja"           "/ja/";
    "DE:de"           "/de/";
    "FR:fr"           "/fr/";
    "ES:es"           "/es/";
    "~^GB:.*"         "/en-gb/";
}

server {
    listen 80;
    server_name www.example.com;

    location = / {
        # Redirect root visitors to their localized portal
        return 302 $localized_path;
    }

    location / {
        root /var/www/multilingual;
        try_files $uri $uri/ /index.html;
    }
}
```

---

## 6. Production Best Practices

1. **Always Define a `default` in `geo` and `map`**: Never omit `default`. If an incoming client IP does not match any CIDR block or database entry, NGINX must have an explicit fallback (e.g., `default US;` or `default external;`).
2. **Automate MaxMind Database Updates**: GeoIP databases change weekly as telecom providers allocate new IP ranges. Use MaxMind's `geoipupdate` cron utility to download updated `.mmdb` files, and specify `auto_reload 1h;` in your NGINX `geoip2` block.
3. **Configure `proxy` and `proxy_recursive` in `geo` Behind CDNs**: If NGINX sits behind Cloudflare, AWS ALB, or Akamai, `$remote_addr` is the proxy's IP. In `geo {}`, define `proxy <CDN_CIDR>;` and `proxy_recursive;` so NGINX automatically inspects the true client IP in `X-Forwarded-For`.
4. **Use HTTP 451 for Legal Blocking**: When blocking countries for regulatory, copyright, or sanctions reasons, use RFC 7725 status code `451 Unavailable For Legal Reasons` instead of `403 Forbidden` to provide transparent legal clarity.
5. **Keep Geo Mappings External for Clean Configs**: For large CIDR lists (e.g., 5,000 corporate subnets), use `include /etc/nginx/conf.d/corporate_subnets.geo;` inside the `geo {}` block to maintain readable configurations.
6. **Graceful Fallbacks for Local Testing**: When developing locally (`127.0.0.1`), GeoIP lookups will resolve to `UNKNOWN` or `null`. Always ensure your `map` defaults handle local loopbacks gracefully without 500 errors.

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Why It Fails | Correct Approach |
| :--- | :--- | :--- |
| **GeoIP lookups using `$remote_addr` behind a CDN** | Resolves the location of the CDN edge server (e.g., Ashburn, VA) instead of the actual user; 100% of global users are routed to US-East. | Use `source=$http_cf_connecting_ip` or configure `ngx_http_realip_module`. |
| **Using outdated GeoIP legacy `.dat` modules** | MaxMind discontinued legacy formats; legacy modules cannot parse modern `.mmdb` databases and fail to compile on modern Linux distros. | Use `ngx_http_geoip2_module` with modern GeoLite2 `.mmdb` files. |
| **Multiple nested `if` statements for location routing** | "If is Evil" causes directive inheritance failure or crashes when evaluating complex geographic rules. | Use `map "$var1:$var2" $result` with composite string matching. |
| **Omitting `auto_reload` on `.mmdb` files** | When `geoipupdate` updates `.mmdb` on disk, NGINX continues using the old memory-mapped database until manually reloaded. | Add `auto_reload 1h;` (or `24h;`) to the `geoip2 {}` block. |
| **Missing `default` parameter in `map`** | Unmatched keys result in an empty string `""`. If passed to `proxy_pass http://$backend;`, NGINX throws a fatal 500 configuration error. | Always supply an explicit fallback default: `default origin_us;`. |
| **Blocking countries without checking Search Bots** | Geoblocking country X blocks Googlebot or Bingbot crawling from those regions, inadvertently de-indexing content. | Add search bot exemptions to your `map` or `geo` definitions. |

---

## 8. Performance, Security & Troubleshooting

### Diagnostic CLI Commands

```bash
# 1. Test Client GeoIP Lookup via mmdblookup CLI
# (Install: apt-get install mmdb-bin)
mmdblookup --file /usr/share/GeoIP/GeoLite2-Country.mmdb --ip 8.8.8.8 country iso_code
# Expected output: "US" <utf8_string>

# 2. Test NGINX Geographic Routing via Mock IP Header
# (Simulate client connecting from Tokyo, Japan IP: 133.242.0.1)
curl -s -I -H "X-Forwarded-For: 133.242.0.1" http://api.global-platform.net/ | grep -i x-routed-datacenter

# 3. Simulate European Visitor (Frankfurt, Germany IP: 149.201.0.1)
curl -s -I -H "X-Forwarded-For: 149.201.0.1" http://api.global-platform.net/ | grep -i x-routed-datacenter

# 4. Verify Geoblocked Response (HTTP 451)
curl -s -I -H "X-Forwarded-For: 175.45.176.1" http://cloud.example.com/
# Expected: HTTP/1.1 451 Unavailable For Legal Reasons
```

### Memory Footprint of GeoIP2
- `GeoLite2-Country.mmdb`: $\approx 4\text{ MB}$ file size.
- `GeoLite2-City.mmdb`: $\approx 70\text{ MB}$ file size.
- Because `libmaxminddb` uses `mmap()`, the database is loaded into shared memory once by the OS kernel; all NGINX worker processes share the same read-only memory pages with near-zero additional RAM overhead.

---

## 9. Architectural Comparison Table

| Metric / Feature | Built-in `geo` Module | `geoip2` (`.mmdb`) | DNS-Based Geo (AWS Route 53) |
| :--- | :--- | :--- | :--- |
| **Data Source** | Hardcoded CIDR lists in config | MaxMind Binary Database | Authoritative DNS resolver IP |
| **Lookup Resolution** | Custom CIDR / Subnet | Country, City, Continent, ASN | Approximate Geo-resolver IP |
| **Accuracy** | 100% exact for internal subnets | High (99.8% Country, 80% City) | Medium (Tied to user's DNS server) |
| **HTTP Layer Awareness** | Yes (Can inspect headers/methods) | Yes (Can combine with headers) | **No** (DNS resolution only) |
| **Failover Capability** | Dynamic via NGINX `proxy_pass` | Dynamic via NGINX `proxy_pass` | Slow (Tied to DNS TTL expiration) |
| **Best For** | VPC/Office classification, IP whitelists | Global country routing, GDPR, geofence| Global edge CDN entry routing |

---

## 10. Technical Interview Questions

<details>
<summary><strong>Q1: How does the ngx_http_geo_module achieve microsecond lookup times across thousands of CIDR blocks?</strong></summary>

**Answer:**
When NGINX parses a `geo {}` block during configuration loading, it does not evaluate CIDRs sequentially. Instead, it compiles all IP addresses and subnet masks into a **radix tree (also known as a patricia trie)** in memory.

A radix tree stores binary IP prefixes hierarchically. When a request arrives, NGINX takes the client's 32-bit (IPv4) or 128-bit (IPv6) binary address and performs bitwise branching down the tree. Finding the most specific matching CIDR block requires at most 32 bit-checks for IPv4, guaranteeing constant-time $O(1)$ performance regardless of whether the configuration contains 5 subnets or 500,000 subnets.
</details>

<details>
<summary><strong>Q2: Why should you avoid DNS-based Geo-routing (like AWS Route 53 Geo DNS) for strict legal compliance like GDPR?</strong></summary>

**Answer:**
Route 53 and other Geo-DNS services inspect the IP address of the **client's recursive DNS resolver** (e.g., Google DNS `8.8.8.8` or Cloudflare `1.1.1.1`), not the client's actual device IP (unless the resolver supports and sends EDNS0 Client Subnet / ECS). 

If a user in France configures a US-based DNS resolver that strips ECS, the Geo-DNS server will incorrectly assume the user is in the United States and resolve the domain to a US datacenter IP. 

In contrast, NGINX executes Layer 7 inspection: it inspects the client's direct TCP connection IP (or verified CDN header), guaranteeing 100% accurate enforcement of data sovereignty rules at the application layer.
</details>

<details>
<summary><strong>Q3: How do you configure the geo module to read the client IP from X-Forwarded-For when deployed behind a trusted load balancer?</strong></summary>

**Answer:**
Inside the `geo {}` block, use the `proxy` and `proxy_recursive` directives:
```nginx
geo $client_tier {
    proxy 10.0.0.0/8;       # Trust internal load balancer subnet
    proxy 192.168.1.1;      # Trust specific proxy IP
    proxy_recursive;        # Walk backwards through multiple proxies
    
    default       external;
    192.168.0.0/16 corporate;
}
```
When `proxy` is declared, NGINX checks if the direct connection IP matches a trusted proxy. If it matches, NGINX automatically parses the `X-Forwarded-For` header and evaluates the `geo` rules against the real client IP instead of the proxy IP.
</details>

<details>
<summary><strong>Q4: What is the significance of the auto_reload parameter in the geoip2 configuration?</strong></summary>

**Answer:**
MaxMind GeoIP2 `.mmdb` files are binary databases that are updated periodically (often weekly). By default, NGINX opens and memory-maps the database file when the master process boots. 

If an automated cron job (such as `geoipupdate`) replaces the `.mmdb` file on disk, NGINX will continue reading the old version from memory until manually reloaded. Specifying `auto_reload 1h;` (or `24h;`) instructs NGINX's worker processes to check the file's modification timestamp on disk at the configured interval and automatically reload the memory map with zero downtime and without needing an `nginx -s reload`.
</details>

<details>
<summary><strong>Q5: What is HTTP Status Code 451, and why is it preferred over 403 for geographic blocking?</strong></summary>

**Answer:**
RFC 7725 established **HTTP 451: Unavailable For Legal Reasons** (named after Ray Bradbury's novel *Fahrenheit 451*). 

While `403 Forbidden` indicates generic access denial (such as bad credentials or insufficient permissions), `451` explicitly informs the client, automated crawlers, and legal auditors that access is restricted due to legal mandates, court orders, or government sanctions (e.g., OFAC or copyright territorial licensing). It is the recognized standard for transparent compliance.
</details>

<details>
<summary><strong>Q6: How does composite variable mapping work when routing traffic based on both country and device type?</strong></summary>

**Answer:**
Because NGINX `map` takes a single string as its input, you can concatenate multiple variables together using a delimiter (such as a colon or hyphen):
```nginx
map "$geoip2_country_code:$http_user_agent" $mobile_eu_user {
    default              0;
    "~^GB:.*Mobile.*"    1;
    "~^DE:.*Mobile.*"    1;
    "~^FR:.*Mobile.*"    1;
}
```
NGINX interpolates `$geoip2_country_code` and `$http_user_agent` at runtime into a single combined string (e.g., `"DE:Mozilla/5.0 (iPhone; ...)"`) and matches it against the map's regex patterns.
</details>

<details>
<summary><strong>Q7: What happens if an IP address cannot be found in the GeoIP2 database?</strong></summary>

**Answer:**
If an IP address is not present in the `.mmdb` database (such as private RFC 1918 addresses `10.0.0.0/8`, `192.168.0.0/16`, or local loopbacks `127.0.0.1`), the mapped variable evaluates to the value specified in the `default=` parameter of the `geoip2` directive:
```nginx
geoip2 /path/to/db.mmdb {
    $country default=UNKNOWN source=$remote_addr country iso_code;
}
```
If `default=` is omitted, the variable evaluates to an empty string `""`.
</details>

<details>
<summary><strong>Q8: How do you safely update GeoIP databases on disk without corrupting active NGINX worker reads?</strong></summary>

**Answer:**
Because NGINX workers read the `.mmdb` file via memory mapping (`mmap`), overwriting the file in place with a direct copy (`cp new.mmdb current.mmdb`) can corrupt active memory pointers and crash worker processes. 

The correct procedure is to use an **atomic move**:
```bash
# Download to temporary file first:
curl -o /tmp/GeoLite2-Country.mmdb.tmp https://...
# Atomic rename:
mv /tmp/GeoLite2-Country.mmdb.tmp /usr/share/GeoIP/GeoLite2-Country.mmdb
```
Under POSIX filesystem semantics, `mv` performs an atomic `rename()` system call: the old file inode remains in memory for existing processes while new reads bind to the new inode seamlessly.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Create an Internal vs External IP Classifier</strong></summary>

**Goal:** Configure a `geo` block classifying `127.0.0.1` and `10.0.0.0/8` as `internal`, and everything else as `external`.

**Solution:**
```nginx
geo $client_zone {
    default       external;
    127.0.0.1/32  internal;
    10.0.0.0/8    internal;
}

server {
    listen 80;
    server_name test.local;

    location / {
        add_header X-Client-Zone $client_zone always;
        return 200 "Client Zone: $client_zone\n";
    }
}
```
Test with `curl -i http://localhost/` -> verifies `Client Zone: internal`.
</details>

<details>
<summary><strong>Exercise 2: Implement Dynamic Upstream Switching via Map</strong></summary>

**Goal:** Route requests with header `X-Region: APAC` to `apac_cluster`, and default to `us_cluster`.

**Solution:**
```nginx
map $http_x_region $target_pool {
    default    us_cluster;
    "APAC"     apac_cluster;
    "EMEA"     emea_cluster;
}

upstream us_cluster { server 127.0.0.1:8001; }
upstream apac_cluster { server 127.0.0.1:8002; }
upstream emea_cluster { server 127.0.0.1:8003; }

server {
    listen 80;
    location / {
        proxy_pass http://$target_pool;
    }
}
```
</details>

<details>
<summary><strong>Exercise 3: Configure Country-Based Geoblocking Returning 451</strong></summary>

**Goal:** Block requests from country code `XX` with an HTTP 451 JSON response.

**Solution:**
```nginx
map $geoip2_country_code $blocked_country {
    default 0;
    "XX"    1;
    "YY"    1;
}

server {
    listen 80;
    location / {
        if ($blocked_country = 1) {
            default_type application/json;
            return 451 '{"error": 451, "message": "Access restricted by law in your region."}\n';
        }
        return 200 "Access Granted\n";
    }
}
```
</details>

<details>
<summary><strong>Exercise 4: Combine Accept-Language with Country Code for Redirection</strong></summary>

**Goal:** Redirect visitors from Germany (`DE`) requesting German (`de`) to `/de/`.

**Solution:**
```nginx
map "$geoip2_country_code:$http_accept_language" $locale_redirect {
    default           "";
    "~^DE:.*de.*"     "/de/";
    "~^FR:.*fr.*"     "/fr/";
}

server {
    listen 80;
    location = / {
        if ($locale_redirect != "") {
            return 302 $locale_redirect;
        }
        return 200 "Standard English Portal\n";
    }
}
```
</details>

<details>
<summary><strong>Exercise 5: Configure Trusted Proxy Handling in the geo Directive</strong></summary>

**Goal:** Configure `geo` to inspect the real client IP inside `X-Forwarded-For` when forwarded by an AWS ALB at `10.0.1.50`.

**Solution:**
```nginx
geo $real_client_type {
    proxy 10.0.1.50;
    proxy_recursive;

    default       public_client;
    192.168.1.0/24 office_network;
}
```
</details>

<details>
<summary><strong>Exercise 6: Test Atomic MMDB Database Swapping</strong></summary>

**Goal:** Simulate an automated database update using atomic file replacement.

**Solution:**
```bash
# 1. Download or copy new database to temp file
cp /usr/share/GeoIP/GeoLite2-Country.mmdb /tmp/GeoLite2-Country.mmdb.new

# 2. Perform atomic replace
mv /tmp/GeoLite2-Country.mmdb.new /usr/share/GeoIP/GeoLite2-Country.mmdb

# 3. Verify file permissions are readable by nginx worker (chmod 644)
chmod 644 /usr/share/GeoIP/GeoLite2-Country.mmdb
```
</details>

---

## 12. Summary & Quick Reference

```nginx
# Production GeoIP & Map-Based Routing Cheat Sheet

# 1. Built-in CIDR Classification (http context)
geo $client_network {
    default        external;
    127.0.0.1/32   loopback;
    10.0.0.0/8     vpc_internal;
    192.168.1.0/24 corporate;
}

# 2. MaxMind GeoIP2 Integration
geoip2 /usr/share/GeoIP/GeoLite2-Country.mmdb {
    auto_reload 1h;
    $client_country default=US source=$remote_addr country iso_code;
    $client_continent default=NA source=$remote_addr continent code;
}

# 3. Dynamic Upstream Steering Map
map $client_continent $regional_backend {
    default  origin_us;
    "EU"     origin_eu;
    "AS"     origin_apac;
}

# 4. Geofencing Map (Legal Compliance)
map $client_country $is_sanctioned {
    default 0;
    "CU"    1;
    "IR"    1;
    "KP"    1;
}

server {
    location / {
        if ($is_sanctioned = 1) {
            return 451 "Unavailable For Legal Reasons\n";
        }
        proxy_pass http://$regional_backend;
        proxy_set_header Host $host;
    }
}
```

---

## 13. Further Reading & Official Documentation
- [Official NGINX ngx_http_geo_module Documentation](https://nginx.org/en/docs/http/ngx_http_geo_module.html)
- [MaxMind GeoIP2 NGINX Module (GitHub)](https://github.com/leev/ngx_http_geoip2_module)
- [RFC 7725: An HTTP Status Code to Report Legal Obstacles (HTTP 451)](https://datatracker.ietf.org/doc/html/rfc7725)
- [MaxMind GeoLite2 Free Geolocation Data](https://dev.maxmind.com/geoip/geolite2-free-geolocation-data)

---

[← Previous: Auth Request & SSO Gateways](04-auth-request-and-sso-gateways.md) | [Module Overview](README.md) | [Next: Canary and Blue-Green Deployments →](06-canary-and-blue-green-deployments.md)
