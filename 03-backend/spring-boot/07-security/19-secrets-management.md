# 19. Secrets Management & Vault Integration in Spring Boot

| Metadata | Value |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | `01-spring-security-architecture.md`, Spring Boot Configuration, Cloud Infrastructure Basics |
| **Estimated Reading Time** | 35 minutes |
| **Status** | Complete |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | OpenJDK 21, Spring Boot 3.4.1, HashiCorp Vault 1.17, AWS Secrets Manager |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance & Security Notes](#8-performance--security-notes)
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### Problem Statement
Storing credentials, database passwords, TLS private keys, and third-party API tokens inside static configuration files (`application.yml` or `application.properties`) creates catastrophic security vulnerabilities:
- Source code repositories (GitHub/GitLab) get cloned to developer laptops, logged in CI/CD build outputs, or leaked publicly.
- Static credentials rarely get rotated because changing a password requires application redeployments and service downtime.
- In multi-service microservice architectures, there is no centralized audit trail tracking which service accessed which database secret or API key.

**Secrets Management** provides centralized, encrypted, audited, and dynamically rotating credential storage using specialized engines such as **HashiCorp Vault**, **AWS Secrets Manager**, or **Kubernetes Secrets**.

### Historical Evolution
- **Early Enterprise Java (2000s)**: Plaintext JNDI configurations or hardcoded database strings in XML files.
- **Spring Cloud Config & Jasypt (2010s)**: Introduced encrypted property strings (`ENC(...)`) and externalized Git-backed config servers, which still lacked dynamic credential rotation.
- **Modern Cloud-Native Spring Boot 3.x/4.x**: Direct integration with HashiCorp Vault (`spring-cloud-starter-vault-config`), AWS Secrets Manager, and Kubernetes volume mounts. Supports dynamic database credential generation with automated lease renewal and zero-downtime certificate rotation.

### Real-World Relevance
Proper secrets management is mandatory for SOC 2, ISO 27001, and PCI-DSS compliance, eliminating static hardcoded passwords across the entire software delivery lifecycle.

---

## 2. Core Concepts

### Architectural Contracts
- **Key-Value (KV) Secret Engine**: Stores static encrypted secrets (e.g., Stripe API keys, Slack Webhooks).
- **Dynamic Secret Engine**: Generates short-lived, on-demand credentials with an automated Time-To-Live (TTL) lease (e.g., creating a dedicated PostgreSQL user per Spring Boot instance).
- **Transit Secret Engine (Cryptography as a Service)**: Offloads encryption and decryption of sensitive application data (credit cards, SSNs) to Vault without storing the plaintext data in Vault.
- **Lease & Renewal**: A duration during which a dynamic secret is valid. Spring Vault automatically renews active leases and invalidates them upon application shutdown.
- **`VaultTemplate`**: Spring's high-level abstraction for interacting with Vault's HTTP API.

### The Mental Model
```
[ Spring Boot Microservice ]
        │
        ├── 1. Authenticates using Kubernetes ServiceAccount Token or AppRole
        │      POST /v1/auth/kubernetes/login
        ▼
[ HashiCorp Vault Server ]
        │
        ├── 2. Validates identity with Kubernetes API Server
        ├── 3. Issues Vault Client Token with Lease (TTL = 1 hour)
        │
        ├── 4. Spring requests Database Credentials:
        │      GET /v1/database/creds/order-service-role
        │
        ├── 5. Vault generates unique temporary PostgreSQL user:
        │      Username: v-kub-order-abc123-1700000000
        │      Password: temporary-high-entropy-secret
        │      TTL: 3600 seconds
        ▼
[ PostgreSQL Database ] ◄── Spring Boot connects using dynamically generated credentials!
```

### Invariants
1. Plaintext secrets must **never** be committed to version control systems under any circumstance.
2. Dynamic credentials must have a finite TTL and automated background renewal.
3. Access to secrets must be governed by least-privilege IAM policies and fully audited.

---

## 3. How It Works Internally

### Spring Cloud Vault Bootstrap & Lease Renewal
```
Spring Boot Application Startup
   │
   ├── 1. ConfigDataLocationResolver processes 'vault://' config tree
   ├── 2. VaultAuthentication executes (AppRole / Kubernetes / AWS IAM)
   ├── 3. Fetches secrets from KV engine (secret/data/order-service)
   ├── 4. Requests dynamic DB credentials from /database/creds/readonly
   ├── 5. Injects resolved properties into Spring Environment:
   │      spring.datasource.username = <generated_user>
   │      spring.datasource.password = <generated_pass>
   │
   └── 6. Launches SecretLeaseContainer in background:
          ├─ At 75% TTL: Sends heartbeat renew-lease RPC to Vault
          └─ On JVM shutdown: Revokes database credentials immediately
```

---

## 4. Syntax & Usage

### 1. Starter Dependencies (`pom.xml`)
```xml
<dependency>
    <groupId>org.springframework.cloud</groupId>
    <artifactId>spring-cloud-starter-vault-config</artifactId>
</dependency>
<dependency>
    <groupId>org.springframework.vault</groupId>
    <artifactId>spring-vault-core</artifactId>
</dependency>
```

### 2. Configuration (`application.yml`)
```yaml
spring:
  application:
    name: order-service
  cloud:
    vault:
      uri: https://vault.corp.internal:8200
      ssl:
        trust-store: classpath:vault-truststore.jks
        trust-store-password: ${TRUSTSTORE_PASSWORD}
      authentication: KUBERNETES
      kubernetes:
        role: order-service-role
        kubernetes-path: kubernetes
      kv:
        enabled: true
        backend: secret
        default-context: order-service
      database:
        enabled: true
        role: order-service-db-role
        backend: database
```

---

## 5. Practical Examples

### Example 1: Using `VaultTemplate` for Transit Encryption (Data at Rest)
Encrypting sensitive customer Personally Identifiable Information (PII) before persisting it to a database using Vault's Transit engine.

```java
package com.example.service;

import org.springframework.stereotype.Service;
import org.springframework.vault.core.VaultTemplate;
import org.springframework.vault.support.Plaintext;

@Service
public class CustomerPiiEncryptionService {

    private final VaultTemplate vaultTemplate;
    private static final String TRANSIT_KEY_NAME = "customer-pii-key";

    public CustomerPiiEncryptionService(VaultTemplate vaultTemplate) {
        this.vaultTemplate = vaultTemplate;
    }

    public String encryptSensitiveData(String plaintextSsn) {
        // Sends plaintext to Vault; Vault returns ciphertext without storing the data
        return vaultTemplate.opsForTransit()
            .encrypt(TRANSIT_KEY_NAME, Plaintext.of(plaintextSsn))
            .getCiphertext();
    }

    public String decryptSensitiveData(String ciphertext) {
        return vaultTemplate.opsForTransit()
            .decrypt(TRANSIT_KEY_NAME, ciphertext)
            .asString();
    }
}
```

### Example 2: AWS Secrets Manager Integration
Loading database passwords dynamically from AWS Secrets Manager using Spring Cloud AWS.

```yaml
# application.yml
spring:
  config:
    import:
      - "aws-secretsmanager:/production/order-service/secrets"
```
```java
package com.example.config;

import org.springframework.beans.factory.annotation.Value;
import org.springframework.context.annotation.Configuration;

@Configuration
public class ThirdPartyApiConfig {

    // Populated directly from AWS Secrets Manager secret key "stripe_api_key"
    @Value("${stripe_api_key}")
    private String stripeApiKey;

    public String getStripeApiKey() {
        return stripeApiKey;
    }
}
```

### Example 3: Edge Case – Dynamic PostgreSQL Credential Rotation with HikariCP
When dynamic database credentials expire or rotate, the database connection pool must update its authentication credentials without dropping active transactions.

```java
package com.example.security.vault;

import com.zaxxer.hikari.HikariDataSource;
import org.springframework.cloud.vault.config.VaultConfigurationProperties;
import org.springframework.context.annotation.Configuration;
import org.springframework.vault.core.lease.SecretLeaseContainer;
import org.springframework.vault.core.lease.event.SecretLeaseRotatedEvent;

@Configuration
public class DynamicDbRotationListener {

    public DynamicDbRotationListener(SecretLeaseContainer leaseContainer, HikariDataSource dataSource) {
        leaseContainer.addLeaseListener(event -> {
            if (event instanceof SecretLeaseRotatedEvent rotatedEvent) {
                if (rotatedEvent.getSource().getPath().contains("database/creds")) {
                    String newUser = (String) rotatedEvent.getSecrets().get("username");
                    String newPass = (String) rotatedEvent.getSecrets().get("password");

                    // Hot-swap credentials inside the Hikari connection pool
                    dataSource.setUsername(newUser);
                    dataSource.setPassword(newPass);
                    // Soft evict idle connections so they re-authenticate with the new credentials
                    dataSource.getHikariPoolMXBean().softEvictConnections();
                }
            }
        });
    }
}
```

### Example 4: Failure Anti-Pattern & Production Fix
**Anti-Pattern**: Using static Jasypt property encryption with the master password committed in cleartext inside environment variables or build scripts.

```properties
# INSECURE: Anyone with read access to the Dockerfile or repository can decrypt secrets!
jasypt.encryptor.password=MasterPassword123
datasource.password=ENC(vK7b4fWz8A9L...)
```

**Production Fix**:
Use identity-based authentication (Kubernetes ServiceAccount or AWS IAM) to retrieve secrets dynamically at runtime without holding static master keys.

```yaml
# SECURE: Zero master keys stored in application code; authenticated via local container identity
spring:
  cloud:
    vault:
      authentication: KUBERNETES
      kubernetes:
        role: payment-service-role
```

### Example 5: High-Performance Non-Blocking WebFlux Vault Integration
Using `ReactiveVaultTemplate` for non-blocking asynchronous secret retrieval.

```java
package com.example.security.reactive;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.vault.core.ReactiveVaultTemplate;
import reactor.core.publisher.Mono;

import java.util.Map;

@Configuration
public class ReactiveSecretsConfig {

    public Mono<String> getApiKeyAsynchronously(ReactiveVaultTemplate reactiveVaultTemplate) {
        return reactiveVaultTemplate
            .read("secret/data/external-services")
            .map(response -> {
                @SuppressWarnings("unchecked")
                Map<String, Object> data = (Map<String, Object>) response.getData().get("data");
                return (String) data.get("api_key");
            });
    }
}
```

---

## 6. Best Practices

1. **Use Ephemeral Dynamic Credentials**: Generate on-demand database users per microservice instance via Vault's database secret engine instead of sharing static credentials.
2. **Authenticate Using Cloud Identities**: Use Kubernetes ServiceAccounts or AWS IAM roles rather than static Vault tokens.
3. **Keep Secret Leases Short**: Set TTLs between 1 hour and 24 hours, letting Spring Vault renew leases automatically.
4. **Use Transit Encryption for PII**: Never store raw credit card numbers or Social Security Numbers in databases; encrypt them via Vault Transit.
5. **Enforce Least Privilege**: Vault policies should only permit reading paths required by the specific microservice (`secret/data/order-service/*`).
6. **Graceful Handling of Revocation**: Handle secret lease expiry events by reconnecting connection pools cleanly without crashing worker pods.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Hardcoding Vault tokens in `application.yml` | The Vault root/service token is leaked into source control. | Use AppRole or Kubernetes authentication. |
| Neglecting SSL verification for Vault | Intercepted traffic exposes unencrypted secrets across the internal network. | Enable TLS verification and provide truststores for Vault communication. |
| Forgetting lease renewal listener | Database connection pool fails with authentication errors when dynamic credentials expire. | Configure `SecretLeaseContainer` and soft-evict idle connections on rotation. |
| High lease volume database exhaustion | Creating thousands of ephemeral users without cleanup exhausts PostgreSQL role limits. | Ensure Spring Vault revokes leases upon JVM termination (`destroyMethod`). |
| Missing fallback defaults | Application fails to start locally when Vault is unreachable in offline development. | Use Spring profiles (`@Profile("!local")`) to load local secrets from environment mocks. |
| Exposing secrets via Actuator `/env` | Actuator displays resolved Vault properties in HTTP responses. | Set `management.endpoint.env.show-values=NEVER`. |

---

## 8. Performance & Security Notes

- **Network Hops at Startup**: Resolving secrets from Vault adds 50–200ms to application startup time. Once resolved, Spring caches properties in memory, introducing zero latency during runtime HTTP requests.
- **Lease Heartbeat Overhead**: Background lease renewals occur every few minutes per service and consume less than 1KB of network bandwidth.
- **Virtual Threads Integration**: Network calls to Vault during lease renewal and secret fetching unmount carrier threads cleanly in Java 21+.

---

## 9. Comparison / When to Use What

| Solution | Dynamic Secrets? | Encryption-as-a-Service? | Infrastructure Complexity | Best Use Case |
|---|---|---|---|---|
| **HashiCorp Vault** | **Yes (Full DB/Cloud support)** | **Yes (Transit Engine)** | High (Self-hosted or HCP) | Multi-cloud enterprise, strict compliance |
| **AWS Secrets Manager** | Partial (Via Lambda rotation) | No | Low (Managed AWS) | 100% AWS cloud-native deployments |
| **Kubernetes Secrets** | No (Static base64) | No | Very Low | Basic Kubernetes container deployments |
| **Jasypt Encrypted Props**| No (Static file) | No | Very Low | Legacy monolithic applications |

---

## 10. Interview Questions

<details>
<summary>1. What is the fundamental difference between static and dynamic secrets in HashiCorp Vault?</summary>

**Answer:**
- **Static Secrets (KV Engine)**: Values are written once and read by clients until manually updated (e.g., a static API key). The secret does not change upon reading.
- **Dynamic Secrets (Database/Cloud Engine)**: Secrets do not exist until they are requested. When an application queries Vault, Vault creates a unique, short-lived user on the fly (e.g., in PostgreSQL), assigns it a specific TTL lease, and automatically deletes the user when the lease expires or is revoked.
</details>

<details>
<summary>2. How does Kubernetes authentication work with Spring Cloud Vault?</summary>

**Answer:**
1. The Spring Boot pod mounts its Kubernetes ServiceAccount JWT at `/var/run/secrets/kubernetes.io/serviceaccount/token`.
2. Spring Vault reads this token and sends it to Vault's `/v1/auth/kubernetes/login` endpoint along with a configured role name.
3. Vault calls the Kubernetes TokenReview API to verify the token's signature, namespace, and service account identity.
4. If valid, Vault issues a scoped client token with attached policies to the Spring Boot application.
</details>

<details>
<summary>3. What is Vault's Transit Secret Engine (Cryptography as a Service)?</summary>

**Answer:**
The Transit engine handles cryptographic functions (encryption, decryption, signing) on data in-transit without persisting the data in Vault. An application sends plaintext data to Vault, and Vault encrypts it using an internal master key and returns the ciphertext. This ensures the master encryption key never leaves Vault, protecting against database dumps and memory scraping attacks.
</details>

<details>
<summary>4. What happens when a Spring Boot instance crashes without revoking its dynamic database lease?</summary>

**Answer:**
Vault associates every dynamic credential with a Time-To-Live (TTL). If the Spring Boot instance crashes and stops sending lease renewal heartbeats, Vault's background lease manager automatically detects the expiration and drops the dynamic user in the database, preventing orphan credential accumulation.
</details>

<details>
<summary>5. How does `spring.config.import` work with modern secrets managers?</summary>

**Answer:**
Spring Boot 2.4+ replaced `bootstrap.yml` with `spring.config.import`. Developers can specify external configuration locations directly in `application.yml`, such as:
`spring.config.import: "vault://secret/data/order-service"` or `"aws-secretsmanager:/production/order-service"`.
Spring resolves these remote property sources before initializing application context beans.
</details>

<details>
<summary>6. How do you rotate database credentials without restarting the Spring Boot JVM?</summary>

**Answer:**
By combining Spring Cloud Vault's `SecretLeaseContainer` with a dynamic connection pool like HikariCP. When Vault emits a `SecretLeaseRotatedEvent`, an application listener updates `dataSource.setUsername()` and `setPassword()`, and invokes `softEvictConnections()`. Existing in-flight queries complete normally, while new connections authenticate using the updated credentials.
</details>

<details>
<summary>7. What are the security risks of storing secrets in environment variables?</summary>

**Answer:**
Environment variables are often exposed through:
1. Process listing tools (`ps aux` or `/proc/<pid>/environ`).
2. Error reports, container crash logs, and debug endpoints.
3. Inherited child processes (e.g., executing shell scripts).
Using memory-injected secrets or dynamic secret engines provides significantly better containment.
</details>

<details>
<summary>8. How do you handle secrets during local development without running a full Vault cluster?</summary>

**Answer:**
1. Use Spring profiles (`@Profile("local")`) to mock secret properties or load them from local `.env` files (excluded from git).
2. Use Testcontainers with `VaultContainer` in integration tests to spin up an ephemeral in-memory Vault instance during builds.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Basic Vault KV Path Configuration (Easy)</summary>

**Task**: Write an `application.yml` snippet using `spring.config.import` to load static key-value secrets from HashiCorp Vault at path `secret/data/application`.

**Solution**:
```yaml
spring:
  config:
    import:
      - "vault://secret/data/application"
  cloud:
    vault:
      uri: https://vault.local:8200
      token: s.myDevToken123
```
</details>

<details>
<summary>Exercise 2: Transit Encryption Utility (Easy)</summary>

**Task**: Implement a utility method using `VaultTemplate` that accepts a plaintext credit card string and returns the encrypted ciphertext string.

**Solution**:
```java
public String encryptCard(String rawCard, VaultTemplate vaultTemplate) {
    return vaultTemplate.opsForTransit()
        .encrypt("payment-key", Plaintext.of(rawCard))
        .getCiphertext();
}
```
</details>

<details>
<summary>Exercise 3: Kubernetes Auth Configuration (Medium)</summary>

**Task**: Configure Spring Cloud Vault to authenticate using the Kubernetes authentication backend with role `"billing-service-role"`.

**Solution**:
```yaml
spring:
  cloud:
    vault:
      authentication: KUBERNETES
      kubernetes:
        role: billing-service-role
        service-account-token-file: /var/run/secrets/kubernetes.io/serviceaccount/token
```
</details>

<details>
<summary>Exercise 4: Lease Renewal Listener (Medium)</summary>

**Task**: Create an event listener that logs a warning whenever a Vault secret lease fails to renew.

**Solution**:
```java
@Component
public class VaultLeaseErrorListener {
    public VaultLeaseErrorListener(SecretLeaseContainer leaseContainer) {
        leaseContainer.addLeaseListener(event -> {
            if (event instanceof SecretLeaseEvent.SecretLeaseErrorEvent errorEvent) {
                LoggerFactory.getLogger(VaultLeaseErrorListener.class)
                    .error("Failed to renew Vault lease for path: {}", errorEvent.getSource().getPath());
            }
        });
    }
}
```
</details>

<details>
<summary>Exercise 5: Dynamic Database Secret Configuration (Hard)</summary>

**Task**: Configure Spring Cloud Vault to fetch dynamic PostgreSQL credentials from path `database/creds/app-readonly` and configure HikariCP to use the returned credentials.

**Solution**:
```yaml
spring:
  cloud:
    vault:
      database:
        enabled: true
        role: app-readonly
        backend: database
  datasource:
    driver-class-name: org.postgresql.Driver
    url: jdbc:postgresql://db.corp:5432/orders
```
</details>

<details>
<summary>Exercise 6: Testcontainers Vault Integration Test (Hard)</summary>

**Task**: Write an integration test using Testcontainers `VaultContainer` that initializes a transit key, encrypts a string, and asserts that the ciphertext can be successfully decrypted back to plaintext.

**Solution**:
```java
@SpringBootTest
@Testcontainers
class VaultIntegrationTest {

    @Container
    static VaultContainer<?> vault = new VaultContainer<>("hashicorp/vault:1.17")
        .withVaultToken("test-root-token");

    @DynamicPropertySource
    static void configureVault(DynamicPropertyRegistry registry) {
        registry.add("spring.cloud.vault.uri", vault::getHttpHostAddress);
        registry.add("spring.cloud.vault.token", () -> "test-root-token");
    }

    @Autowired
    private VaultTemplate vaultTemplate;

    @Test
    void shouldEncryptAndDecryptViaTransitEngine() {
        vaultTemplate.opsForTransit().createKey("test-key");

        String plaintext = "Secret Message";
        String ciphertext = vaultTemplate.opsForTransit()
            .encrypt("test-key", Plaintext.of(plaintext))
            .getCiphertext();

        String decrypted = vaultTemplate.opsForTransit()
            .decrypt("test-key", ciphertext)
            .asString();

        assertThat(decrypted).isEqualTo(plaintext);
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Zero Cleartext Credentials in Git**: Use Vault, AWS Secrets Manager, or Kubernetes Secrets.
- **Dynamic Secrets for Databases**: Ephemeral users with TTL leases prevent credential reuse.
- **Transit Engine for PII**: Offloads field encryption (credit cards, SSNs) to Vault without storing keys in application memory.
- **Automated Lease Renewal**: `SecretLeaseContainer` maintains active leases in the background.
- **Authenticate with Cloud IAM**: Use Kubernetes ServiceAccounts or AWS IAM roles rather than static tokens.

---

## 13. Further Reading
- [Spring Cloud Vault Reference Documentation](https://docs.spring.io/spring-cloud-vault/docs/current/reference/html/)
- [HashiCorp Vault Official Documentation](https://developer.hashicorp.com/vault/docs)
- [AWS Secrets Manager Best Practices](https://docs.aws.amazon.com/secretsmanager/latest/userguide/best-practices.html)
- [NIST SP 800-57: Recommendation for Key Management](https://csrc.nist.gov/publications/detail/sp/800-57-part-1/rev-5/final)
- [Testcontainers Vault Module](https://java.testcontainers.org/modules/vault/)
