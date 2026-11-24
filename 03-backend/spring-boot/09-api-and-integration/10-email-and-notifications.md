# Email and Notifications with JavaMailSender and Thymeleaf

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x |
| **Java Version** | 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | `spring-boot-starter-mail`, `org.thymeleaf:thymeleaf-spring6` |

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
Transactional notifications (user registration welcome emails, password reset links, invoice delivery, multi-factor authentication codes) are foundational to web applications. Dispatching emails synchronously inside HTTP request threads introduces crippling latency—SMTP handshakes, TLS negotiation, and MIME message transfers can take between 500ms and 5 seconds per email. If SMTP servers experience network slowdowns or downtime, user HTTP requests time out, database transactions are blocked, and user registrations fail.

### The Evolution
Spring Framework provides the `JavaMailSender` abstraction over the Jakarta Mail API (`jakarta.mail`). Combined with Thymeleaf template processing, applications decouple presentation logic (rich responsive HTML layouts) from application code. By pairing `JavaMailSender` with Spring's asynchronous task execution (`@Async`), Virtual Threads, or message queues, email delivery is offloaded to non-blocking background workers.

### Relevance in Modern Engineering
Modern email systems require HTML templating, multipart attachments, inline images (CID), multi-tenant branding, SPF/DKIM compliance, and resilient queue-backed retry mechanisms to withstand third-party SMTP provider (SendGrid, AWS SES, Postmark, Mailgun) outages.

---

## 2. Core Concepts

- **`JavaMailSender`**: The core Spring interface providing methods to send simple text emails (`SimpleMailMessage`) and complex MIME messages (`MimeMessage`).
- **`MimeMessageHelper`**: Utility class simplifying the construction of multipart MIME emails with HTML formatting, UTF-8 character encoding, attachments, and inline CID images.
- **Thymeleaf Template Engine**: A modern server-side template engine that merges Java model attributes into structured HTML email templates.
- **Asynchronous Task Execution (`@Async`)**: Running email transmission on background threads or Java 21 Virtual Threads to prevent blocking web request threads.
- **Jakarta Mail Properties**: Configuration settings governing SMTP authentication, STARTTLS encryption, socket timeouts, and debug logging.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------+
|                        Asynchronous Email Dispatch Engine                         |
|                                                                                   |
| 1. HTTP Request (User Registration)                                               |
|    POST /api/v1/users/register                                                    |
|                   |                                                               |
|                   v                                                               |
| 2. User Created & Event Published                                                 |
|    publishEvent(new UserRegisteredEvent(user))                                    |
|                   |                                                               |
|                   v                                                               |
| 3. Asynchronous Event Listener (@Async / Virtual Thread)                          |
|    +-----------------------------+                                                |
|    | EmailService executes async | ---> Releases HTTP thread immediately (201 OK) |
|    +-----------------------------+                                                |
|                   |                                                               |
|                   v                                                               |
| 4. Thymeleaf HTML Rendering                                                       |
|    TemplateEngine.process("welcome-email.html", Context)                          |
|                   | Produces rendered HTML string                                 |
|                   v                                                               |
| 5. MimeMessage Construction                                                       |
|    MimeMessageHelper sets to, from, subject, html body, CID assets               |
|                   |                                                               |
|                   v                                                               |
| 6. SMTP Dispatch over TLS                                                         |
|    JavaMailSender.send(mimeMessage) ---> SMTP Server (SES / SendGrid / Mailpit)   |
+-----------------------------------------------------------------------------------+
```

When an event triggers an email, the `@Async` execution context schedules the task on a dedicated executor thread pool. The service initializes a Thymeleaf `Context` with domain variables (e.g., user name, verification link). The template engine processes the template located at `src/main/resources/templates/email/`. Next, `JavaMailSender.createMimeMessage()` creates a Jakarta Mail message wrapped in `MimeMessageHelper`. Finally, the message is transmitted across an encrypted TLS connection to the SMTP server.

---

## 4. Syntax & Usage

### Maven Dependencies
```xml
<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-starter-mail</artifactId>
</dependency>
<dependency>
    <groupId>org.springframework.boot</groupId>
    <artifactId>spring-boot-starter-thymeleaf</artifactId>
</dependency>
```

### Application Properties
```properties
# SMTP Server Configuration (e.g., Mailpit for local dev, AWS SES for prod)
spring.mail.host=localhost
spring.mail.port=1025
spring.mail.username=
spring.mail.password=
spring.mail.properties.mail.smtp.auth=false
spring.mail.properties.mail.smtp.starttls.enable=false
spring.mail.properties.mail.smtp.connectiontimeout=5000
spring.mail.properties.mail.smtp.timeout=5000
spring.mail.properties.mail.smtp.writetimeout=5000
```

---

## 5. Practical Examples

### Example 1: Thymeleaf Email Template (`templates/email/welcome.html`)
```html
<!DOCTYPE html>
<html xmlns:th="http://www.thymeleaf.org">
<head>
    <meta http-equiv="Content-Type" content="text/html; charset=UTF-8" />
    <style>
        body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif; background-color: #f4f6f8; margin: 0; padding: 20px; }
        .card { max-width: 580px; margin: 0 auto; background: #ffffff; border-radius: 8px; padding: 32px; box-shadow: 0 4px 6px rgba(0,0,0,0.05); }
        .btn { display: inline-block; background-color: #0066cc; color: #ffffff; text-decoration: none; padding: 12px 24px; border-radius: 4px; font-weight: bold; margin-top: 16px; }
    </style>
</head>
<body>
    <div class="card">
        <h2>Welcome to Platform, <span th:text="${recipientName}">User</span>!</h2>
        <p>Thank you for creating an account. Please click the button below to verify your email address:</p>
        <p>
            <a th:href="${verificationUrl}" class="btn">Verify Email Address</a>
        </p>
        <p>This verification link will expire in <span th:text="${expirationHours}">24</span> hours.</p>
        <hr style="border: none; border-top: 1px solid #eeeeee; margin: 24px 0;" />
        <small style="color: #888888;">If you did not create this account, please disregard this email.</small>
    </div>
</body>
</html>
```

### Example 2: Enterprise Email Dispatch Service with HTML Templating
```java
package com.example.platform.notification.service;

import jakarta.mail.MessagingException;
import jakarta.mail.internet.MimeMessage;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.mail.javamail.JavaMailSender;
import org.springframework.mail.javamail.MimeMessageHelper;
import org.springframework.scheduling.annotation.Async;
import org.springframework.stereotype.Service;
import org.thymeleaf.TemplateEngine;
import org.thymeleaf.context.Context;

import java.nio.charset.StandardCharsets;
import java.util.Map;

@Service
public class EmailNotificationService {

    private static final Logger log = LoggerFactory.getLogger(EmailNotificationService.class);
    private final JavaMailSender mailSender;
    private final TemplateEngine templateEngine;
    private final String senderEmail;

    public EmailNotificationService(
            JavaMailSender mailSender,
            TemplateEngine templateEngine,
            @Value("${notification.mail.from:no-reply@example.com}") String senderEmail) {
        this.mailSender = mailSender;
        this.templateEngine = templateEngine;
        this.senderEmail = senderEmail;
    }

    @Async("emailTaskExecutor")
    public void sendTemplatedEmail(String to, String subject, String templateName, Map<String, Object> variables) {
        log.info("Preparing async email dispatch to: {}, subject: {}", to, subject);

        try {
            // 1. Render HTML via Thymeleaf Context
            Context context = new Context();
            context.setVariables(variables);
            String htmlContent = templateEngine.process("email/" + templateName, context);

            // 2. Build MIME message with UTF-8 encoding
            MimeMessage message = mailSender.createMimeMessage();
            MimeMessageHelper helper = new MimeMessageHelper(
                    message,
                    MimeMessageHelper.MULTIPART_MODE_MIXED_RELATED,
                    StandardCharsets.UTF_8.name());

            helper.setFrom(senderEmail);
            helper.setTo(to);
            helper.setSubject(subject);
            helper.setText(htmlContent, true); // true = HTML

            // 3. Dispatch to SMTP server
            mailSender.send(message);
            log.info("Email successfully sent to: {}", to);
        } catch (MessagingException e) {
            log.error("Failed to construct or send email to: {}", to, e);
        }
    }
}
```

### Example 3: Dedicated Task Executor Configuration for Email Dispatch
```java
package com.example.platform.notification.config;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.scheduling.annotation.EnableAsync;
import org.springframework.scheduling.concurrent.ThreadPoolTaskExecutor;

import java.util.concurrent.Executor;

@Configuration
@EnableAsync
public class NotificationExecutorConfig {

    @Bean(name = "emailTaskExecutor")
    public Executor emailTaskExecutor() {
        ThreadPoolTaskExecutor executor = new ThreadPoolTaskExecutor();
        executor.setCorePoolSize(4);
        executor.setMaxPoolSize(16);
        executor.setQueueCapacity(200);
        executor.setThreadNamePrefix("email-worker-");
        executor.setWaitForTasksToCompleteOnShutdown(true);
        executor.setAwaitTerminationSeconds(30);
        executor.initialize();
        return executor;
    }
}
```

### Example 4: Synchronous Dispatch in Controller (Anti-Pattern Fix)
```java
// ANTI-PATTERN: Sending emails synchronously inside the controller method.
// If the SMTP server hangs, the HTTP request hangs, locking the database transaction.
// @PostMapping("/register")
// public User register(@RequestBody User user) {
//     userRepository.save(user);
//     mailSender.send(simpleMessage); // STALLS FOR SECONDS!
//     return user;
// }

// FIX: Publish a Spring ApplicationEvent and handle email dispatch asynchronously after commit
package com.example.platform.notification.listener;

import com.example.platform.notification.service.EmailNotificationService;
import org.springframework.stereotype.Component;
import org.springframework.transaction.event.TransactionPhase;
import org.springframework.transaction.event.TransactionalEventListener;

import java.util.Map;

public record UserRegisteredEvent(String email, String username, String verifyToken) {}

@Component
public class UserRegistrationNotificationListener {

    private final EmailNotificationService emailService;

    public UserRegistrationNotificationListener(EmailNotificationService emailService) {
        this.emailService = emailService;
    }

    @TransactionalEventListener(phase = TransactionPhase.AFTER_COMMIT)
    public void onUserRegistered(UserRegisteredEvent event) {
        // Only fires after database transaction commits successfully
        emailService.sendTemplatedEmail(
                event.email(),
                "Welcome to Platform! Please verify your account",
                "welcome",
                Map.of(
                        "recipientName", event.username(),
                        "verificationUrl", "https://app.example.com/verify?token=" + event.verifyToken(),
                        "expirationHours", 24
                )
        );
    }
}
```

### Example 5: High-Performance Multipart Email with PDF Invoice Attachment
```java
package com.example.platform.notification.service;

import jakarta.mail.MessagingException;
import jakarta.mail.internet.MimeMessage;
import org.springframework.core.io.ByteArrayResource;
import org.springframework.mail.javamail.JavaMailSender;
import org.springframework.mail.javamail.MimeMessageHelper;
import org.springframework.scheduling.annotation.Async;
import org.springframework.stereotype.Service;

import java.nio.charset.StandardCharsets;

@Service
public class InvoiceEmailService {

    private final JavaMailSender mailSender;

    public InvoiceEmailService(JavaMailSender mailSender) {
        this.mailSender = mailSender;
    }

    @Async("emailTaskExecutor")
    public void sendInvoiceEmail(String to, String invoiceId, byte[] pdfBytes) throws MessagingException {
        MimeMessage message = mailSender.createMimeMessage();
        MimeMessageHelper helper = new MimeMessageHelper(
                message,
                MimeMessageHelper.MULTIPART_MODE_MIXED_RELATED,
                StandardCharsets.UTF_8.name());

        helper.setFrom("billing@example.com");
        helper.setTo(to);
        helper.setSubject("Your Tax Invoice #" + invoiceId);
        helper.setText("<h3>Thank you for your business.</h3><p>Attached is your PDF invoice.</p>", true);

        // Attach binary PDF without saving to disk
        helper.addAttachment("Invoice-" + invoiceId + ".pdf", new ByteArrayResource(pdfBytes), "application/pdf");

        mailSender.send(message);
    }
}
```

---

## 6. Best Practices

1. **Always Dispatch Emails Asynchronously**: Never invoke `JavaMailSender.send()` on the main HTTP container thread; always delegate to an `@Async` executor or a message broker.
2. **Use `@TransactionalEventListener(AFTER_COMMIT)`**: Never dispatch confirmation emails inside uncommitted transactions; if the transaction rolls back, users still receive phantom emails.
3. **Configure Strict Connection and Socket Timeouts**: Set `connectiontimeout`, `timeout`, and `writetimeout` in `mail.smtp` properties to 5000ms to prevent hanging worker threads.
4. **Use Multipart HTML + Plaintext Fallback**: Construct MIME emails with both HTML and plain-text alternatives (`helper.setText(plainText, htmlText)`) for maximum deliverability and accessibility.
5. **Use Mailpit or GreenMail in Testing**: Integrate lightweight mock SMTP servers (Mailpit / GreenMail) in local development and CI to inspect emails without contacting real SMTP servers.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Synchronous email dispatch inside `@Transactional` methods | SMTP slowness keeps database locks open; failures trigger unwanted DB rollback | Move dispatch to `@TransactionalEventListener(phase = AFTER_COMMIT)`. |
| Omitting SMTP socket timeouts | Thread hangs indefinitely when SMTP server disconnects mid-handshake | Configure `mail.smtp.connectiontimeout` and `timeout` properties. |
| Forgetting UTF-8 encoding configuration | International characters (accents, emojis) appear as broken question marks | Pass `StandardCharsets.UTF_8.name()` to `MimeMessageHelper`. |
| Committing production SMTP credentials to Git | Credentials stolen by automated GitHub credential scanners | Inject credentials via environment variables (`SPRING_MAIL_PASSWORD`). |
| Inlined images using base64 data URIs | Major email clients (Gmail, Outlook) strip base64 image strings | Use CID attachments (`helper.addInline("logo", resource)`) or host images on S3/CDN. |
| Missing SPF, DKIM, and DMARC DNS records | Corporate email filters classify notifications directly as spam | Configure domain SPF and DKIM records on the sending domain. |

---

## 8. Performance & Security Notes

- **Credential Masking**: Ensure Spring Boot Actuator's `env` endpoint masks `spring.mail.password` to prevent credential leakage.
- **HTML Injection / Sanitization**: Validate user inputs before merging into Thymeleaf email templates to prevent HTML injection attacks into other users' mailboxes.
- **SMTP Connection Pooling**: Standard JavaMail creates a new TCP socket per email. For high volume (> 100 emails/sec), replace direct SMTP with HTTP API SDKs (e.g., AWS SES SDK or SendGrid HTTP API) that reuse HTTP/2 connection pools.

---

## 9. Comparison / When to Use What

| Notification Channel | SMTP / JavaMailSender | Cloud Email API (SES / SendGrid) | WebSockets / Push |
|---|---|---|---|
| **Protocol** | SMTP / STARTTLS (TCP) | HTTP/1.1 or HTTP/2 REST API | TCP / WebSocket Framing |
| **Throughput** | 10 - 50 emails/sec per host | 500 - 5000+ emails/sec | 100,000+ real-time events/sec |
| **Delivery Guarantee** | Store and forward | Delivery metrics, bounce webhooks | Ephemeral (delivered if online) |
| **Best For** | Standard enterprise apps, intranet | High-volume SaaS, transactional apps | In-app live alerts, user presence |

---

## 10. Interview Questions

<details>
<summary>1. Why should emails never be sent synchronously inside an HTTP controller method?</summary>
SMTP communication is inherently slow and subject to network latency, TLS handshakes, and third-party rate limits. Synchronous sending blocks the HTTP request worker thread for seconds, degrading user experience and risking thread pool exhaustion. If executed inside a database transaction, it holds locks unnecessarily.
</details>

<details>
<summary>2. What is the advantage of using `@TransactionalEventListener(phase = AFTER_COMMIT)` for email dispatch?</summary>
It guarantees that the email notification is only dispatched if the surrounding database transaction successfully commits. If the transaction rolls back due to a constraint violation or exception, the email event is discarded, preventing users from receiving misleading notifications.
</details>

<details>
<summary>3. How does Thymeleaf integrate with Spring's JavaMailSender?</summary>
Thymeleaf acts as the HTML rendering engine. Application services populate a `org.thymeleaf.context.Context` with model attributes and invoke `templateEngine.process("template.html", context)`. The resulting HTML string is set on the `MimeMessageHelper.setText(html, true)`.
</details>

<details>
<summary>4. How do you attach inline images to an email so Gmail displays them without blocking external URLs?</summary>
Using Content-ID (CID) inline attachments. In the HTML template, reference the image as `<img src="cid:companyLogo" />`. In Java, invoke `helper.addInline("companyLogo", new ClassPathResource("images/logo.png"))`.
</details>

<details>
<summary>5. What are the essential SMTP timeout properties that must be configured in Spring Boot?</summary>
`mail.smtp.connectiontimeout` (socket connection timeout), `mail.smtp.timeout` (socket read timeout), and `mail.smtp.writetimeout` (socket write timeout). Setting these (e.g., to 5000ms) prevents worker threads from hanging indefinitely during network partitions.
</details>

<details>
<summary>6. How do you test email delivery in integration tests without sending real emails?</summary>
Using mock SMTP servers such as GreenMail (embedded in JUnit) or Mailpit (in Docker/Testcontainers). Tests configure Spring's `spring.mail.port` to point to the mock server, trigger the business logic, and assert that the mock server received the expected MIME message with correct recipients and headers.
</details>

<details>
<summary>7. What is the difference between SimpleMailMessage and MimeMessage?</summary>
`SimpleMailMessage` only supports plain-text emails without formatting, attachments, or custom encodings. `MimeMessage` supports MIME standards, including rich HTML bodies, multipart attachments, alternative text versions, and inline images.
</details>

<details>
<summary>8. Why is direct SMTP sending sometimes replaced by REST APIs (like AWS SES or SendGrid)?</summary>
SMTP requires stateful TCP handshakes and connection setup per message, which is inefficient at high volumes. REST APIs utilize persistent HTTP connection pools, deliver higher throughput, provide immediate webhook telemetry (opens, clicks, bounces), and handle IP warm-up and reputation management automatically.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1 (Easy): Send a simple plain text email</summary>
Write a method using `JavaMailSender.send(SimpleMailMessage)` that sends a password reset instructions text email.
</details>

<details>
<summary>Exercise 2 (Easy): Configure SMTP properties for local Mailpit</summary>
Configure `application.properties` with host `localhost`, port `1025`, and 5000ms timeouts.
</details>

<details>
<summary>Exercise 3 (Medium): Render an HTML email with Thymeleaf Context</summary>
Create a Thymeleaf template `receipt.html` and write a service method that injects order items and total cost, sending a formatted HTML email.
</details>

<details>
<summary>Exercise 4 (Medium): Implement an event-driven email dispatcher</summary>
Create an `OrderPlacedEvent` and an `@Async` listener using `@TransactionalEventListener(phase = AFTER_COMMIT)` to dispatch order confirmation emails.
</details>

<details>
<summary>Exercise 5 (Hard): Send an email with dynamically generated PDF attachment</summary>
Write a service method that generates a PDF byte array using OpenPDF/iText, attaches it via `MimeMessageHelper.addAttachment()`, and dispatches it asynchronously.
</details>

<details>
<summary>Exercise 6 (Hard): Integration test with GreenMail JUnit 5 extension</summary>
Write a Spring Boot integration test using the GreenMail extension that verifies that triggering registration sends an email containing a verification link.
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------+
|                       Spring Boot Email & Notifications                         |
+---------------------------------------------------------------------------------+
| Starter:            spring-boot-starter-mail + spring-boot-starter-thymeleaf    |
| Core Bean:          JavaMailSender                                              |
| MIME Builder:       MimeMessageHelper(msg, MULTIPART_MODE_MIXED_RELATED, UTF_8) |
| HTML Rendering:     templateEngine.process("email/template.html", context)      |
| Async Rule:         Always dispatch via @Async("emailTaskExecutor")             |
| Transaction Rule:   Use @TransactionalEventListener(phase = AFTER_COMMIT)       |
| Timeout Flags:      mail.smtp.connectiontimeout, timeout, writetimeout = 5000ms |
| Attachments:        helper.addAttachment("file.pdf", byteArrayResource)         |
+---------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Official Spring Boot Mail Documentation](https://docs.spring.io/spring-boot/docs/current/reference/html/features.html#features.email)
- [Thymeleaf Spring 6 Integration Guide](https://www.thymeleaf.org/doc/tutorials/3.1/thymeleafspring.html)
- [Jakarta Mail Specification](https://jakarta.ee/specifications/mail/)
