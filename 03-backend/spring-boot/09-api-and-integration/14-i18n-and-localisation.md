# Internationalization (i18n) and Localization (l10n) in Spring Boot

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x |
| **Java Version** | 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | `MessageSource`, `LocaleResolver`, `AcceptHeaderLocaleResolver` |

---

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Usage](#4-syntax--usage)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices](#6-best-practices)
- [7. Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
- [8. Performance & Security Notes](#8-performance--security-notes)
- [9. Comparison / When to Use What](#9-comparison--when-to-use-what)
- [10. Interview Questions](#10-interview-questions)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading](#13-further-reading)

---

## 1. What & Why

### The Problem
Global software applications serve users across diverse geographic regions, cultures, and languages. Hardcoding user-facing strings (error messages, validation feedback, email bodies, UI labels) directly into Java source code forces developers to rebuild and redeploy services just to alter text copy or support a new language. Furthermore, formatting dates, currencies, and numbers according to regional conventions requires structured localization infrastructure.

### The Evolution
Spring Framework provides enterprise-grade Internationalization (i18n) and Localization (l10n) abstractions built on top of standard Java resource bundles. In Spring Boot 3, internationalization is automated through `ResourceBundleMessageSource` (or reloadable `ReloadableResourceBundleMessageSource`), configurable `LocaleResolver` implementations (detecting locales via `Accept-Language` headers, cookies, or query parameters), and native integration with Jakarta Bean Validation error interpolation.

### Relevance in Modern Engineering
In multi-tenant SaaS platforms, modern APIs must dynamically adapt error responses (RFC 7807 `ProblemDetail`), transactional notifications, and currency formats to each tenant's user profile or browser preferences without incurring performance overhead.

---

## 2. Core Concepts

- **Internationalization (i18n)**: Designing software architecture so it can be adapted to various languages and regions without engineering changes.
- **Localization (l10n)**: Adapting internationalized software for a specific locale by populating language-specific text, date formats, and numeric conventions.
- **`MessageSource`**: Spring interface for resolving messages with parameterized arguments and regional fallback support.
- **Resource Bundles (`messages*.properties`)**: Property files located under `src/main/resources/` (e.g., `messages.properties` for default, `messages_es.properties` for Spanish, `messages_de.properties` for German).
- **`LocaleResolver`**: Strategy interface that inspects incoming HTTP requests to determine the current `java.util.Locale`.
  - `AcceptHeaderLocaleResolver`: Inspects HTTP `Accept-Language` header (default in Spring MVC).
  - `CookieLocaleResolver`: Persists user language choice across browser sessions via HTTP cookies.
  - `SessionLocaleResolver`: Stores user locale in the HTTP session.
- **`LocaleChangeInterceptor`**: Intercepts requests with a designated query parameter (e.g., `?lang=es`) to dynamically switch locales.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------+
|                        Spring MVC i18n Resolution Pipeline                        |
|                                                                                   |
| 1. Client HTTP Request                                                            |
|    GET /api/v1/orders/123 (Headers: Accept-Language: es-ES,es;q=0.9,en;q=0.8)    |
|                   |                                                               |
|                   v                                                               |
| 2. DispatcherServlet Interception                                                 |
|    +-----------------------------+                                                |
|    | LocaleResolver Resolves     | ---> Resolves Locale: es_ES (Spanish)          |
|    +-----------------------------+                                                |
|                   | Binds LocaleContext to ThreadLocal (LocaleContextHolder)      |
|                   v                                                               |
| 3. Controller / Service Execution                                                 |
|    Locale currentLocale = LocaleContextHolder.getLocale();                        |
|    String msg = messageSource.getMessage("order.notFound", args, currentLocale);  |
|                   |                                                               |
|                   v                                                               |
| 4. MessageSource Hierarchy Lookup                                                 |
|    messages_es_ES.properties ---> messages_es.properties ---> messages.properties |
|                   |                                                               |
|                   v (Interpolates arguments: {0} -> "123")                        |
| 5. Resolved String: "El pedido 123 no fue encontrado."                            |
+-----------------------------------------------------------------------------------+
```

When an HTTP request enters `DispatcherServlet`, the registered `LocaleResolver` determines the caller's `Locale`. The locale is bound to the current thread via `LocaleContextHolder`. When the application queries `MessageSource`, it searches property files from most specific (`messages_es_ES.properties`) to language general (`messages_es.properties`) to the default fallback (`messages.properties`). Dynamic parameters (`{0}`, `{1}`) are interpolated using Java's `MessageFormat`.

---

## 4. Syntax & Usage

### Application Properties
```properties
# Base names for message bundle files (comma-delimited)
spring.messages.basename=messages
spring.messages.encoding=UTF-8
spring.messages.cache-duration=3600s
spring.messages.fallback-to-system-locale=false
```

### Property Files Layout
- `src/main/resources/messages.properties` (Default English fallback)
- `src/main/resources/messages_es.properties` (Spanish)
- `src/main/resources/messages_fr.properties` (French)
- `src/main/resources/messages_de.properties` (German)

---

## 5. Practical Examples

### Example 1: Resource Bundle Definition (`messages.properties` & `messages_es.properties`)
```properties
# src/main/resources/messages.properties (Default)
account.created=Account successfully created for user {0}.
account.insufficient.funds=Account {0} has insufficient balance. Required: {1,number,currency}, Available: {2,number,currency}.
error.not_found=Resource with identifier {0} was not found.

# src/main/resources/messages_es.properties (Spanish)
account.created=Cuenta creada exitosamente para el usuario {0}.
account.insufficient.funds=La cuenta {0} no tiene saldo suficiente. Requerido: {1,number,currency}, Disponible: {2,number,currency}.
error.not_found=No se encontró el recurso con el identificador {0}.
```

### Example 2: Configuring LocaleResolver and ReloadableMessageSource
```java
package com.example.platform.i18n.config;

import org.springframework.context.MessageSource;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.context.support.ReloadableResourceBundleMessageSource;
import org.springframework.web.servlet.LocaleResolver;
import org.springframework.web.servlet.config.annotation.InterceptorRegistry;
import org.springframework.web.servlet.config.annotation.WebMvcConfigurer;
import org.springframework.web.servlet.i18n.AcceptHeaderLocaleResolver;
import org.springframework.web.servlet.i18n.LocaleChangeInterceptor;

import java.nio.charset.StandardCharsets;
import java.util.List;
import java.util.Locale;

@Configuration
public class LocalizationConfig implements WebMvcConfigurer {

    @Bean
    public LocaleResolver localeResolver() {
        AcceptHeaderLocaleResolver resolver = new AcceptHeaderLocaleResolver();
        resolver.setDefaultLocale(Locale.ENGLISH);
        // Supported languages
        resolver.setSupportedLocales(List.of(
                Locale.ENGLISH,
                Locale.forLanguageTag("es"),
                Locale.FRENCH,
                Locale.GERMAN
        ));
        return resolver;
    }

    @Bean
    public MessageSource messageSource() {
        ReloadableResourceBundleMessageSource source = new ReloadableResourceBundleMessageSource();
        source.setBasename("classpath:messages");
        source.setDefaultEncoding(StandardCharsets.UTF_8.name());
        source.setFallbackToSystemLocale(false); // Crucial: avoid server OS language leak
        source.setCacheSeconds(3600); // Cache properties for 1 hour
        return source;
    }

    @Bean
    public LocaleChangeInterceptor localeChangeInterceptor() {
        LocaleChangeInterceptor interceptor = new LocaleChangeInterceptor();
        interceptor.setParamName("lang"); // e.g. /api/v1/items?lang=es
        return interceptor;
    }

    @Override
    public void addInterceptors(InterceptorRegistry registry) {
        registry.addInterceptor(localeChangeInterceptor());
    }
}
```

### Example 3: Localized Exception Handling in `@RestControllerAdvice`
```java
package com.example.platform.i18n.controller;

import org.springframework.context.MessageSource;
import org.springframework.context.i18n.LocaleContextHolder;
import org.springframework.http.HttpStatus;
import org.springframework.http.ProblemDetail;
import org.springframework.web.bind.annotation.ExceptionHandler;
import org.springframework.web.bind.annotation.RestControllerAdvice;

import java.net.URI;
import java.util.Locale;

public class ResourceNotFoundException extends RuntimeException {
    private final String resourceId;
    public ResourceNotFoundException(String resourceId) {
        super("Resource not found: " + resourceId);
        this.resourceId = resourceId;
    }
    public String getResourceId() { return resourceId; }
}

@RestControllerAdvice
public class LocalizedExceptionAdvice {

    private final MessageSource messageSource;

    public LocalizedExceptionAdvice(MessageSource messageSource) {
        this.messageSource = messageSource;
    }

    @ExceptionHandler(ResourceNotFoundException.class)
    public ProblemDetail handleNotFound(ResourceNotFoundException ex) {
        // Automatically extracts Locale resolved from Accept-Language header
        Locale locale = LocaleContextHolder.getLocale();

        String localizedDetail = messageSource.getMessage(
                "error.not_found",
                new Object[]{ex.getResourceId()},
                "Resource not found",
                locale);

        ProblemDetail problem = ProblemDetail.forStatusAndDetail(HttpStatus.NOT_FOUND, localizedDetail);
        problem.setType(URI.create("https://api.example.com/errors/not-found"));
        problem.setTitle("Resource Not Found");
        return problem;
    }
}
```

### Example 4: Leaking Server OS Locale (Anti-Pattern Fix)
```java
// ANTI-PATTERN: fallbackToSystemLocale = true.
// If an unsupported language (e.g., Japanese) is requested, and the server runs in Frankfurt (German OS),
// the user receives German instead of the application's default English fallback!

// FIX: Explicitly disable system locale fallback
package com.example.platform.i18n.util;

import org.springframework.context.support.ReloadableResourceBundleMessageSource;

public class SafeMessageSourceFactory {
    public static ReloadableResourceBundleMessageSource create() {
        ReloadableResourceBundleMessageSource source = new ReloadableResourceBundleMessageSource();
        source.setBasename("classpath:messages");
        source.setFallbackToSystemLocale(false); // Ensures fallback always uses default messages.properties
        return source;
    }
}
```

### Example 5: High-Performance Regional Currency and Number Formatter
```java
package com.example.platform.i18n.service;

import org.springframework.context.i18n.LocaleContextHolder;
import org.springframework.stereotype.Service;

import java.math.BigDecimal;
import java.text.NumberFormat;
import java.time.LocalDate;
import java.time.format.DateTimeFormatter;
import java.time.format.FormatStyle;
import java.util.Locale;

@Service
public class RegionalFormattingService {

    public String formatCurrency(BigDecimal amount) {
        Locale currentLocale = LocaleContextHolder.getLocale();
        NumberFormat currencyFormatter = NumberFormat.getCurrencyInstance(currentLocale);
        return currencyFormatter.format(amount);
    }

    public String formatDate(LocalDate date) {
        Locale currentLocale = LocaleContextHolder.getLocale();
        DateTimeFormatter dateFormatter = DateTimeFormatter
                .ofLocalizedDate(FormatStyle.MEDIUM)
                .withLocale(currentLocale);
        return date.format(dateFormatter);
    }
}
```

---

## 6. Best Practices

1. **Always Set `fallbackToSystemLocale = false`**: Never fall back to the host operating system's locale; if an unsupported language is requested, always fall back to the base `messages.properties` (typically English).
2. **Standardize on UTF-8 Encoding**: Explicitly configure `spring.messages.encoding=UTF-8` or `source.setDefaultEncoding("UTF-8")` to support non-ASCII alphabets (CJK, Cyrillic, Arabic).
3. **Use `LocaleContextHolder.getLocale()`**: Retrieve the current request's locale from thread-local storage rather than passing `HttpServletRequest` throughout service layers.
4. **Use NumberFormat Currency Interpolation**: Leverage `{1,number,currency}` inside message bundles to let Java format currencies automatically according to regional standards.
5. **Set Supported Locales on `AcceptHeaderLocaleResolver`**: Define allowed locales (`resolver.setSupportedLocales(...)`) so malicious or malformed `Accept-Language` headers are sanitized to default locales.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Relying on host OS default locale (`fallbackToSystemLocale = true`) | Cloud instances in different AWS regions emit different languages to users | Configure `source.setFallbackToSystemLocale(false)` in message source. |
| Using ISO-8859-1 encoding for properties files | Non-English characters, accents, and emojis appear corrupted | Enforce UTF-8 encoding in `spring.messages.encoding=UTF-8`. |
| Passing unescaped single quotes in message patterns | `MessageFormat` treats single quotes as literal escape characters, swallowing parameters | Escape single quotes by doubling them (`''`) in `.properties` files. |
| Hardcoding error messages in controllers or services | Prevents multilingual translations without code modification | Always look up error messages via `MessageSource.getMessage()`. |
| Neglecting to configure default fallback in `getMessage()` | If a message key is missing, throws `NoSuchMessageException` | Provide a default message argument: `getMessage(code, args, defaultMsg, locale)`. |
| Storing user language choices only in local memory | Lost when user switches devices or when server restarts | Persist user language preferences in user profile database table or HTTP cookie. |

---

## 8. Performance & Security Notes

- **Message Caching**: `ReloadableResourceBundleMessageSource` can reload files without server restarts in development, but must be configured with a long cache duration (e.g., `setCacheSeconds(3600)`) in production to avoid repeated disk reads.
- **MessageFormat Injection Defense**: Never interpolate unsanitized user inputs directly into message format templates; pass user variables strictly as positional arguments (`{0}`, `{1}`) to prevent format string injection.
- **ThreadLocal Safety**: Spring MVC automatically purges `LocaleContextHolder` after request completion, preventing cross-request locale bleeding on pooled threads.

---

## 9. Comparison / When to Use What

| Strategy | AcceptHeaderLocaleResolver | CookieLocaleResolver | SessionLocaleResolver |
|---|---|---|---|
| **Mechanism** | `Accept-Language` HTTP Header | Browser HTTP Cookie | Server HTTP Session |
| **Statefulness** | Completely Stateless | Client-side Persistent | Server-side Stateful |
| **User Override** | Depends on browser settings | User can switch via UI button | Lost on session expiration |
| **API Architecture** | Ideal for REST APIs & Microservices | Traditional Server-Side Web | Legacy Stateful MVC Apps |

---

## 10. Interview Questions

<details>
<summary>1. How does Spring determine the Locale for an incoming HTTP request?</summary>
Spring uses the configured `LocaleResolver` bean (by default `AcceptHeaderLocaleResolver`). The resolver inspects the `Accept-Language` header, matches it against supported locales, and binds the resolved `Locale` to `LocaleContextHolder` (a `ThreadLocal` storage), making it accessible anywhere in the call stack.
</details>

<details>
<summary>2. Why is setting fallbackToSystemLocale(false) critical in cloud environments?</summary>
If `fallbackToSystemLocale` is true and a client requests an unsupported language, Spring falls back to the JVM's default locale derived from the host OS environment. In multi-region cloud deployments (e.g., AWS us-east-1 vs eu-central-1), identical requests could produce English on one node and German on another. Setting it to `false` guarantees deterministic fallback to base `messages.properties`.
</details>

<details>
<summary>3. What is the role of LocaleContextHolder in Spring MVC?</summary>
`LocaleContextHolder` provides static access to the current request's `Locale` and `TimeZone` via `ThreadLocal`. This eliminates the need to pass `HttpServletRequest` or `Locale` objects through controller, service, and repository method signatures.
</details>

<details>
<summary>4. How does MessageFormat handle argument interpolation and single quotes?</summary>
`MessageFormat` replaces numbered tokens (`{0}`, `{1}`) with provided arguments. In `MessageFormat` syntax, single quotes (`'`) are special characters used to quote literal text. An unescaped single quote disables parameter substitution. To output a literal single quote, it must be doubled: `Don''t`.
</details>

<details>
<summary>5. How does Spring integrate Bean Validation with MessageSource?</summary>
Spring Boot auto-configures `LocalValidatorFactoryBean`, which bridges Hibernate Validator with Spring's `MessageSource`. When a validation error occurs (e.g., `@NotNull(message = "{user.name.required}")`), Spring resolves `{user.name.required}` from `messages.properties` using the current locale.
</details>

<details>
<summary>6. What is the difference between ResourceBundleMessageSource and ReloadableResourceBundleMessageSource?</summary>
`ResourceBundleMessageSource` relies on standard `java.util.ResourceBundle` and classloader loading; property files cannot be reloaded without restarting the JVM. `ReloadableResourceBundleMessageSource` uses Spring's `ResourceLoader`, reads files from any location (classpath or file system), supports hot reloading via `cacheSeconds`, and handles UTF-8 natively.
</details>

<details>
<summary>7. How does AcceptHeaderLocaleResolver handle multiple quality-factor languages (e.g., es,en;q=0.8)?</summary>
It parses the header and sorts the language ranges by their quality factor (`q` value). It iterates through the client's preferences in descending priority order and selects the first language that matches the server's configured `supportedLocales` list.
</details>

<details>
<summary>8. How do you prevent NoSuchMessageException when querying MessageSource?</summary>
By using the overloaded `getMessage(code, args, defaultMessage, locale)` method that accepts a default fallback string, or by setting `messageSource.setUseCodeAsDefaultMessage(true)`, which returns the message code itself if no translation is found.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1 (Easy): Create a messages.properties file and read a value</summary>
Create `messages.properties` with key `welcome.title=Welcome to Store` and write a test retrieving it via `MessageSource`.
</details>

<details>
<summary>Exercise 2 (Easy): Configure AcceptHeaderLocaleResolver with default English</summary>
Write a `@Configuration` class declaring an `AcceptHeaderLocaleResolver` bean defaulting to `Locale.US`.
</details>

<details>
<summary>Exercise 3 (Medium): Format currency dynamically based on request locale</summary>
Write a service method that formats a `BigDecimal` balance into localized currency strings using `NumberFormat.getCurrencyInstance(LocaleContextHolder.getLocale())`.
</details>

<details>
<summary>Exercise 4 (Medium): Build a localized validation error handler</summary>
Implement a `@ControllerAdvice` method that catches `MethodArgumentNotValidException` and resolves error message codes from `messages.properties`.
</details>

<details>
<summary>Exercise 5 (Hard): Configure dynamic locale switching via query parameter and cookie persistence</summary>
Configure `CookieLocaleResolver` and `LocaleChangeInterceptor` listening on parameter `?lang=`, persisting the user's language choice in an HTTP cookie for 30 days.
</details>

<details>
<summary>Exercise 6 (Hard): Integration test asserting multi-language responses</summary>
Write a `@WebMvcTest` test executing GET requests with `Accept-Language: es` and `Accept-Language: de`, asserting that the returned JSON contains Spanish and German strings respectively.
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------+
|                       Spring i18n & l10n Cheat Sheet                            |
+---------------------------------------------------------------------------------+
| Resource Files:     src/main/resources/messages[_<lang>].properties             |
| File Encoding:      spring.messages.encoding=UTF-8                              |
| Fallback Rule:      source.setFallbackToSystemLocale(false) [Crucial]           |
| Core SPI:           MessageSource, LocaleResolver, LocaleContextHolder          |
| Web Resolvers:      AcceptHeaderLocaleResolver (REST), CookieLocaleResolver     |
| Locale Lookup:      Locale currentLocale = LocaleContextHolder.getLocale()      |
| Formatting:         NumberFormat.getCurrencyInstance(currentLocale)             |
| Quote Escape:       Use '' (double single quotes) to escape literal quotes      |
+---------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Official Spring Framework Internationalization Guide](https://docs.spring.io/spring-framework/reference/core/beans/context-introduction.html#context-functionality-messagesource)
- [Java Internationalization: Locale and ResourceBundle Guide](https://docs.oracle.com/en/java/javase/21/docs/api/java.base/java/util/Locale.html)
- [RFC 4647: Matching of Language Tags](https://datatracker.ietf.org/doc/html/rfc4647)
