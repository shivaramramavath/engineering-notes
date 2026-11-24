# Production Secrets and Configuration Management in Spring Boot

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x |
| **Spring Framework Version** | 6.2.x |
| **Java Version** | 21 LTS |
| **Module** | 14-production-and-deployment |
| **Status** | Complete |

---

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Configuration Reference](#4-syntax--configuration-reference)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices & Production Guidelines](#6-best-practices--production-guidelines)
- [7. Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
- [8. Performance & Security Notes](#8-performance--security-notes)
- [9. Comparison / When to Use What](#9-comparison--when-to-use-what)
- [10. Interview Questions & Answers](#10-interview-questions--answers)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading](#13-further-reading)

---

## 1. What & Why

### Problem Statement
Managing sensitive credentials—database passwords, third-party API keys, JWT signing keys, TLS certificates—is one of the most perilous aspects of enterprise software engineering:
- Hardcoding secrets in source code or `application.yml` leads to credential exposure in Git repositories.
- Baking secrets into Docker container images exposes credentials to anyone with registry read access.
- Static, long-lived credentials are rarely rotated, violating security compliance standards (SOC2, PCI-DSS, ISO 27001).
- In traditional Kubernetes environments, standard `Secret` resources are merely base64-encoded strings, stored in plaintext unless encrypted at rest in etcd.

### Historical Evolution
Early Spring applications used property placeholder encryption tools like Jasypt, which required maintaining a master decryption key in environment variables.

Modern enterprise architecture standardizes on **Centralized Secret Management Engines**:
1. **HashiCorp Vault**: Provides identity-based secret access, dynamic just-in-time database credentials with automatic lease expiration, and transit encryption-as-a-service.
2. **Cloud-Native Vaults (AWS Secrets Manager & Parameter Store)**: Managed cloud secrets integrated via Spring Cloud AWS.
3. **External Secrets Operator (ESO)**: Kubernetes operator that synchronizes secrets from external vaults (AWS, Vault, Azure Key Vault) directly into native Kubernetes Secrets, decoupling Spring Boot from vendor SDKs.

### Production Relevance
Implementing professional secrets management ensures that:
- Credentials rotate automatically without requiring application restarts or pod redeployments.
- Developers and CI/CD systems have zero direct access to production database passwords.
- Every secret access is cryptographically audited and bound to strict time-to-live leases.

---

## 2. Core Concepts

### Architectural Foundations
1. **Dynamic Secrets**: Instead of static credentials, Vault creates short-lived, unique database users on-demand (e.g. valid for 1 hour) with automatic revocation upon lease expiry.
2. **Lease Lifecycle & Renewal**: Secrets are granted with a Time-To-Live (TTL). Spring Cloud Vault maintains a background lease renewal thread to extend leases before they expire.
3. **External Secrets Operator (ESO)**: A Kubernetes operator that reads from external secret stores (Vault, AWS Secrets Manager) and generates standard Kubernetes `Secret` objects inside the cluster.
4. **Spring Cloud Config Data Import (`spring.config.import`)**: Spring Boot 3 mechanism to load external secret trees directly into the `Environment` during startup.
5. **Dynamic Configuration Refresh (`@RefreshScope`)**: Spring Cloud mechanism that re-instantiates beans when underlying configuration properties change without restarting the JVM process.

---

## 3. How It Works Internally

### Dynamic Secret Provisioning & Lease Renewal with HashiCorp Vault

```
+------------------------------------------------------------------------------------+
|                         SPRING BOOT 3 APPLICATION                                  |
|                                                                                    |
|  1. Spring Cloud Vault authenticates via Kubernetes ServiceAccount Token           |
|     (POST /v1/auth/kubernetes/login)                                               |
+------------------------------------------------------------------------------------+
                                         |
                                         v
+------------------------------------------------------------------------------------+
|                         HASHICORP VAULT SERVER                                     |
|  2. Validates token with Kubernetes TokenReview API                                |
|  3. Issues Vault Client Token with configured IAM policy                           |
+------------------------------------------------------------------------------------+
                                         |
                                         v
+------------------------------------------------------------------------------------+
| 4. DYNAMIC DATABASE CREDENTIAL GENERATION                                          |
|    Spring Boot requests: GET /v1/database/creds/order-service-role                 |
|    - Vault connects to PostgreSQL as DBA                                           |
|    - Executes: CREATE ROLE "v-token-xyz" WITH LOGIN PASSWORD 'abc...' VALID 1 HOUR |
|    - Returns temporary credentials and Lease ID to Spring Boot                     |
+------------------------------------------------------------------------------------+
                                         |
                                         v
+------------------------------------------------------------------------------------+
| 5. HIKARICP POOL INITIALIZATION & LEASE MAINTENANCE                                |
|    - HikariCP initializes datasource using temporary credentials                   |
|    - Spring Cloud Vault background daemon periodically renews lease every 30 mins  |
+------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Configuration Reference

### 1. HashiCorp Vault Integration (`pom.xml` & `application.yml`)
```xml
<dependency>
    <groupId>org.springframework.cloud</groupId>
    <artifactId>spring-cloud-starter-vault-config</artifactId>
</dependency>
```

```yaml
# application.yml
spring:
  cloud:
    vault:
      uri: https://vault.internal:8200
      connection-timeout: 3000
      read-timeout: 5000
      authentication: KUBERNETES
      kubernetes:
        role: order-service-role
        kubernetes-path: kubernetes
      # Dynamic PostgreSQL database credentials
      database:
        enabled: true
        role: order-service-db-role
        backend: database
  config:
    import:
      - vault://secret/order-service/production
```

### 2. External Secrets Operator Manifest (`ExternalSecret.yaml`)
Syncs AWS Secrets Manager into native Kubernetes Secret without requiring any Java SDKs in Spring Boot:

```yaml
apiVersion: external-secrets.io/v1beta1
kind: ExternalSecret
metadata:
  name: order-service-db-secret
spec:
  refreshInterval: "1h" # Syncs and rotates every hour
  secretStoreRef:
    name: aws-secrets-manager
    kind: ClusterSecretStore
  target:
    name: order-service-k8s-secret # Generates this native K8s Secret!
    creationPolicy: Owner
  data:
    - secretKey: db-password
      remoteRef:
        key: production/order-service
        property: password
```

---

## 5. Practical Examples

### Example 1: AWS Secrets Manager Integration with Spring Cloud AWS 3
```xml
<!-- pom.xml -->
<dependency>
    <groupId>io.awspring.cloud</groupId>
    <artifactId>spring-cloud-aws-starter-secrets-manager</artifactId>
    <version>3.2.1</version>
</dependency>
```

```yaml
# application-prod.yml
spring:
  config:
    import:
      # Automatically loads all key-value pairs stored in AWS Secrets Manager!
      - aws-secretsmanager:/production/order-service
  datasource:
    # Injected seamlessly from the imported secret!
    url: ${database.url}
    username: ${database.username}
    password: ${database.password}
```

### Example 2: Dynamic Property Refresh with `@RefreshScope`
When secrets or configurations rotate in production, beans annotated with `@RefreshScope` can reload their state without rebooting the JVM:

```java
package com.example.production.secrets;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.cloud.context.config.annotation.RefreshScope;
import org.springframework.stereotype.Service;

@Service
@RefreshScope // Re-created on-demand when /actuator/refresh is triggered!
public class PaymentGatewayClient {

    private static final Logger log = LoggerFactory.getLogger(PaymentGatewayClient.class);

    private final String apiKey;
    private final String endpoint;

    public PaymentGatewayClient(
        @Value("${payment.gateway.api-key}") String apiKey,
        @Value("${payment.gateway.endpoint}") String endpoint) {
        this.apiKey = apiKey;
        this.endpoint = endpoint;
        log.info("Initialized PaymentGatewayClient with endpoint: {}", endpoint);
    }

    public void charge(double amount) {
        log.info("Executing charge against {} with active key [***{}]", 
            endpoint, apiKey.substring(Math.max(0, apiKey.length() - 4)));
    }
}
```

### Example 3: Transit Encryption-as-a-Service with Vault
Instead of managing cryptographic keys in application memory, encrypt sensitive data (e.g. credit card numbers or SSNs) via HashiCorp Vault's Transit Engine:

```java
package com.example.production.secrets;

import org.springframework.stereotype.Service;
import org.springframework.vault.core.VaultOperations;
import org.springframework.vault.support.Plaintext;

@Service
public class CreditCardEncryptionService {

    private final VaultOperations vaultOperations;
    private static final String KEY_NAME = "customer-card-key";

    public CreditCardEncryptionService(VaultOperations vaultOperations) {
        this.vaultOperations = vaultOperations;
    }

    public String encryptCardNumber(String rawCardNumber) {
        // Encrypts in Vault; the plaintext key NEVER leaves the Vault server!
        return vaultOperations.opsForTransit()
            .encrypt(KEY_NAME, Plaintext.of(rawCardNumber))
            .getCiphertext();
    }

    public String decryptCardNumber(String ciphertext) {
        return vaultOperations.opsForTransit()
            .decrypt(KEY_NAME, ciphertext)
            .asString();
    }
}
```

### Example 4: Anti-Pattern vs. Production Fix (Baking Secrets into Docker Images)
```dockerfile
# ANTI-PATTERN: Passing secrets via Docker build args or baking them into images
ARG DB_PASSWORD=Secret123!
ENV DATABASE_PASSWORD=$DB_PASSWORD
# CATASTROPHIC FLAW:
# Anyone running `docker history <image>` or inspecting container layers sees the plaintext password!

# PRODUCTION FIX: Inject secrets at runtime via Kubernetes ExternalSecret or AWS Secrets Manager
# The container image remains 100% generic, immutable, and secret-free!
```

---

## 6. Best Practices & Production Guidelines

1. **Use External Secrets Operator (ESO) in Kubernetes**: Decouple Spring Boot from vendor-specific secret SDKs. Let ESO sync AWS/Vault secrets into native Kubernetes `Secret` objects and inject them via standard environment variables (`valueFrom.secretKeyRef`).
2. **Never Check Master Keys into Code**: If using envelope encryption or Jasypt, inject the master encryption key (`JASYPT_ENCRYPTOR_PASSWORD`) strictly via environment variables or secret vaults at runtime.
3. **Enable Automated Secret Rotation**: Standardize on a 30-to-90 day automated secret rotation policy. Ensure application datasources and HTTP clients gracefully handle credential renewal without downtime.
4. **Enforce Least Privilege Policies in Vault**: Define granular Vault ACL policies. An `order-service` role should only read `/secret/order-service/*` and have zero access to `/secret/billing-service/*`.
5. **Encrypt Kubernetes etcd at Rest**: Ensure your Kubernetes cluster has KMS encryption enabled for `Secret` resources in etcd to prevent direct etcd database reads from exposing credentials.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Baking secrets into Docker images | `docker history` or container inspection reveals secrets in plaintext. | Inject secrets at runtime via environment variables or ESO. |
| Forgetting Vault lease renewal | When the 1-hour lease expires, Vault revokes the database user; all subsequent queries fail with `FATAL: role does not exist`. | Enable Spring Cloud Vault lease renewal daemon. |
| Storing master encryption key in Git | Defeats the purpose of encryption; compromised repo compromises all secrets. | Inject master key via secure cloud secret manager. |
| Missing network timeouts on Vault calls | If Vault is temporarily unreachable on startup, application boot hangs indefinitely. | Set `spring.cloud.vault.connection-timeout: 3000`. |
| Unsanitized Actuator `/env` | Inspecting `/actuator/env` reveals unmasked secrets if custom property keys were used. | Register a custom `SanitizingFunction` matching proprietary secret keys. |
| Hardcoding Vault Token in config | Static token can expire or be compromised. | Use `KUBERNETES` or `AWS_IAM` dynamic authentication in Vault. |

---

## 8. Performance & Security Notes

### Overhead of Vault Secret Resolution
- Querying HashiCorp Vault or AWS Secrets Manager during application startup adds 50ms to 300ms to the boot sequence.
- During runtime, secrets are cached in memory; zero ongoing latency is incurred for standard database or API operations.

### Auditing & Zero-Trust
- Every secret read in HashiCorp Vault generates an immutable audit log entry detailing the exact pod IP, Kubernetes ServiceAccount, timestamp, and requested secret path.
- In SOC2 and PCI audits, centralized secret access logs are mandatory for proving compliance.

---

## 9. Comparison / When to Use What

| Secret Solution | Management Model | Dynamic Credentials? | Ideal Environment |
|---|---|---|---|
| **External Secrets Operator (ESO)**| Kubernetes Native Operator | No (Syncs external secrets) | **Kubernetes multi-cloud environments** |
| **HashiCorp Vault** | Dedicated Enterprise Server | **Yes (Just-in-time creation)**| Enterprise hybrid-cloud with strict compliance |
| **AWS Secrets Manager** | Fully Managed AWS Native | Yes (via Lambda rotators) | AWS-centric cloud microservices |
| **Spring Cloud Vault** | Embedded Java SDK | Yes | Spring Boot apps running with direct Vault access |
| **Native Kubernetes Secrets** | In-Cluster resource | No (Static) | Basic environments with etcd encryption active |

---

## 10. Interview Questions & Answers

### Q1: What are Dynamic Secrets in HashiCorp Vault and how do they benefit Spring Boot?
<details>
<summary>Click to view answer</summary>

Dynamic secrets are credentials generated on-demand by Vault that do not exist until requested. For example, when Spring Boot boots, Vault connects to PostgreSQL, creates a temporary database user with an auto-generated password, assigns permissions, and gives it a 1-hour Time-To-Live (TTL). When the application shuts down or the lease expires, Vault automatically deletes the user. There are no static passwords to steal, rotate, or leak.
</details>

### Q2: What is the External Secrets Operator (ESO) and why is it popular in Kubernetes?
<details>
<summary>Click to view answer</summary>

The External Secrets Operator is a Kubernetes operator that integrates with external secret management systems (HashiCorp Vault, AWS Secrets Manager, Azure Key Vault, Google Secret Manager). It securely fetches secrets from the external store and generates standard Kubernetes `Secret` objects inside the cluster.  
Benefit: The Spring Boot application code requires **zero third-party SDK dependencies or vendor lock-in**; it simply reads standard Kubernetes environment variables or mounted files.
</details>

### Q3: How does Spring Cloud Vault authenticate using Kubernetes ServiceAccount tokens?
<details>
<summary>Click to view answer</summary>

1. Spring Boot reads its auto-mounted Kubernetes ServiceAccount JWT from `/var/run/secrets/kubernetes.io/serviceaccount/token`.
2. It sends this JWT to Vault's `/v1/auth/kubernetes/login` endpoint along with its configured role name.
3. Vault validates the JWT with the Kubernetes API Server using the `TokenReview` API.
4. If valid, Vault issues a scoped Vault token, allowing the application to read its authorized secrets.
</details>

### Q4: What does Spring Cloud's `@RefreshScope` do and how is it triggered?
<details>
<summary>Click to view answer</summary>

`@RefreshScope` is a Spring Cloud scope. Beans annotated with `@RefreshScope` are lazily managed by a proxy cache. When an administrator triggers an HTTP POST to `/actuator/refresh` (or when a Spring Cloud Bus event fires), Spring reloads the environment properties and clears the bean cache. The next time the bean is accessed, it is re-instantiated with the new property values without restarting the JVM.
</details>

### Q5: Why is standard base64 encoding in Kubernetes Secrets NOT encryption?
<details>
<summary>Click to view answer</summary>

Base64 is a reversible encoding scheme designed for transmitting binary data across text-based protocols; it provides zero confidentiality or security. Anyone with read access to the Kubernetes namespace can run `echo "<encoded>" | base64 -d` to decode the password in milliseconds. Real security requires encrypting secrets at rest in etcd using KMS keys and restricting RBAC access.
</details>

### Q6: What is Transit Encryption in HashiCorp Vault?
<details>
<summary>Click to view answer</summary>

Transit Encryption is "Cryptography as a Service." The application sends plaintext data (e.g. credit card number) to Vault, and Vault encrypts it using an internal key and returns the ciphertext. The private encryption key never leaves the secure memory of the Vault server. This eliminates the risk of private cryptographic keys leaking from application memory dumps.
</details>

### Q7: How does Spring Boot 3's `spring.config.import` simplify secret loading?
<details>
<summary>Click to view answer</summary>

In older Spring Boot versions, external secrets required a dedicated `bootstrap.yml` file and separate bootstrap application context. In Spring Boot 3, bootstrap is eliminated; external configuration trees are declared directly in `application.yml` via `spring.config.import: aws-secretsmanager:/path` or `vault://path`, loading external secrets as standard first-class property sources during application boot.
</details>

### Q8: What happens if a HashiCorp Vault server goes down after Spring Boot has started?
<details>
<summary>Click to view answer</summary>

Once Spring Boot initializes, secrets are cached in the application's in-memory `Environment`. A temporary Vault outage will not affect active database queries or user transactions. However, if dynamic secrets are used, Spring Cloud Vault must be able to reach Vault before the lease renewal deadline expires to prevent Vault from revoking the credentials.
</details>

---

## 11. Practice Exercises

### Exercise 1: Configure AWS Secrets Manager Import in YAML (Easy)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Write an `application.yml` configuration to import secrets from AWS Secrets Manager for `/apps/order-service/prod` in region `eu-central-1`.

**Solution**:
```yaml
spring:
  config:
    import:
      - aws-secretsmanager:/apps/order-service/prod
  cloud:
    aws:
      region:
        static: eu-central-1
```
</details>

### Exercise 2: ExternalSecret Kubernetes Manifest for Vault Sync (Medium)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Write an External Secrets Operator `ExternalSecret` manifest that syncs `secret/data/billing/db` from a `ClusterSecretStore` named `vault-backend` into a Kubernetes Secret `billing-db-credentials`.

**Solution**:
```yaml
apiVersion: external-secrets.io/v1beta1
kind: ExternalSecret
metadata:
  name: billing-db-external-secret
spec:
  refreshInterval: "30m"
  secretStoreRef:
    name: vault-backend
    kind: ClusterSecretStore
  target:
    name: billing-db-credentials
    creationPolicy: Owner
  data:
    - secretKey: username
      remoteRef:
        key: secret/data/billing/db
        property: username
    - secretKey: password
      remoteRef:
        key: secret/data/billing/db
        property: password
```
</details>

### Exercise 3: Dynamic Property Reloader with Actuator Event Listener (Hard)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Implement a Spring `@Component` that listens to `RefreshScopeRefreshedEvent` and logs whenever encryption keys or gateway credentials are dynamically rotated at runtime.

**Solution**:
```java
package com.example.production.secrets;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.cloud.context.scope.refresh.RefreshScopeRefreshedEvent;
import org.springframework.context.event.EventListener;
import org.springframework.stereotype.Component;

import java.time.Instant;

@Component
public class SecretRotationAuditor {

    private static final Logger log = LoggerFactory.getLogger(SecretRotationAuditor.class);

    @EventListener(RefreshScopeRefreshedEvent.class)
    public void onSecretsRefreshed(RefreshScopeRefreshedEvent event) {
        log.warn("SECURITY AUDIT: Application secrets refreshed at {} via {}. All @RefreshScope beans re-instantiated.",
            Instant.now(), event.getName());
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------+-------------------------------------------------------------+
| Solution / Principle                        | Description & Best Practice                                 |
+---------------------------------------------+-------------------------------------------------------------+
| External Secrets Operator (ESO)             | Recommended K8s pattern: syncs cloud secrets into K8s Secrets|
| HashiCorp Vault                             | Dynamic JIT credentials with automatic lease revocation     |
| spring.config.import                        | Standard Boot 3 mechanism to import external secret trees   |
| @RefreshScope                               | Re-instantiates beans when secrets rotate without JVM reboot|
| Zero Static Credentials                     | Eliminate hardcoded passwords and long-lived cloud keys     |
| Base64 is NOT Encryption                    | Standard K8s secrets must be encrypted at rest in etcd      |
| Transit Encryption                          | Cryptography-as-a-service; private keys stay inside Vault   |
+---------------------------------------------+-------------------------------------------------------------+
```

---

## 13. Further Reading
- HashiCorp Vault Official Documentation: Dynamic Database Secrets
- External Secrets Operator (ESO) Official Documentation
- Spring Cloud AWS: Secrets Manager Integration Reference
- OWASP Secrets Management Cheat Sheet