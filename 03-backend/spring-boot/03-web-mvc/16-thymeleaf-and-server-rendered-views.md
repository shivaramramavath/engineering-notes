# Thymeleaf & Server-Rendered Views in Spring Boot

| Metadata | Value |
| :--- | :--- |
| **Level** | Intermediate to Advanced |
| **Prerequisites** | Spring MVC Controllers, HTML5/CSS, View Resolvers |
| **Estimated Reading Time** | 22 minutes |
| **Status** | Production-Ready |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Baseline** | OpenJDK 21 LTS / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | Thymeleaf 3.1, OpenJDK 21.0.2 |

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
9. [Comparison: Thymeleaf vs JSP vs Client-Side SPA](#9-comparison-thymeleaf-vs-jsp-vs-client-side-spa)
10. [Interview Questions & Deep Dives](#10-interview-questions--deep-dives)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference](#12-summary--quick-reference)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

While single-page applications (SPAs) dominate consumer web apps, server-side rendered (SSR) web applications remain the gold standard for internal enterprise admin dashboards, transactional email generation, customer billing portals, and SEO-critical content.

### The Problem
Legacy server-rendering technologies like JavaServer Pages (JSP):
1. Break natural HTML rendering—a `.jsp` file cannot be opened directly in a browser without a running application server.
2. Intermingle raw Java code inside presentation markup (`<% ... %>`), creating unmaintainable templates.
3. Have poor support in modern embedded containers (Tomcat executable JARs struggle with JSP compilation).

### The Solution: Thymeleaf "Natural Templates"
Thymeleaf is modern XML/XHTML/HTML5 template engine designed for Spring Boot. Its killer feature is **Natural Templating**: templates are completely valid HTML5 files. A static `.html` template can be previewed directly in a browser by designers, and then dynamically populated with data by the Spring server at runtime using custom HTML attributes (`th:text`, `th:each`, `th:if`).

---

## 2. Core Concepts

### Mental Model & View Resolution Workflow
```
             Client HTTP Request: GET /admin/users
                              │
                              ▼
           AdminUserController.listUsers(Model model)
           • Injects domain records into Model
           • Returns String view name: "admin/user-list"
                              │
                              ▼
                 ThymeleafViewResolver
           • Prepends prefix: classpath:/templates/
           • Appends suffix: .html
           • Target template: /templates/admin/user-list.html
                              │
                              ▼
                 Thymeleaf Template Engine
           • Parses HTML AST
           • Evaluates SpEL expressions (${users}, #{msg.welcome})
           • Strips prototype values; replaces with model data
           • Escapes HTML output automatically (XSS defense)
                              │
                              ▼
           Outputs Clean Valid HTML5 Response Stream
```

### Invariants
1. **Natural HTML Invariant**: Any Thymeleaf template must remain valid HTML syntax that can be opened and displayed directly in a web browser without throwing syntax errors.
2. **Context-Aware Automatic Escaping**: Variables evaluated via `th:text="${var}"` are automatically HTML-escaped. Unescaped injection requires `th:utext` and should be strictly audited.
3. **Template Resolution Boundary**: Templates reside in `src/main/resources/templates/` and cannot be accessed directly as static files from the web root.

---

## 3. How It Works Internally

### The Standard Expression Syntax
Thymeleaf organizes expressions into specialized categories:
- **`${...}` (Variable Expressions)**: Spring Expression Language (SpEL) executed against the model context (e.g. `${user.name}`).
- **`*{...}` (Selection Variable Expressions)**: Evaluated against a previously selected object (`th:object="${user}"`).
- **`#{...}` (Message Expressions)**: Resolves localized strings from `messages.properties` for internationalization.
- **`@{...}` (Link URL Expressions)**: Context-aware URL generation (e.g. `@{/users/{id}(id=${u.id})}` handles context paths and URL encoding).
- **`~{...}` (Fragment Expressions)**: Imports reusable page fragments (headers, footers, navigation bars).

---

## 4. Syntax & Configuration

### Dependency Setup (`pom.xml`)
```xml
<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-starter-thymeleaf</artifactId>
</dependency>
```

### Standard `application.yaml` Configuration
```yaml
spring:
  thymeleaf:
    prefix: classpath:/templates/
    suffix: .html
    mode: HTML
    encoding: UTF-8
    cache: true # Set to false in local development (DevTools auto-disables cache)
    servlet:
      content-type: text/html
```

---

## 5. Practical Examples

### Example 1: Basic Controller and Natural HTML Template

```java
package com.example.web.thymeleaf;

import org.springframework.stereotype.Controller;
import org.springframework.ui.Model;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RequestMapping;

import java.util.List;

public record MemberDto(Long id, String name, String role, boolean active) {}

@Controller
@RequestMapping("/portal/members")
public class MemberPortalController {

    @GetMapping
    public String listMembers(Model model) {
        List<MemberDto> members = List.of(
                new MemberDto(1L, "Alice Johnson", "ADMIN", true),
                new MemberDto(2L, "Bob Smith", "MEMBER", false),
                new MemberDto(3L, "Carol Williams", "EDITOR", true)
        );
        model.addAttribute("members", members);
        model.addAttribute("pageTitle", "Member Directory");
        return "members/directory"; // Resolves to templates/members/directory.html
    }
}
```

```html
<!-- File: src/main/resources/templates/members/directory.html -->
<!DOCTYPE html>
<html xmlns:th="http://www.thymeleaf.org" lang="en">
<head>
    <meta charset="UTF-8">
    <title th:text="${pageTitle}">Default Directory Title</title>
    <link rel="stylesheet" th:href="@{/css/bootstrap.min.css}" href="../../static/css/bootstrap.min.css">
</head>
<body class="container py-4">
    <h1 th:text="${pageTitle}">Member Directory</h1>
    
    <table class="table table-striped table-hover">
        <thead>
            <tr>
                <th>ID</th>
                <th>Name</th>
                <th>Role</th>
                <th>Status</th>
            </tr>
        </thead>
        <tbody>
            <!-- Natural Loop: Prototyped rows are replaced by server iterations -->
            <tr th:each="member : ${members}">
                <td th:text="${member.id}">101</td>
                <td th:text="${member.name}">Sample Name</td>
                <td th:text="${member.role}">USER</td>
                <td>
                    <span th:if="${member.active}" class="badge bg-success">Active</span>
                    <span th:unless="${member.active}" class="badge bg-secondary">Inactive</span>
                </td>
            </tr>
        </tbody>
    </table>
</body>
</html>
```

### Example 2: Enterprise Layout Dialect with Modular Fragments

```html
<!-- File: src/main/resources/templates/fragments/layout.html -->
<!DOCTYPE html>
<html xmlns:th="http://www.thymeleaf.org" lang="en">
<head th:fragment="header-assets(title)">
    <meta charset="UTF-8">
    <title th:text="${title}">Enterprise System</title>
    <link rel="stylesheet" th:href="@{/css/app.css}">
</head>
<body>
    <nav th:fragment="navbar" class="navbar navbar-dark bg-dark">
        <a class="navbar-brand" th:href="@{/}">Corporate Portal</a>
    </nav>

    <footer th:fragment="footer" class="text-center py-3">
        <p>&copy; 2026 Enterprise Corp. All rights reserved.</p>
    </footer>
</body>
</html>
```

```html
<!-- File: src/main/resources/templates/dashboard.html -->
<!DOCTYPE html>
<html xmlns:th="http://www.thymeleaf.org" lang="en">
<head th:replace="~{fragments/layout :: header-assets('Dashboard')}"></head>
<body>
    <div th:replace="~{fragments/layout :: navbar}"></div>

    <main class="container my-5">
        <h2>Executive Dashboard</h2>
        <p>Welcome back, <span th:text="${currentUser}">Admin</span>!</p>
    </main>

    <div th:replace="~{fragments/layout :: footer}"></div>
</body>
</html>
```

### Example 3: Edge Case — Secure HTML Email Templating with `SpringTemplateEngine`

Thymeleaf is widely used outside web browsers to render formatted, dynamic HTML transactional emails.

```java
package com.example.web.email;

import org.springframework.stereotype.Service;
import org.thymeleaf.context.Context;
import org.thymeleaf.spring6.SpringTemplateEngine;

import java.util.Locale;
import java.util.Map;

@Service
public class EmailTemplateRenderingService {

    private final SpringTemplateEngine templateEngine;

    public EmailTemplateRenderingService(SpringTemplateEngine templateEngine) {
        this.templateEngine = templateEngine;
    }

    public String generateWelcomeEmailHtml(String userName, String activationUrl, Locale locale) {
        Context context = new Context(locale);
        context.setVariables(Map.of(
                "name", userName,
                "activationUrl", activationUrl
        ));
        // Renders templates/emails/welcome.html into raw HTML string
        return templateEngine.process("emails/welcome", context);
    }
}
```

### Example 4: Anti-Pattern vs Production Fix

#### Anti-Pattern: Using `th:utext` for Unsanitized User Inputs
```html
<!-- ANTI-PATTERN: Unescaped HTML rendering introduces Critical Stored XSS! -->
<!-- If blog.comment contains <script>stealCookies()</script>, it executes in victim's browser! -->
<p th:utext="${userComment}">Sample Comment</p>
```

#### Production Fix: Use `th:text` for Automatic Output Escaping
```html
<!-- PRODUCTION FIX: th:text automatically escapes HTML tags to &lt;script&gt; -->
<p th:text="${userComment}">Sample Comment</p>
```

### Example 5: High-Performance Form Handling with Validation and CSRF

```java
package com.example.web.controller;

import jakarta.validation.Valid;
import jakarta.validation.constraints.Email;
import jakarta.validation.constraints.NotBlank;
import org.springframework.stereotype.Controller;
import org.springframework.validation.BindingResult;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.ModelAttribute;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestMapping;

public record RegistrationForm(
        @NotBlank String username,
        @NotBlank @Email String email
) {}

@Controller
@RequestMapping("/register")
public class RegistrationViewController {

    @GetMapping
    public String showForm(@ModelAttribute("form") RegistrationForm form) {
        return "auth/register";
    }

    @PostMapping
    public String submitForm(@Valid @ModelAttribute("form") RegistrationForm form,
                             BindingResult bindingResult) {
        if (bindingResult.hasErrors()) {
            return "auth/register"; // Return same template with validation error indicators
        }
        // Save user...
        return "redirect:/register/success";
    }
}
```

```html
<!-- File: src/main/resources/templates/auth/register.html -->
<!DOCTYPE html>
<html xmlns:th="http://www.thymeleaf.org">
<body>
    <form th:action="@{/register}" th:object="${form}" method="post">
        <!-- Spring Security automatically appends hidden CSRF token! -->
        <div>
            <label>Username:</label>
            <input type="text" th:field="*{username}" />
            <span th:if="${#fields.hasErrors('username')}" th:errors="*{username}" class="error"></span>
        </div>
        <div>
            <label>Email:</label>
            <input type="email" th:field="*{email}" />
            <span th:if="${#fields.hasErrors('email')}" th:errors="*{email}" class="error"></span>
        </div>
        <button type="submit">Submit</button>
    </form>
</body>
</html>
```

---

## 6. Best Practices

1. **Always Use `th:text` Instead of `th:utext`**: `th:text` automatically escapes special HTML characters, providing robust protection against Cross-Site Scripting (XSS).
2. **Modularize via Fragments**: Break pages into reusable components using `th:fragment` and `th:replace` / `th:insert` to prevent duplicating navigation bars, scripts, and footers.
3. **Use Link Expressions (`@{...}`)**: Always wrap URLs in `@{...}` so Thymeleaf automatically prepends application context paths and handles URL parameter encoding.
4. **Enable Template Cache in Production**: Ensure `spring.thymeleaf.cache: true` is enabled in production environments to avoid re-parsing HTML files on every HTTP request.
5. **Use `#fields.hasErrors()` for Form Validation**: Pair form binding (`th:object`) with `#fields.hasErrors()` to render localized validation feedback inline with form fields.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Using `@RestController` Instead of `@Controller`** | `@RestController` serializes the returned string directly to JSON rather than resolving the view template. | Use standard `@Controller` for view-returning methods. |
| **Missing Model Attribute for Form Binding** | Thymeleaf throws `IllegalStateException: Neither BindingResult nor plain target object available` if `th:object` is null. | Ensure controller populates an empty DTO in the model via `@ModelAttribute`. |
| **Using `th:utext` on User Content** | Allows attackers to inject arbitrary JavaScript (Stored XSS). | Never use `th:utext` on untrusted input; use `th:text`. |
| **Leaving `spring.thymeleaf.cache: false` in Production** | Recompiling templates from disk on every request degrades server throughput by 80%. | Ensure template caching is enabled in production configuration profiles. |
| **Hardcoding Relative URLs (`href="/users"`)** | Breaks if the application is deployed behind a servlet context path (e.g. `/my-app/`). | Always use Thymeleaf URL syntax: `th:href="@{/users}"`. |
| **Typo in Fragment Path** | Throws `TemplateInputException: Error resolving fragment` at runtime. | Verify exact template file paths and fragment names. |

---

## 8. Performance & Security Considerations

### Cross-Site Scripting (XSS) Prevention
Thymeleaf provides built-in context-aware escaping. When evaluated inside a `<script>` tag (`th:inline="javascript"`), it serializes variables into safe JSON literals rather than raw strings, preventing script injection.

### Template Cache Sizing
Thymeleaf caches parsed template ASTs in memory. For large applications with hundreds of views, monitor memory utilization and configure template cache sizes via `ThymeleafViewResolver` if necessary.

---

## 9. Comparison: Thymeleaf vs JSP vs Client-Side SPA

| Feature | Thymeleaf | JavaServer Pages (JSP) | Client-Side SPA (React/Vue) |
| :--- | :--- | :--- | :--- |
| **Natural Templating** | Yes (Valid HTML in browser) | No (Requires running server) | No (JSX / Vue templates) |
| **Spring Boot Executable JAR** | Native First-Class Support | Poor (WAR packaging needed) | Separately built / bundled |
| **Security (XSS Defenses)** | Context-aware auto-escaping | Manual escaping required | Framework-level escaping |
| **SEO Optimization** | Immediate (Server-rendered) | Immediate (Server-rendered) | Requires SSR (Next.js / Nuxt) |
| **Client CPU Overhead** | Zero (Pure HTML) | Zero (Pure HTML) | High (Heavy JS execution) |

---

## 10. Interview Questions & Deep Dives

<details>
<summary><strong>Q1: What does "Natural Templating" mean in Thymeleaf?</strong></summary>

**Answer:**
Natural Templating means that Thymeleaf templates are fully valid, standard HTML files. They use standard HTML attributes prefixed with the `th:` namespace (e.g. `th:text="${name}"`). A designer can open the `.html` file directly in a web browser using the `file://` protocol, and it will render the prototype mock values. When executed inside Spring Boot, Thymeleaf intercepts the `th:*` attributes and replaces the mock values with dynamic server data.
</details>

<details>
<summary><strong>Q2: What is the difference between `th:replace` and `th:insert`?</strong></summary>

**Answer:**
- `th:insert`: Inserts the specified fragment content *inside* the host tag as its child.
- `th:replace`: Completely replaces the host tag with the fragment tag.
</details>

<details>
<summary><strong>Q3: How does Thymeleaf prevent Cross-Site Scripting (XSS)?</strong></summary>

**Answer:**
Thymeleaf evaluates variables via `th:text` using context-aware escaping. Special characters such as `<`, `>`, `&`, `"`, and `'` are automatically converted to their corresponding HTML entities (`&lt;`, `&gt;`, `&amp;`, `&quot;`, `&#39;`). If evaluated within JavaScript blocks (`th:inline="javascript"`), values are serialized as escaped JSON literals.
</details>

<details>
<summary><strong>Q4: Why does a controller returning `"users/list"` fail if annotated with `@RestController`?</strong></summary>

**Answer:**
`@RestController` is a meta-annotation composed of `@Controller` and `@ResponseBody`. When `@ResponseBody` is present, Spring skips view resolution and writes the return value directly to the HTTP response body as raw text (`"users/list"`). To trigger Thymeleaf's `ViewResolver`, the class must be annotated with `@Controller` without `@ResponseBody`.
</details>

<details>
<summary><strong>Q5: What are the primary Thymeleaf expression types?</strong></summary>

**Answer:**
1. `${...}`: Variable expressions (SpEL).
2. `*{...}`: Selection variable expressions (relative to `th:object`).
3. `#{...}`: Message expressions (i18n localization).
4. `@{...}`: Link URL expressions (context-aware URLs).
5. `~{...}`: Fragment expressions (component composition).
</details>

<details>
<summary><strong>Q6: How does Spring Security CSRF protection integrate with Thymeleaf forms?</strong></summary>

**Answer:**
When using `th:action="@{/url}"` on a `<form method="post">`, Thymeleaf automatically inspects the request for an active Spring Security `CsrfToken` and injects a hidden input field (`<input type="hidden" name="_csrf" value="..." />`) into the form automatically.
</details>

<details>
<summary><strong>Q7: How do you perform internationalization (i18n) in Thymeleaf?</strong></summary>

**Answer:**
Create property files like `messages.properties` and `messages_es.properties`. In templates, reference keys using `#{welcome.message}`. Spring's `LocaleResolver` determines the client's locale from the `Accept-Language` header or a session cookie, and Thymeleaf fetches the corresponding localized text.
</details>

<details>
<summary><strong>Q8: Can Thymeleaf be used without Spring MVC?</strong></summary>

**Answer:**
Yes. You can instantiate `TemplateEngine` directly, configure a `ClassLoaderTemplateResolver`, and call `templateEngine.process("templateName", context)` to generate HTML strings for offline document generation or transactional email bodies.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Basic Controller View Resolution</strong></summary>

**Task**: Write a `@Controller` method for `/welcome` that places attribute `"user" = "Developer"` into the model and returns template `"welcome"`.

```java
// Solution:
@Controller
public class WelcomeController {
    @GetMapping("/welcome")
    public String welcome(Model model) {
        model.addAttribute("user", "Developer");
        return "welcome";
    }
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Conditional Rendering in HTML</strong></summary>

**Task**: Write a Thymeleaf snippet rendering an alert box only if the model attribute `errorMessage` is present and not empty.

```html
<!-- Solution: -->
<div th:if="${errorMessage != null and !errorMessage.isBlank()}" class="alert alert-danger">
    <p th:text="${errorMessage}">Error placeholder</p>
</div>
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Table Iteration with Status Tracking</strong></summary>

**Task**: Iterate over a list `products` rendering an alternate background CSS class `table-dark` on odd rows using Thymeleaf's iteration status variable (`stat.odd`).

```html
<!-- Solution: -->
<table>
    <tr th:each="p, stat : ${products}" th:class="${stat.odd} ? 'table-dark' : 'table-light'">
        <td th:text="${stat.count}">1</td>
        <td th:text="${p.name}">Product Name</td>
    </tr>
</table>
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Form Binding with Validation Errors</strong></summary>

**Task**: Create an HTML form bound to `th:object="${feedbackForm}"` that displays field errors for the `comments` textarea.

```html
<!-- Solution: -->
<form th:action="@{/feedback}" th:object="${feedbackForm}" method="post">
    <textarea th:field="*{comments}"></textarea>
    <div th:if="${#fields.hasErrors('comments')}" th:errors="*{comments}" class="text-danger"></div>
    <button type="submit">Submit</button>
</form>
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Reusable Pagination Fragment Component</strong></summary>

**Task**: Build a reusable Thymeleaf fragment `pagination.html` that accepts a Spring Data `Page<?>` object and renders dynamic "Previous", page number buttons, and "Next" links preserving existing query string parameters.
</details>

<details>
<summary><strong>Exercise 6 (Hard): Multi-Tenant Themed Template Resolver</strong></summary>

**Task**: Implement a custom Thymeleaf `ITemplateResolver` that dynamically selects template files from tenant-specific subfolders (`/templates/tenants/{tenantId}/...`) based on the active thread-local tenant context.
</details>

---

## 12. Summary & Quick Reference

- **Natural Templates**: Valid HTML5 files executable in browser prototypes.
- **Controller Annotation**: Must use `@Controller` (never `@RestController`).
- **Core Expressions**: `${...}` (Variables), `*{...}` (Selections), `#{...}` (i18n), `@{...}` (URLs), `~{...}` (Fragments).
- **Security**: Always prefer `th:text` (auto-escaped) over `th:utext`.
- **Production Setting**: `spring.thymeleaf.cache: true`.

---

## 13. Further Reading

- [Thymeleaf Official Tutorial: Using Thymeleaf](https://www.thymeleaf.org/doc/tutorials/3.1/usingthymeleaf.html)
- [Spring Boot Reference: Thymeleaf Template Engine](https://docs.spring.io/spring-boot/reference/web/servlet.html#web.servlet.spring-mvc.template-engines)
- [OWASP Cross-Site Scripting (XSS) Prevention Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Cross_Site_Scripting_Prevention_Cheat_Sheet.html)
- [Tutorial: Thymeleaf + Spring Security Integration](https://www.thymeleaf.org/doc/articles/springsecurity.html)
