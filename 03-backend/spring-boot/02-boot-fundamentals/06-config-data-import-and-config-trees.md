# Config Data Import & Kubernetes ConfigTrees

> Master modern external configuration in cloud environments: the `spring.config.import` SPI, resolving Kubernetes ConfigMaps and Secrets as `configtree:` volume mounts, handling optional resources, and writing custom `ConfigDataLoader` extensions.

| Metadata | Value |
| :--- | :--- |
| **Level** | Advanced |
| **Prerequisites** | [Externalized Configuration & Property Hierarchy](file:///03-backend/spring-boot/02-boot-fundamentals/03-externalized-configuration-and-properties.md), [Profiles in Spring Boot](file:///03-backend/spring-boot/02-boot-fundamentals/05-profiles-in-boot.md) |
| **Time to Read** | 25 minutes |
| **Status** | Complete |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.0.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | OpenJDK 25.0.1, Spring Boot 3.4.0 |

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
In cloud-native Kubernetes environments, configuration parameters and credentials are rarely stored in traditional `.properties` or `.yaml` files. Instead, Kubernetes mounts **ConfigMaps** and **Secrets** as volume directories into the container's filesystem. Inside these directories, each file name represents a property key, and the file's raw content represents the property value. Historically, Java developers had to write custom file-reading scripts or install heavyweight external client libraries (like Spring Cloud Kubernetes) to read these volumes.

### Historical Evolution
- **Pre-Boot 2.4**: External configurations had to be defined in fixed classpath or filesystem locations (`application.properties`).
- **Spring Boot 2.4**: Introduced the unified **Config Data API** and `spring.config.import`, adding first-class support for `configtree:` volume mounts and `optional:` prefixes.
- **Spring Boot 3.x & 3.4+**: Fully consolidated Kubernetes ConfigTrees into the core runtime without requiring Spring Cloud dependencies.

### Real-World Relevance
`spring.config.import` is the official modern standard for importing external configurations in production, enabling zero-code integration with Kubernetes volumes, HashiCorp Vault, AWS Secrets Manager, and shared network drives.

---

## 2. Core Concepts

### 1. The `spring.config.import` Property
Directs Spring Boot to load additional configuration files or trees during bootstrap:
```yaml
spring:
  config:
    import:
      - "optional:file:/etc/config/app-settings.yaml"
      - "optional:configtree:/etc/secrets/"
```

### 2. The `optional:` Prefix
By default, if an imported location does not exist, Spring Boot aborts startup with a `ConfigDataLocationNotFoundException`. Prepending `optional:` instructs Spring Boot to silently ignore missing files or directories—crucial for local development where Kubernetes volume mounts do not exist on developer laptops.

### 3. The `configtree:` Location Protocol
Tells Spring Boot to treat a filesystem directory as a hierarchical property tree:
- Directory structure: `/etc/secrets/db/password`
- Spring Boot converts this into property: `db.password` containing the text inside the file.
- Nested folders are automatically converted into dot-separated keys.

### 4. ConfigData SPI Extensions
The engine is extensible via two core SPI interfaces:
- `ConfigDataLocationResolver`: Resolves a string URI into a `ConfigDataLocation`.
- `ConfigDataLoader`: Reads data from the resolved location into `ConfigData` properties.

---

## 3. How It Works Internally

### Kubernetes Volume to Spring Boot Property Mapping

```
KUBERNETES VOLUME MOUNT: `/etc/secrets/`
├── database/
│   ├── username        (Contains: "postgres_admin")
│   └── password        (Contains: "vault_secret_9988!")
└── stripe/
    └── api-key         (Contains: "sk_live_abc123")

                               |
              Configured in application.yaml:
              `spring.config.import=configtree:/etc/secrets/`
                               |
                               v
+-------------------------------------------------------------------------+
|             ConfigTreeConfigDataLoader                                  |
|  - Recursively walks directory `/etc/secrets/`                          |
|  - Translates paths into property keys:                                 |
|    * `/database/username` -> `database.username = "postgres_admin"`     |
|    * `/database/password` -> `database.password = "vault_secret_9988!"`|
|    * `/stripe/api-key`    -> `stripe.api-key = "sk_live_abc123"`        |
+------------------------------------+------------------------------------+
                                     |
                                     v
+-------------------------------------------------------------------------+
|                  Injected into Spring Environment                       |
|  - Available via @Value("${database.password}")                         |
|  - Available via @ConfigurationProperties                               |
+-------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### Standard YAML Configuration with Multiple Imports
```yaml
spring:
  application:
    name: order-service
  config:
    import:
      # Load optional local developer overrides if present
      - "optional:file:./local-overrides.yaml"
      # Load Kubernetes ConfigMap volume
      - "optional:configtree:/etc/config/order-service/"
      # Load Kubernetes Secret volume
      - "optional:configtree:/etc/secrets/order-service/"
```

### Kubernetes Pod Volume Mount Specification
```yaml
apiVersion: v1
kind: Pod
metadata:
  name: order-service
spec:
  containers:
    - name: app
      image: order-service:latest
      volumeMounts:
        - name: app-secrets
          mountPath: /etc/secrets/order-service
          readOnly: true
  volumes:
    - name: app-secrets
      secret:
        secretName: order-service-credentials
```

---

## 5. Practical Examples

### Example 1: Reading Kubernetes ConfigTrees via `@ConfigurationProperties`
```java
package com.example.demo.config;

import org.springframework.boot.context.properties.ConfigurationProperties;

@ConfigurationProperties(prefix = "database")
public record DatabaseCredentials(
    // Mapped automatically from /etc/secrets/order-service/database/username
    String username,
    // Mapped automatically from /etc/secrets/order-service/database/password
    String password
) {}
```
```java
package com.example.demo.service;

import com.example.demo.config.DatabaseCredentials;
import org.springframework.boot.CommandLineRunner;
import org.springframework.stereotype.Component;

@Component
public class DatabaseCredentialsVerifier implements CommandLineRunner {

    private final DatabaseCredentials credentials;

    public DatabaseCredentialsVerifier(DatabaseCredentials credentials) {
        this.credentials = credentials;
    }

    @Override
    public void run(String... args) {
        System.out.println("ConfigTree mapped username: " + credentials.username());
        System.out.println("ConfigTree mapped password length: " + 
            (credentials.password() != null ? credentials.password().length() : 0));
    }
}
```

### Example 2: Graceful Optional Imports for Seamless Local & Cloud Execution
```yaml
# application.yaml
spring:
  application:
    name: billing-service
  config:
    import:
      # In Cloud: Loads mounted volume.
      # On Local Laptop: Volume doesn't exist, but 'optional:' prevents startup crash!
      - "optional:configtree:/var/run/secrets/kubernetes.io/serviceaccount/"
      - "optional:file:/etc/app/billing-overrides.properties"
```

### Example 3: Importing External Cloud Storage Files
```yaml
# application.yaml
spring:
  config:
    import:
      # Loads externalized shared company configuration from mounted network volume
      - "optional:file:/mnt/shared-configs/company-global-defaults.yaml"
```

### Example 4: Missing `optional:` Anti-Pattern & Remediation
```yaml
# ANTI-PATTERN: Forgetting 'optional:' prefix on external files
spring:
  config:
    import:
      # FLAW: Works in production Kubernetes, but crashes immediately on developer laptops!
      # org.springframework.boot.context.config.ConfigDataLocationNotFoundException: 
      # Config data location 'configtree:/etc/secrets/' does not exist!
      - "configtree:/etc/secrets/"

# REMEDIATION: Always prepend 'optional:' for external infrastructure paths
spring:
  config:
    import:
      # SAFE: Silently ignored if directory or file does not exist locally
      - "optional:configtree:/etc/secrets/"
```

### Example 5: Custom `ConfigDataLoader` for Proprietary Encrypted Store
```java
package com.example.demo.customconfig;

import org.springframework.boot.context.config.*;
import org.springframework.boot.env.OriginTrackedMapPropertySource;
import org.springframework.core.env.MapPropertySource;
import org.springframework.core.io.Resource;

import java.io.IOException;
import java.util.Map;

// Custom Location representing proprietary custom vault URI: "vault:secret-key"
public record VaultConfigDataLocation(String secretKey) implements ConfigDataLocation {}

// Loader reading and decrypting data
public class VaultConfigDataLoader implements ConfigDataLoader<VaultConfigDataLocation> {

    @Override
    public ConfigData load(ConfigDataLoaderContext context, VaultConfigDataLocation location) throws IOException {
        System.out.println("Fetching secret from proprietary vault for key: " + location.secretKey());
        
        // Simulates remote encrypted vault call
        Map<String, Object> decryptedProperties = Map.of(
            "app.security.token", "DECRYPTED-TOKEN-XYZ",
            "app.security.ttl", "3600s"
        );

        MapPropertySource propertySource = new MapPropertySource("vaultSource", decryptedProperties);
        return new ConfigData(java.util.List.of(propertySource));
    }
}
```

---

## 6. Best Practices

1. **Always Use `optional:` on External Infrastructure Paths**: Prefix all `file:` and `configtree:` imports with `optional:`. This guarantees your repository can boot locally on developer machines without requiring mock filesystem directories.
2. **Standardize Kubernetes Secret Mount Paths**: Establish a company-wide convention for secret volume mounts (e.g., `/etc/secrets/{service-name}/`) and declare it in base `application.yaml`.
3. **Use Subdirectories in ConfigTrees for Namespacing**: Organize Kubernetes ConfigMap keys into subfolders (e.g., `/etc/config/database/url`) so Spring Boot cleanly namespaces them into dot-notation keys (`database.url`).
4. **Trim Whitespace in ConfigTree Files**: Kubernetes ConfigMap files sometimes include trailing newlines. Spring Boot's `ConfigTreeConfigDataLoader` automatically trims trailing newlines by default.
5. **Combine ConfigTrees with `@ConfigurationProperties` Records**: Bind imported config tree hierarchies directly to immutable records rather than reading individual keys via `@Value`.
6. **Never Import Unvetted Remote URLs**: Do not configure `spring.config.import` with untrusted public HTTP URLs (`https://`), as an outage or MITM attack will compromise or halt application startup.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Omitting `optional:` prefix** | App crashes on local startup because `/etc/secrets` only exists inside Kubernetes. | Prepend `optional:configtree:...`. |
| **Trailing slash missing on `configtree:`** | `configtree:/etc/secrets` without a trailing slash fails path resolution on some OS versions. | Always include the trailing slash: `configtree:/etc/secrets/`. |
| **Binary files in a ConfigTree directory** | ConfigTree expects text files; encountering binary files (e.g., `.keystore`, `.png`) can cause UTF-8 parsing errors. | Mount binary secrets to a separate directory outside the config tree. |
| **Hidden dot-files in Kubernetes volume** | Kubernetes creates hidden `..data` symlink directories in mounted secrets, which can create unwanted property keys if improperly configured. | Spring Boot's built-in `ConfigTreeConfigDataLoader` automatically ignores files and directories starting with `.`. |
| **Expectation of Dynamic Auto-Reload** | Updating a Kubernetes Secret updates the file on disk, but Spring Boot does NOT reload properties in memory without Actuator refresh. | Trigger Actuator `/actuator/refresh` or use Kubernetes pod restarts on secret rotation. |
| **Syntax typo in import statement** | Misspelling `spring.config.import` as `spring.config.imports` fails silently without importing. | Verify exact key name: `spring.config.import` (singular). |

---

## 8. Performance & Security Notes

### Zero Overhead of ConfigTrees
`ConfigTreeConfigDataLoader` reads directory trees synchronously once during bootstrap. Once loaded into the Spring `Environment`, file handles are closed, resulting in zero background CPU or I/O overhead during request processing.

### Secret Rotation Security
When Kubernetes rotates a Secret:
- The projected filesystem file is updated by the kubelet within 60 seconds.
- By default, Spring Boot caches properties in memory. To apply rotated credentials, either restart the pod (recommended Kubernetes pattern) or expose Actuator `/actuator/refresh` with Spring Cloud Context.

---

## 9. Comparison / When to Use What

| Mechanism | Storage Format | Cloud Native? | Requires Extra Starters? | Best For |
| :--- | :--- | :---: | :---: | :--- |
| **`configtree:`** | Directory with 1 file per key | **Yes (K8s native)** | **No (Built into Boot)** | Kubernetes ConfigMaps & Secrets |
| **`file:`** | Single `.yaml` or `.properties` file| Yes | No | Mounted external YAML config files |
| **`SPRING_APPLICATION_JSON`** | Single JSON string | Yes | No | Docker CLI & CI/CD pipeline flags |
| **Spring Cloud Config / Vault** | Centralized REST Server / Vault | Yes | Yes (`spring-cloud-starter`)| Centralized multi-repo configuration |

---

## 10. Interview Questions

<details>
<summary><strong>Q1: What is `spring.config.import` and when was it introduced?</strong></summary>

**Answer:**  
Introduced in **Spring Boot 2.4**, `spring.config.import` is a configuration property that allows applications to import external configuration sources (such as additional YAML files, property files, directory trees, or cloud configuration servers) into the Spring `Environment` during startup.
</details>

<details>
<summary><strong>Q2: How does the `configtree:` protocol work in Spring Boot?</strong></summary>

**Answer:**  
`configtree:` instructs Spring Boot to inspect a filesystem directory and convert its file hierarchy into property key-value pairs:
- The path relative to the root directory becomes the dot-separated property key.
- The text content inside each file becomes the property value.
This provides seamless native integration with Kubernetes ConfigMaps and Secrets mounted as volume directories without requiring any third-party client libraries.
</details>

<details>
<summary><strong>Q3: What does the `optional:` prefix do in `spring.config.import`?</strong></summary>

**Answer:**  
By default, if an imported configuration file or directory does not exist, Spring Boot throws a `ConfigDataLocationNotFoundException` and halts startup. Prepending `optional:` (e.g., `optional:configtree:/etc/secrets/`) instructs Spring Boot to skip the location gracefully if it is missing, enabling seamless execution across both local development and cloud Kubernetes environments.
</details>

<details>
<summary><strong>Q4: How does Spring Boot handle nested directories inside a `configtree:`?</strong></summary>

**Answer:**  
Spring Boot converts directory paths into dot-separated nested property keys. For example, a file located at `/etc/secrets/database/primary/password` is automatically registered in the Spring `Environment` as the property `database.primary.password`.
</details>

<details>
<summary><strong>Q5: How does Spring Boot ignore hidden Kubernetes symlinks like `..data` in secret volumes?</strong></summary>

**Answer:**  
Kubernetes uses symlinks (like `..data` and `..2026_05_12_...`) to atomically update secret volume mounts. Spring Boot's `ConfigTreeConfigDataLoader` explicitly ignores any file or directory whose name begins with a dot (`.`), automatically filtering out Kubernetes administrative symlinks.
</details>

<details>
<summary><strong>Q6: Can you import multiple files in `spring.config.import`? How is precedence handled?</strong></summary>

**Answer:**  
Yes, `spring.config.import` accepts a list of locations. Locations are processed in the order declared: properties defined in later imports take precedence and override properties defined in earlier imports.
</details>

<details>
<summary><strong>Q7: Does updating a file in a Kubernetes ConfigTree automatically reload the property in a running Spring Boot app?</strong></summary>

**Answer:**  
**No.** Spring Boot reads configuration files and config trees once during application startup and caches them in memory. To pick up updated files without restarting the container, you must trigger a context refresh (e.g., via Spring Cloud's `/actuator/refresh` or custom file watchers). The standard Kubernetes practice is to allow Kubernetes to trigger a rolling pod restart when configurations change.
</details>

<details>
<summary><strong>Q8: What SPI interfaces do you implement to build a custom `spring.config.import` protocol (e.g., `consul:` or `vault:`)?</strong></summary>

**Answer:**  
1. `ConfigDataLocationResolver`: Parses the location string and determines if the URI is valid.
2. `ConfigDataLoader`: Connects to the target store, reads the data, and returns a `ConfigData` object wrapping one or more `PropertySource` instances.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Create a Simulated ConfigTree on Disk (Easy)</strong></summary>

**Task:**  
Create a temporary directory `target/test-configtree/app/`. Create a text file `secret-key` containing `my-super-secret-123`. Configure `spring.config.import=configtree:target/test-configtree/`. Assert that `@Value("${app.secret-key}")` injects `my-super-secret-123`.
</details>

<details>
<summary><strong>Exercise 2: Verify `optional:` Prefix Resilience (Easy)</strong></summary>

**Task:**  
Configure `spring.config.import=optional:file:/non-existent-path/file.yaml`. Boot the application and verify that it boots successfully without throwing `ConfigDataLocationNotFoundException`.
</details>

<details>
<summary><strong>Exercise 3: Nested ConfigTree Directory Mapping (Medium)</strong></summary>

**Task:**  
Create a nested directory structure `db/pool/max-size` with content `50`. Bind it using `@ConfigurationProperties(prefix = "db.pool")` and verify that `pool.maxSize()` returns `50`.
</details>

<details>
<summary><strong>Exercise 4: Test Precedence Between Multiple Imports (Medium)</strong></summary>

**Task:**  
Configure two imports in `application.yaml`: `file:./config1.yaml` and `file:./config2.yaml`. Define `app.message=One` in file 1 and `app.message=Two` in file 2. Verify that `app.message` resolves to `Two`.
</details>

<details>
<summary><strong>Exercise 5: Build a Custom In-Memory ConfigDataLoader (Hard)</strong></summary>

**Task:**  
Implement `ConfigDataLocationResolver` and `ConfigDataLoader` for a custom prefix `mockvault:my-secret`. Register them via `META-INF/spring.factories` and verify that Spring Boot successfully imports properties from `mockvault:`.
</details>

<details>
<summary><strong>Exercise 6: Kubernetes Pod ConfigTree Integration Test (Hard)</strong></summary>

**Task:**  
Write a Testcontainers integration test that mounts a host directory as a volume to a Spring Boot container at `/etc/secrets/`. Verify the containerized application successfully boots and reads credentials from the volume.
</details>

---

## 12. Summary / Cheat Sheet

| Prefix / Protocol | Syntax Example | Meaning |
| :--- | :--- | :--- |
| **`configtree:`** | `configtree:/etc/secrets/` | Maps directory files into dot-separated keys |
| **`optional:`** | `optional:configtree:/etc/config/` | Silently ignores location if non-existent |
| **`file:`** | `file:/etc/app/config.yaml` | Loads external filesystem configuration file |
| **`classpath:`** | `classpath:custom.yaml` | Loads file from project classpath |
| **Trailing Slash** | Must include `/` on directories | `configtree:/dir/` (Mandatory for directory resolution) |
| **Hidden Files** | Files starting with `.` ignored | Filters out Kubernetes internal `.data` symlinks |

---

## 13. Further Reading
- [Official Spring Boot Reference: Config Data Imports](https://docs.spring.io/spring-boot/docs/current/reference/html/features.html#features.external-config.files.configtree)
- [Spring Boot 2.4 Config Data Processing Overhaul](https://spring.io/blog/2020/08/14/config-file-processing-in-spring-boot-2-4)
- [Kubernetes Secrets as Volume Mounts](https://kubernetes.io/docs/concepts/configuration/secret/#using-secrets-as-files-from-a-pod)
