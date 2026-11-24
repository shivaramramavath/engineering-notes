# SSL/TLS & Mutual TLS (mTLS) with Spring Boot SSL Bundles

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | Cryptography Basics (X.509, PKI, Keystores), HTTP Clients, TLS Handshake |
| **Estimated Reading Time** | 25 minutes |
| **Status** | Production-Ready |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Baseline** | OpenJDK 21 LTS / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | TLSv1.3, PKCS12, OpenJDK 21.0.2 |

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
9. [Comparison: Standard TLS vs Mutual TLS (mTLS)](#9-comparison-standard-tls-vs-mutual-tls-mtls)
10. [Interview Questions & Deep Dives](#10-interview-questions--deep-dives)
11. [Hands-On Practice Exercises](#11-hands-on-practice-exercises)
12. [Summary & Quick Reference](#12-summary--quick-reference)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

In zero-trust architectures and regulated financial or healthcare domains, encrypting traffic in transit using standard HTTPS is only the first step. Microservices must also cryptographically verify the identity of the client calling them—a pattern known as **Mutual TLS (mTLS)**.

### The Problem
Historically, configuring SSL/TLS in Spring applications was painful:
1. Developers set global JVM system properties (`-Djavax.net.ssl.trustStore=...`), which polluted the entire JVM process and prevented configuring different truststores for different external APIs.
2. Writing programmatic `SSLContext` boilerplate with `KeyStore`, `KeyManagerFactory`, and `TrustManagerFactory` required dozens of lines of error-prone cryptographic code.
3. Rotating expiring certificates required restarting the entire JVM.

### The Solution: Spring Boot SSL Bundles (Spring Boot 3.1+)
Spring Boot 3.1 introduced **SSL Bundles** (`spring.ssl.bundle`). SSL bundles allow you to declare named collections of key stores, trust stores, key passwords, and ciphers directly in `application.yaml`. You can then attach these named bundles cleanly to embedded web servers, `RestClient`, `WebClient`, `RestTemplate`, or HTTP interfaces without writing a single line of raw cryptographic Java code.

---

## 2. Core Concepts

### Mental Model & mTLS Cryptographic Handshake
```
   Client (Payment Service)                              Server (Banking Core)
             │                                                     │
             │ 1. ClientHello (Supported TLS versions & Ciphers)   │
             ├────────────────────────────────────────────────────►│
             │                                                     │
             │ 2. ServerHello + Server Certificate + CertificateRequest
             │◄────────────────────────────────────────────────────┤
             │                                                     │
             │ Evaluates Server Certificate against Truststore:    │
             │ • Is issuer CA trusted? YES                         │
             │ • Is certificate unexpired & valid? YES             │
             │                                                     │
             │ 3. Client Certificate + Finished                    │
             ├────────────────────────────────────────────────────►│
             │                                                     │ Evaluates Client Certificate:
             │                                                     │ • Is Client cert issued by CA? YES
             │                                                     │ • Establishes identity / CN
             │                                                     │
             │ 4. Session Keys Established (AES-GCM-256)           │
             │◄────────────────────────────────────────────────────►│
             │           Encrypted Mutual Communication            │
```

### Core SSL Bundle Types in Spring Boot
- **`jks`**: Standard Java Keystore or PKCS12 files (`.p12`, `.pfx`, `.jks`).
- **`pem`**: Plain text privacy-enhanced mail certificates (`.crt`, `.pem`) and private keys without requiring `keytool` conversions.
- **`watch`**: Automatically reloads certificates from disk when updated (zero-downtime certificate rotation).

---

## 3. How It Works Internally

### `SslBundles` Abstraction
1. Spring Boot's `SslAutoConfiguration` creates an `SslBundles` bean containing all configured bundles defined under `spring.ssl.bundle.*`.
2. Each `SslBundle` provides pre-configured:
   - `KeyManagerFactory`: Holds client/server identity private keys and certificates.
   - `TrustManagerFactory`: Validates incoming or outgoing certificates against trusted Root/Intermediate CAs.
   - `SSLContext`: The initialized cryptographic engine configured with TLSv1.3 / TLSv1.2 protocols.
3. Client builders (`RestClient.Builder`, `WebClient.Builder`) provide an `.apply(sslBundle.createCustomizer())` method that wires the underlying request factory cleanly.

---

## 4. Syntax & Configuration

### Configuring SSL Bundles in `application.yaml`
```yaml
spring:
  ssl:
    bundle:
      # Option 1: PKCS12 Keystore Bundle
      jks:
        payment-gateway-mtls:
          key:
            alias: client-cert
            password: "clientSecretKeyPassword"
          keystore:
            location: classpath:certs/client-identity.p12
            password: "keystorePassword"
            type: PKCS12
          truststore:
            location: classpath:certs/trusted-cas.p12
            password: "truststorePassword"
            type: PKCS12
          protocol: TLSv1.3

      # Option 2: PEM Plaintext Certificate Bundle (with automated hot-reload!)
      pem:
        vault-cert-bundle:
          reload-on-update: true # Hot-reload on certificate renewal!
          keystore:
            certificate: file:/etc/tls/client.crt
            private-key: file:/etc/tls/client.key
          truststore:
            certificate: file:/etc/tls/ca-bundle.crt
```

---

## 5. Practical Examples

### Example 1: Wiring an SSL Bundle to `RestClient` for mTLS Communication

```java
package com.example.clients.config;

import org.springframework.boot.autoconfigure.web.client.RestClientSsl;
import org.springframework.boot.ssl.SslBundle;
import org.springframework.boot.ssl.SslBundles;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.web.client.RestClient;

@Configuration(proxyBeanMethods = false)
public class MtlsRestClientConfig {

    @Bean
    public RestClient paymentGatewayMtlsClient(RestClient.Builder builder, SslBundles sslBundles) {
        // Retrieve the pre-configured mTLS SSL bundle defined in YAML
        SslBundle mtlsBundle = sslBundles.getBundle("payment-gateway-mtls");

        return builder
                .baseUrl("https://banking-partner.internal/api/v1")
                // Applies KeyManager (client cert) and TrustManager (server CA) cleanly!
                .apply(RestClientSsl.from(mtlsBundle))
                .defaultHeader("Accept", "application/json")
                .build();
    }
}
```

### Example 2: Configuring Inbound Server-Side mTLS (Requiring Client Certificates)

When the embedded Tomcat server itself must enforce that callers present a valid client certificate:

```yaml
# application.yaml: Inbound mTLS on embedded Tomcat
server:
  port: 8443
  ssl:
    bundle: server-inbound-mtls
    client-auth: need # 'need' enforces mandatory client certificate (403 if absent); 'want' is optional

spring:
  ssl:
    bundle:
      jks:
        server-inbound-mtls:
          keystore:
            location: classpath:certs/server-identity.p12
            password: "serverPassword"
            type: PKCS12
          truststore:
            location: classpath:certs/allowed-clients-ca.p12
            password: "truststorePassword"
            type: PKCS12
```

```java
package com.example.web.controller;

import jakarta.servlet.http.HttpServletRequest;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

import java.security.cert.X509Certificate;

@RestController
@RequestMapping("/api/secure")
public class MtlsPrincipalInspectionController {

    @GetMapping("/identity")
    public ResponseEntity<String> inspectClientCertificate(HttpServletRequest request) {
        // Extract client certificates verified during the mTLS handshake
        X509Certificate[] certs = (X509Certificate[]) request.getAttribute("jakarta.servlet.request.X509Certificate");

        if (certs != null && certs.length > 0) {
            String clientDn = certs[0].getSubjectX500Principal().getName();
            return ResponseEntity.ok("mTLS Handshake Authenticated! Client Identity: " + clientDn);
        }

        return ResponseEntity.status(403).body("No verified client certificate found.");
    }
}
```

### Example 3: Edge Case — Hot Reloading Expired Certificates with Zero Downtime

When using Kubernetes cert-manager or HashiCorp Vault, certificates on disk are refreshed periodically. Enabling `reload-on-update: true` re-reads certificates automatically without restarting the Spring Boot pod.

```yaml
spring:
  ssl:
    bundle:
      pem:
        dynamic-service-mesh:
          reload-on-update: true
          keystore:
            certificate: file:/vault/secrets/service.crt
            private-key: file:/vault/secrets/service.key
          truststore:
            certificate: file:/vault/secrets/root-ca.crt
```

```java
package com.example.clients.ssl;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.boot.ssl.SslBundle;
import org.springframework.boot.ssl.SslBundles;
import org.springframework.stereotype.Service;

@Service
public class SslHealthService {

    private static final Logger log = LoggerFactory.getLogger(SslHealthService.class);
    private final SslBundles sslBundles;

    public SslHealthService(SslBundles sslBundles) {
        this.sslBundles = sslBundles;
    }

    public void logActiveCertificateDetails() {
        SslBundle bundle = sslBundles.getBundle("dynamic-service-mesh");
        log.info("SSL Bundle protocol: {}", bundle.getProtocol());
        // Reloaded automatically when file modification timestamp updates!
    }
}
```

### Example 4: Anti-Pattern vs Production Fix

#### Anti-Pattern: Global JVM System Properties
```java
// ANTI-PATTERN: Pollutes the entire JVM process!
// If API-A needs truststore-A and API-B needs truststore-B, they clobber each other!
System.setProperty("javax.net.ssl.trustStore", "/path/to/truststore.jks");
System.setProperty("javax.net.ssl.trustStorePassword", "password");
```

#### Production Fix: Isolated Scoped SSL Bundles
```yaml
# PRODUCTION FIX: Distinct, scoped bundles per external integration
spring:
  ssl:
    bundle:
      jks:
        payment-gateway:
          truststore: { location: "classpath:certs/payment-ca.p12", password: "pass" }
        inventory-api:
          truststore: { location: "classpath:certs/inventory-ca.p12", password: "pass" }
```

### Example 5: High-Performance Pure PEM Certificate Configuration (No Keytool Needed)

```yaml
spring:
  ssl:
    bundle:
      pem:
        modern-cloud-tls:
          protocol: TLSv1.3
          ciphers:
            - TLS_AES_128_GCM_SHA256
            - TLS_AES_256_GCM_SHA384
          keystore:
            certificate: |
              -----BEGIN CERTIFICATE-----
              MIIB/TCCAZagAwIBAgIUW6o1R...
              -----END CERTIFICATE-----
            private-key: |
              -----BEGIN PRIVATE KEY-----
              MIGHAgEAMBMGByqGSM49AgEG...
              -----END PRIVATE KEY-----
          truststore:
            certificate: |
              -----BEGIN CERTIFICATE-----
              MIICPTCCAaWgAwIBAgIJAN...
              -----END CERTIFICATE-----
```

---

## 6. Best Practices

1. **Adopt Spring Boot SSL Bundles**: Never configure SSL via global JVM `-Djavax.net.ssl` system properties. Always use named SSL bundles (`spring.ssl.bundle`).
2. **Favor PKCS12 or PEM over JKS**: Modern Java 21+ favors PKCS12 (`.p12`) or plaintext PEM files (`.crt`/`.key`). JKS (Java KeyStore) is a legacy proprietary format.
3. **Enforce TLSv1.3**: Set `protocol: TLSv1.3` to mandate modern, secure cipher suites and benefit from faster 1-RTT handshake latency.
4. **Enable `reload-on-update` in Kubernetes**: Combine `reload-on-update: true` with cert-manager to eliminate pod restarts during automated certificate renewal.
5. **Separate Keystore from Truststore**:
   - **Keystore**: Your identity (private key + signed public certificate).
   - **Truststore**: Who you trust (Root and Intermediate CA certificates).

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **`SSLHandshakeException: PKIX path building failed`** | The server's certificate was issued by an internal CA not present in your truststore. | Add the internal CA certificate to the bundle's `truststore`. |
| **`SSLHandshakeException: Received fatal alert: bad_certificate`** | In mTLS, the client certificate presented was rejected or not signed by the server's trusted CA. | Verify client certificate chain matches server's truststore CA. |
| **Using `server.ssl.client-auth: want` Instead of `need`** | `want` makes client certificates optional; unauthenticated callers bypass identity checks. | Use `server.ssl.client-auth: need` for strict mTLS enforcement. |
| **Keystore Password Mismatch with Private Key Password** | In PKCS12, keystore and private key passwords must match; mismatch throws `UnrecoverableKeyException`. | Ensure `key.password` matches `keystore.password`. |
| **Leaking Private Keys in Source Control** | Checking in `.p12` or `.key` files to Git is a critical security vulnerability. | Load certificates from mounted container secrets or Vault volumes. |
| **Missing Subject Alternative Name (SAN)** | Modern Java TLS strictly verifies hostname against SAN. Certificates with only Common Name (CN) fail. | Generate certificates with proper DNS/IP Subject Alternative Names. |

---

## 8. Performance & Security Considerations

### TLSv1.3 vs TLSv1.2 Handshake Latency
TLSv1.3 completes full cryptographic handshakes in **1 Round Trip Time (1-RTT)** compared to 2-RTT in TLSv1.2, reducing handshake latency by 50%. Furthermore, TLSv1.3 supports **0-RTT resumption** (early data). Mandate TLSv1.3 in internal microservice networks.

### Session Resumption & Keep-Alive Synergy
The cryptographic computation required for an mTLS handshake consumes significant CPU. Always pair mTLS clients with long-lived HTTP keep-alive connection pools so that existing secure TCP sessions are reused across thousands of requests without re-negotiating handshakes.

---

## 9. Comparison: Standard TLS vs Mutual TLS (mTLS)

| Feature | Standard TLS (One-Way) | Mutual TLS (mTLS - Two-Way) |
| :--- | :--- | :--- |
| **Server Identity Verified?** | Yes (Client validates Server) | Yes (Client validates Server) |
| **Client Identity Verified?** | No (Relies on Bearer tokens/passwords) | Yes (Server cryptographically validates Client) |
| **Tamper Resistance** | High | Maximum (Cryptographic hardware/cert bound) |
| **Key Management Overhead** | Low (Server cert only) | High (Every client requires cert + rotation) |
| **Best Used For** | Public web traffic, external consumer APIs | Internal microservice mesh, banking gateways |

---

## 10. Interview Questions & Deep Dives

<details>
<summary><strong>Q1: What is the architectural difference between a KeyStore and a TrustStore in Java?</strong></summary>

**Answer:**
- **KeyStore**: Holds your own cryptographic identity—your private key and corresponding public certificate chain. It is presented to remote parties to prove who you are (e.g. during an inbound TLS handshake or outbound mTLS).
- **TrustStore**: Holds the public certificates of trusted Certificate Authorities (CAs). It is used to verify and validate the identity of remote parties presenting certificates to you.
</details>

<details>
<summary><strong>Q2: What are Spring Boot SSL Bundles and why are they superior to legacy configurations?</strong></summary>

**Answer:**
SSL Bundles (introduced in Spring Boot 3.1) provide a centralized, declarative configuration mechanism in `application.yaml` for keystores, truststores, protocols, and PEM certificates. They eliminate the need for global JVM system properties (`-Djavax.net.ssl`), allowing multiple HTTP clients to use completely different truststores within the same application. They also natively support zero-downtime certificate hot-reloading (`reload-on-update`).
</details>

<details>
<summary><strong>Q3: What causes `SSLHandshakeException: PKIX path building failed: sun.security.provider.certpath.SunCertPathBuilderException`?</strong></summary>

**Answer:**
This occurs when the client attempts to establish a TLS connection with a server whose certificate was signed by a Certificate Authority (CA) that does not exist in the client's configured truststore. This commonly happens with internal corporate CAs, self-signed certificates, or staging environments. The remedy is to export the CA's root certificate and import it into the application's SSL bundle `truststore`.
</details>

<details>
<summary><strong>Q4: What is the difference between `server.ssl.client-auth: need` and `server.ssl.client-auth: want`?</strong></summary>

**Answer:**
- `need`: Client authentication is strictly **mandatory**. The server demands a client certificate during the TLS handshake. If the client fails to provide a valid certificate signed by a trusted CA, the TLS handshake fails immediately with a fatal alert.
- `want`: Client authentication is **optional**. The server requests a client certificate, but if the client does not provide one, the handshake still succeeds. The application can inspect whether a certificate was provided later in the filter chain.
</details>

<details>
<summary><strong>Q5: Can you configure PEM certificates directly in YAML without using `keytool`?</strong></summary>

**Answer:**
Yes. Spring Boot supports `spring.ssl.bundle.pem.*`. You can reference plaintext PEM certificates and keys using `file:/path/to/cert.crt` and `file:/path/to/key.key` (or embed PEM strings directly inline in YAML), completely bypassing the legacy `keytool` PKCS12 conversion step.
</details>

<details>
<summary><strong>Q6: How does certificate hot-reloading work with `reload-on-update`?</strong></summary>

**Answer:**
When `reload-on-update: true` is enabled on an SSL bundle, Spring Boot registers a file watcher on the certificate and key files. When a tool like Kubernetes cert-manager writes updated certificate files to disk, the watcher detects the change, reconstructs the `KeyManagerFactory` and `SSLContext` in memory, and applies them without terminating the application process.
</details>

<details>
<summary><strong>Q7: How do you extract client certificate details inside a Spring MVC controller?</strong></summary>

**Answer:**
From the `HttpServletRequest` attribute:
`X509Certificate[] certs = (X509Certificate[]) request.getAttribute("jakarta.servlet.request.X509Certificate");`
The first certificate in the array (`certs[0]`) represents the client's end-entity certificate.
</details>

<details>
<summary><strong>Q8: Why is TLSv1.3 faster than TLSv1.2?</strong></summary>

**Answer:**
TLSv1.3 eliminates round trips during the initial handshake. While TLSv1.2 requires two full round trips (2-RTT) to exchange keys, agree on cipher suites, and verify certificates, TLSv1.3 combines key exchange into the `ClientHello` packet, completing the cryptographic handshake in just one round trip (1-RTT).
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): Configure PEM SSL Bundle in YAML</strong></summary>

**Task**: Write the `application.yaml` snippet defining an SSL bundle `my-bundle` using PEM certificate and key files located in `/etc/certs/`.

```yaml
# Solution:
spring:
  ssl:
    bundle:
      pem:
        my-bundle:
          keystore:
            certificate: file:/etc/certs/client.crt
            private-key: file:/etc/certs/client.key
          truststore:
            certificate: file:/etc/certs/ca.crt
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Bind SSL Bundle to Embedded Server</strong></summary>

**Task**: Configure `application.yaml` so embedded Tomcat uses an SSL bundle named `server-tls` on port 8443.

```yaml
# Solution:
server:
  port: 8443
  ssl:
    bundle: server-tls
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): Wire SSL Bundle into `RestClient` Bean</strong></summary>

**Task**: Inject `SslBundles` into a `@Bean` method and apply bundle `"banking-tls"` to a `RestClient`.

```java
// Solution:
@Bean
public RestClient bankingClient(RestClient.Builder builder, SslBundles sslBundles) {
    SslBundle bundle = sslBundles.getBundle("banking-tls");
    return builder
            .baseUrl("https://banking.internal")
            .apply(RestClientSsl.from(bundle))
            .build();
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Extract Client Certificate Common Name</strong></summary>

**Task**: Write a helper method that extracts the Common Name (CN) from an `X509Certificate`.

```java
// Solution:
public String extractCommonName(X509Certificate cert) throws Exception {
    String dn = cert.getSubjectX500Principal().getName();
    LdapName ldapDN = new LdapName(dn);
    for (Rdn rdn : ldapDN.getRdns()) {
        if ("CN".equalsIgnoreCase(rdn.getType())) {
            return rdn.getValue().toString();
        }
    }
    return null;
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Dynamic Certificate Expiration Alerting Service</strong></summary>

**Task**: Build a scheduled service that inspects active `SslBundles`, checks the `getNotAfter()` expiration date on all keystore certificates, and triggers a warning log/metric if any certificate expires in less than 30 days.
</details>

<details>
<summary><strong>Exercise 6 (Hard): Custom X509 Authentication Filter</strong></summary>

**Task**: Implement a Spring Security filter that extracts the client certificate from the request attributes, validates that its Issuer matches an authorized internal CA fingerprint, and populates `SecurityContextHolder` with granted authorities derived from the certificate's Organizational Unit (OU).
</details>

---

## 12. Summary & Quick Reference

- **Spring Boot 3.1+ Feature**: `spring.ssl.bundle.*` standardizes SSL configurations.
- **Formats**: PKCS12 (`jks`) and plaintext (`pem`).
- **Client Integration**: `RestClient.Builder.apply(RestClientSsl.from(bundle))`.
- **Zero Downtime**: Set `reload-on-update: true` for automated disk reload.
- **mTLS Enforce**: Set `server.ssl.client-auth: need`.

---

## 13. Further Reading

- [Spring Boot Reference: SSL Bundles](https://docs.spring.io/spring-boot/reference/features/ssl.html)
- [RFC 8446: The Transport Layer Security (TLS) Protocol Version 1.3](https://www.rfc-editor.org/rfc/rfc8446.html)
- [NIST Special Publication 800-52: Guidelines for TLS Implementations](https://csrc.nist.gov/publications/detail/sp/800-52/rev-2/final)
- [Kubernetes Cert-Manager Integration with Spring Boot](https://cert-manager.io/)
