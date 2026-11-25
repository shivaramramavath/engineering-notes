---
title: "Cheatsheet: NGINX Location Matching Priority & Decision Tables"
description: "Quick reference tables and evaluation algorithms for NGINX location matching modifiers: exact (=), non-regex prefix (^~), regular expressions (~/~*), and standard prefix matching."
module: "09-cheatsheets"
topic: "02-location-matching-table"
status: "completed"
tags: ["nginx", "cheatsheet", "location", "matching", "priority", "regex", "decision-table"]
prerequisites: ["01-fundamentals/03-server-and-location.md", "08-interview/02-location-matching-and-rewrites-qa.md"]
estimated_read_time: "20 min"
---

# Cheatsheet: NGINX Location Matching Priority & Decision Tables

```
+-------------------------------------------------------------------------------+
| Location Matching Priority Matrix                                             |
|                                                                               |
| Priority | Modifier | Match Type               | Termination Behavior         |
| :------- | :------- | :----------------------- | :--------------------------- |
| **1**    | `=`      | Exact String Match       | **Instant Halt** on match    |
| **2**    | `^~`     | Non-Regex Longest Prefix | **Instant Halt** on longest  |
| **3**    | `~`      | Case-Sensitive Regex     | **Halt on First** file match |
| **3**    | `~*`     | Case-Insensitive Regex   | **Halt on First** file match |
| **4**    | (None)   | Standard Longest Prefix  | Fallback if NO regex matches  |
| **5**    | `@name`  | Named Internal Location  | Internal redirect target only |
+-------------------------------------------------------------------------------+
```

---

## 1. Modifier Quick Reference Table

| Modifier | Syntax Example | Search Mechanism | Regular Expressions Evaluated? |
| :--- | :--- | :--- | :--- |
| `=` | `location = /login` | Checks exact literal string identity. | ❌ No (Search terminates instantly) |
| `^~` | `location ^~ /static/` | Longest prefix match; skips regexes. | ❌ No (Search terminates instantly) |
| `~` | `location ~ \.(php\|py)$` | Evaluates regex with case sensitivity. |  Yes (Evaluated in file order) |
| `~*` | `location ~* \.(jpg\|png)$`| Evaluates regex ignoring case. |  Yes (Evaluated in file order) |
| *(None)* | `location /docs/` | Longest prefix match; permits regexes.|  Yes (Regexes checked after prefix) |
| `@` | `location @fallback` | Explicit named internal location. | ❌ N/A (Cannot match client URIs) |

---

## 2. Comprehensive Test Scenario Matrix

Given the following 7 configured location blocks:
```nginx
location = / {                          # Block 1: Exact root
    return 200 "Block 1 (Exact Root)\n";
}

location = /images/logo.png {            # Block 2: Exact logo
    return 200 "Block 2 (Exact Logo)\n";
}

location ^~ /images/ {                   # Block 3: Non-regex prefix
    return 200 "Block 3 (Non-Regex Images Prefix)\n";
}

location ~* \.(png|jpg|gif)$ {           # Block 4: Case-insensitive image regex
    return 200 "Block 4 (Image Regex)\n";
}

location ~* ^/docs/.*\.pdf$ {            # Block 5: Docs PDF regex
    return 200 "Block 5 (Docs PDF Regex)\n";
}

location /docs/ {                        # Block 6: Standard prefix docs
    return 200 "Block 6 (Standard Docs Prefix)\n";
}

location / {                             # Block 7: Catch-all prefix
    return 200 "Block 7 (Catch-All Prefix)\n";
}
```

### Evaluation Outcomes Across Input URIs:

| Input URI | Matched Block | Winning Reason |
| :--- | :--- | :--- |
| `GET /` | **Block 1** | Exact match (`= /`) takes absolute precedence. |
| `GET /images/logo.png` | **Block 2** | Exact match (`= /images/logo.png`) takes absolute precedence. |
| `GET /images/header.PNG` | **Block 3** | Longest prefix is `^~ /images/`; skips regexes entirely (Block 4 skipped). |
| `GET /photos/vacation.jpg` | **Block 4** | Longest prefix is `/` (Block 7); regex search runs and Block 4 matches. |
| `GET /docs/guide.pdf` | **Block 5** | Longest prefix is `/docs/` (Block 6, no `^~`); regex search runs and Block 5 matches. |
| `GET /docs/readme.txt` | **Block 6** | Longest prefix is `/docs/`; no regex matches; falls back to Block 6. |
| `GET /api/v1/users` | **Block 7** | No other prefix or regex matches; falls back to catch-all Block 7. |

---

## 3. The URI Normalization Invariant

Before NGINX compares a request URI against location blocks, it normalizes the URI:
1. Decodes percent-encoded hex sequences (e.g. `%20` becomes space, `%2F` becomes `/`).
2. Resolves relative path references (`.` and `..`), collapsing `/static/../secret` to `/secret`.
3. Merges duplicate consecutive slashes (`//` becomes `/`) unless `merge_slashes off;` is set.
4. Strips query parameters (`?key=val`) completely! Location blocks match **strictly against the URI path, never query arguments**.

---

## 4. Common Location Traps & Gotchas

### Trap 1: Query String Matching in Location
```nginx
# BROKEN: Will NEVER match! Location ignores query strings.
location /search?query=true {
    return 200 "matched";
}

# CORRECT: Match path in location; check $args or $arg_query inside map
map $arg_query $is_search {
    "true"  1;
    default 0;
}
```

### Trap 2: File Order Precedence in Regular Expressions
```nginx
# VULNERABLE: Specific regex placed AFTER general regex
location ~* \.php$ {
    # Handles all PHP files
}

location ~* ^/admin/.*\.php$ {
    # INTENDED: Special auth checks for admin PHP scripts
    # REALITY: NEVER REACHED! First regex matches /admin/login.php first!
}
```
*Rule*: In NGINX, regular expressions evaluate in **file order** (top to bottom). Always place more specific regex rules *above* general regex rules!

---

[← Previous Cheatsheet: 01-directives-quick-reference.md](01-directives-quick-reference.md) | [Back to Module Index](README.md) | [Next Cheatsheet: 03-proxy-and-headers-snippets.md →](03-proxy-and-headers-snippets.md)
