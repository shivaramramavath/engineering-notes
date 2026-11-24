# Password Encoding & UserDetails: BCrypt, Argon2, DelegatingPasswordEncoder & Custom Principals

| Metadata | Details |
| :--- | :--- |
| **Level** | Intermediate |
| **Prerequisites** | `01-spring-security-architecture.md`, `02-authentication-fundamentals.md`, Cryptographic hashing concepts |
| **Estimated Reading Time** | 28 minutes |
| **Status** | Active |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework** | 6.2.x / 7.0.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven / Gradle |
| **Tested With** | Spring Security 6.4.x / 7.x, BouncyCastle (Argon2), PostgreSQL 16 |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
   - [5.1 Modern PasswordEncoderFactories & DelegatingPasswordEncoder](#51-modern-passwordencoderfactories--delegatingpasswordencoder)
   - [5.2 High-Security Argon2id Password Encoder Configuration](#52-high-security-argon2id-password-encoder-configuration)
   - [5.3 Custom Immutable Principal Record Implementing UserDetails](#53-custom-immutable-principal-record-implementing-userdetails)
   - [5.4 Anti-Pattern: Using Fast Hashes (MD5/SHA-256) or Plaintext for Passwords](#54-anti-pattern-using-fast-hashes-md5sha-256-or-plaintext-for-passwords)
   - [5.5 Transparent Password Hash Upgrades via UserDetailsPasswordService](#55-transparent-password-hash-upgrades-via-userdetailspasswordservice)
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
Storing user passwords in plaintext, reversibly encrypted, or hashed using fast cryptographic algorithms (MD5, SHA-1, SHA-256) is fatal to application security. Modern GPUs and specialized ASIC clusters can compute billions of SHA-256 hashes per second, allowing attackers who breach a database to crack passwords in seconds using rainbow tables and dictionary attacks. Password storage algorithms must be deliberately **slow, salted, and computationally expensive**.

### Historical Evolution
Spring Security introduced the `PasswordEncoder` interface to enforce one-way adaptive password hashing. While **BCrypt** became the standard adaptive hashing algorithm for two decades, modern hardware advances prompted the Password Hashing Competition (PHC) to select **Argon2** (specifically Argon2id) as the gold standard. Argon2 is both CPU-hard and memory-hard, rendering GPU and ASIC brute-force attacks economically and physically infeasible. To manage migrations across algorithms, Spring Security introduced `DelegatingPasswordEncoder`, which embeds the algorithm ID into the hash string (e.g. `{bcrypt}$2a$...`).

### Real-World Relevance
Proper password hashing is mandated by security standards (NIST SP 800-63B, OWASP, PCI-DSS). Implementing secure password encoding paired with a custom `UserDetails` domain model ensures that credentials cannot be cracked even in the event of a total database dump.

---

## 2. Core Concepts

### The PasswordEncoder Contract
```java
public interface PasswordEncoder {
    String encode(CharSequence rawPassword);
    boolean matches(CharSequence rawPassword, String encodedPassword);
    default boolean upgradeEncoding(String encodedPassword) { return false; }
}
```
- `encode()`: Performs salting and hashing, returning a formatted hash string.
- `matches()`: Hashes the raw password with the salt extracted from the encoded string and performs a constant-time comparison.
- `upgradeEncoding()`: Checks if the hash was created with an outdated algorithm or insufficient work factor.

### Modern Password Hashing Algorithms
1. **Argon2id**: The state-of-the-art winner of the Password Hashing Competition. Configurable by memory cost, time cost, and parallelism. Highly resistant to GPU/FPGA/ASIC cracking.
2. **BCrypt**: Adaptive hash based on the Blowfish cipher. Highly reliable and ubiquitous; configurable by logarithmic work factor (e.g. 10–14 rounds).
3. **SCrypt**: Memory-hard predecessor to Argon2.
4. **PBKDF2**: NIST-approved HMAC-based key derivation function.

### DelegatingPasswordEncoder Format
Stores the algorithm identifier as a prefix enclosed in curly braces:
$$\text{\{id\}hash}$$
Examples:
- `{bcrypt}$2a$12$e8n...`
- `{argon2}$argon2id$v=19$m=16384,t=2,p=1$...`
- `{pbkdf2}5d41402a...`

### UserDetails & UserDetailsService
- **`UserDetails`**: Core interface representing the user account state: username, password hash, authorities, and account status flags (`isAccountNonExpired`, `isAccountNonLocked`, `isCredentialsNonExpired`, `isEnabled`).
- **`UserDetailsService`**: Simple data access strategy interface with a single method: `UserDetails loadUserByUsername(String username)`.

---

## 3. How It Works Internally

```
+------------------------------------------------------------------------------------+
|                         DelegatingPasswordEncoder Matching Flow                    |
+------------------------------------------------------------------------------------+
|                                                                                    |
|  rawPassword: "MyPassword123!"                                                     |
|  encodedPassword: "{bcrypt}$2a$12$e8nZtH8F9X...salt...hash"                       |
|         |                                                                          |
|         v                                                                          |
|  DelegatingPasswordEncoder.matches()                                               |
|         |                                                                          |
|         +---> Extracts Prefix ID: "bcrypt"                                         |
|         |                                                                          |
|         +---> Resolves PasswordEncoder instance from Map<String, PasswordEncoder>   |
|         |     (Maps "bcrypt" -> BCryptPasswordEncoder instance)                    |
|         |                                                                          |
|         v                                                                          |
|  BCryptPasswordEncoder.matches()                                                   |
|         |                                                                          |
|         +---> Extracts 128-bit Salt and Cost Factor (12) from hash string          |
|         +---> Computes BCrypt hash of rawPassword using extracted salt             |
|         +---> Compares newly computed hash with stored hash in CONSTANT TIME       |
|               (Prevents side-channel timing attacks!)                              |
|         |                                                                          |
|         v                                                                          |
|  Returns: TRUE (Match) or FALSE (Mismatch)                                         |
|                                                                                    |
+------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### Standard Delegating Password Encoder Bean
```java
@Configuration
public class PasswordConfig {

    @Bean
    public PasswordEncoder passwordEncoder() {
        // Defaults to BCrypt, but understands {argon2}, {pbkdf2}, etc.
        return PasswordEncoderFactories.createDelegatingPasswordEncoder();
    }
}
```

---

## 5. Practical Examples

### 5.1 Modern PasswordEncoderFactories & DelegatingPasswordEncoder
```java
package com.example.security.config;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.security.crypto.argon2.Argon2PasswordEncoder;
import org.springframework.security.crypto.bcrypt.BCryptPasswordEncoder;
import org.springframework.security.crypto.password.DelegatingPasswordEncoder;
import org.springframework.security.crypto.password.PasswordEncoder;

import java.util.HashMap;
import java.util.Map;

@Configuration
public class CustomPasswordEncodingConfig {

    @Bean
    public PasswordEncoder passwordEncoder() {
        String idForEncode = "argon2";
        Map<String, PasswordEncoder> encoders = new HashMap<>();

        // 1. Argon2id (Modern Primary Encoder)
        encoders.put("argon2", Argon2PasswordEncoder.defaultsForSpringSecurity_v5_8());

        // 2. BCrypt with strength factor 12 (Industry Standard Fallback)
        encoders.put("bcrypt", new BCryptPasswordEncoder(12));

        DelegatingPasswordEncoder delegatingPasswordEncoder = 
            new DelegatingPasswordEncoder(idForEncode, encoders);

        // Fallback for un-prefixed legacy hashes
        delegatingPasswordEncoder.setDefaultPasswordEncoderForMatches(new BCryptPasswordEncoder(10));

        return delegatingPasswordEncoder;
    }
}
```

### 5.2 High-Security Argon2id Password Encoder Configuration
```java
package com.example.security.config;

import org.springframework.security.crypto.argon2.Argon2PasswordEncoder;

public class Argon2TuningFactory {

    /**
     * OWASP Recommended Argon2id Parameters for Enterprise Systems:
     * - saltLength: 16 bytes
     * - hashLength: 32 bytes
     * - parallelism: 1 thread
     * - memory: 19456 KiB (19 MiB RAM per hash)
     * - iterations: 2 passes
     */
    public static Argon2PasswordEncoder createEnterpriseArgon2() {
        return new Argon2PasswordEncoder(
            16,     // salt length in bytes
            32,     // hash length in bytes
            1,      // parallelism (threads)
            19456,  // memory cost in KiB (~19MB)
            2       // iterations
        );
    }
}
```

### 5.3 Custom Immutable Principal Record Implementing UserDetails
```java
package com.example.security.domain;

import org.springframework.security.core.GrantedAuthority;
import org.springframework.security.core.userdetails.UserDetails;

import java.util.Collection;
import java.util.Collections;
import java.util.Set;

/**
 * Modern Java 21 Record implementing Spring Security's UserDetails interface.
 * Provides thread-safety, immutability, and zero boilerplate!
 */
public record SecurityUserPrincipal(
    Long id,
    String username,
    String passwordHash,
    String email,
    boolean enabled,
    Set<GrantedAuthority> authorities
) implements UserDetails {

    @Override
    public Collection<? extends GrantedAuthority> getAuthorities() {
        return Collections.unmodifiableSet(authorities);
    }

    @Override
    public String getPassword() {
        return passwordHash;
    }

    @Override
    public String getUsername() {
        return username;
    }

    @Override
    public boolean isAccountNonExpired() { return true; }

    @Override
    public boolean isAccountNonLocked() { return true; }

    @Override
    public boolean isCredentialsNonExpired() { return true; }

    @Override
    public boolean isEnabled() {
        return enabled;
    }
}
```

```java
package com.example.security.service;

import com.example.security.domain.SecurityUserPrincipal;
import com.example.security.domain.UserEntity;
import com.example.security.repository.UserRepository;
import org.springframework.security.core.authority.SimpleGrantedAuthority;
import org.springframework.security.core.userdetails.UserDetails;
import org.springframework.security.core.userdetails.UserDetailsService;
import org.springframework.security.core.userdetails.UsernameNotFoundException;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.util.stream.Collectors;

@Service
public class CustomUserDetailsService implements UserDetailsService {

    private final UserRepository userRepository;

    public CustomUserDetailsService(UserRepository userRepository) {
        this.userRepository = userRepository;
    }

    @Override
    @Transactional(readOnly = true)
    public UserDetails loadUserByUsername(String username) throws UsernameNotFoundException {
        UserEntity user = userRepository.findByEmail(username)
            .orElseThrow(() -> new UsernameNotFoundException("User not found: " + username));

        var authorities = user.getRoles().stream()
            .map(role -> new SimpleGrantedAuthority("ROLE_" + role.getName()))
            .collect(Collectors.toSet());

        return new SecurityUserPrincipal(
            user.getId(),
            user.getEmail(),
            user.getPasswordHash(),
            user.getEmail(),
            user.isActive(),
            authorities
        );
    }
}
```

### 5.4 Anti-Pattern: Using Fast Hashes (MD5/SHA-256) or Plaintext for Passwords
```java
// =========================================================================
// ANTI-PATTERN: Using SHA-256 or MD5 for Password Storage
// =========================================================================
public class VulnerablePasswordService {
    // DISASTROUS SECURITY FLAW:
    // SHA-256 and MD5 are designed for high-speed file checksum verification, NOT passwords!
    // A single consumer GPU (e.g. RTX 4090) computes over 10 BILLION SHA-256 hashes per second!
    // Attackers can crack 8-character passwords from a leaked DB in under 5 minutes!
    public String hashPasswordBad(String raw) throws Exception {
        MessageDigest md = MessageDigest.getInstance("SHA-256");
        return Base64.getEncoder().encodeToString(md.digest(raw.getBytes()));
    }
}

// =========================================================================
// PRODUCTION FIX: Use an Adaptive, Slow Hash (Argon2id or BCrypt)
// =========================================================================
public class SecurePasswordService {
    private final PasswordEncoder encoder = Argon2PasswordEncoder.defaultsForSpringSecurity_v5_8();

    public String hashPasswordSecure(String raw) {
        // SAFE: Takes ~50ms per hash, requires 19MB of RAM per attempt!
        // Renders GPU brute-force cracking mathematically infeasible!
        return encoder.encode(raw);
    }
}
```

### 5.5 Transparent Password Hash Upgrades via UserDetailsPasswordService
```java
package com.example.security.service;

import com.example.security.domain.UserEntity;
import com.example.security.repository.UserRepository;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.security.core.userdetails.UserDetails;
import org.springframework.security.core.userdetails.UserDetailsPasswordService;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

/**
 * Automatically upgrades legacy password hashes (e.g. upgrading an old {bcrypt} cost 10
 * to a modern {argon2} hash) transparently whenever the user successfully logs in!
 */
@Service
public class PasswordUpgradeService implements UserDetailsPasswordService {

    private static final Logger log = LoggerFactory.getLogger(PasswordUpgradeService.class);
    private final UserRepository userRepository;

    public PasswordUpgradeService(UserRepository userRepository) {
        this.userRepository = userRepository;
    }

    @Override
    @Transactional
    public UserDetails updatePassword(UserDetails user, String newPasswordHash) {
        log.info("Upgrading password encoding format for user: {}", user.getUsername());

        UserEntity entity = userRepository.findByEmail(user.getUsername()).orElseThrow();
        entity.setPasswordHash(newPasswordHash);
        userRepository.save(entity);

        return user;
    }
}
```

---

## 6. Best Practices

1. **Always Use `DelegatingPasswordEncoder`**: Prefixing hashes with `{bcrypt}` or `{argon2}` allows seamless upgrades to future cryptographic algorithms without invalidating existing users.
2. **Tune Work Factors to Target 50–200ms per Hash**: A password hash should take approximately 100 milliseconds to compute on your server CPU. For BCrypt, a strength of `12` or `13` is typical.
3. **Implement `UserDetailsPasswordService`**: Enable automatic background password upgrades so that legacy hashes are silently re-hashed using the newest algorithm whenever a user enters their credentials.
4. **Never Write Custom Hashing Logic with `MessageDigest`**: Never roll your own salting, hashing, or byte comparisons. Use Spring Security's certified implementations.
5. **Use Constant-Time Comparison (`MessageDigest.isEqual`)**: When comparing hashes or signatures, always use constant-time comparisons to prevent side-channel timing attacks.
6. **Model `UserDetails` with Java Records**: Use immutable records to represent user principals, preventing accidental mutation of authorities or user IDs in downstream code.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
| :--- | :--- | :--- |
| **Using SHA-256 or MD5 for Passwords** | Fast algorithms can be brute-forced at billions of hashes per second using GPUs. | Use BCrypt or Argon2id. |
| **Encoding a Password Twice** | Calling `encoder.encode(encoder.encode(password))` corrupts the hash, making login impossible. | Call `encode()` only once when setting or updating passwords. |
| **Using Low Work Factor (e.g. BCrypt strength = 4)** | Low iteration count computes in microseconds, making offline dictionary attacks easy. | Use standard strength of 10 to 12. |
| **Returning Raw Entity as `UserDetails`** | Exposes mutable JPA entities to the security layer and can trigger lazy-loading exceptions. | Map entity to an immutable `UserDetails` record/DTO. |
| **Hardcoding Passwords Without Algorithm Prefix** | If `DelegatingPasswordEncoder` is used, un-prefixed hashes fail with `There is no PasswordEncoder mapped for the id "null"`. | Prefix with `{bcrypt}` or configure `setDefaultPasswordEncoderForMatches`. |
| **Storing Passwords in Char Array Without Overwriting** | Strings remain immutable in the JVM string pool; passwords persist until garbage collection. | Clear sensitive char arrays immediately after verification. |

---

## 8. Performance & Security Notes

### Performance
- **CPU Cost by Design**: Password encoders are intentionally CPU-intensive. Under high concurrent login volume, tune the work factor to balance server CPU utilization against brute-force resilience.
- **Argon2 Memory Trade-Off**: Argon2 consumes RAM per thread (e.g. 19MB). Ensure your container memory limits accommodate concurrent authentication requests.

### Security
- **Salt Generation**: BCrypt and Argon2 generate a unique, cryptographically random salt per password automatically and embed it in the resulting hash string. Never manually manage salts.
- **Timing Attack Resistance**: Spring's `matches()` methods perform fixed-time array comparisons (`MessageDigest.isEqual`), ensuring execution time does not leak how many characters matched.

---

## 9. Comparison / When to Use What

| Algorithm | CPU Hard? | Memory Hard? | Resistant to GPUs/ASICs? | Standard Recommendation |
| :--- | :--- | :--- | :--- | :--- |
| **Argon2id** | **Yes** | **Yes (19MB+ RAM)** | **Maximum (Industry Gold Standard)** | **Recommended for all new systems** |
| **BCrypt** | **Yes** | No (Minimal RAM) | High | Ubiquitous industry standard |
| **SCrypt** | Yes | Yes | High | Good alternative to Argon2 |
| **PBKDF2** | Yes | No | Moderate | Government / FIPS compliant |
| **SHA-256 / MD5** | No (Fast) | No | **Zero (Broken)** | **Strictly prohibited for passwords** |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why are SHA-256 and MD5 unacceptable for password hashing even with a salt?</strong></summary>

SHA-256 and MD5 are cryptographic checksum algorithms designed for raw speed. Modern consumer graphics cards (GPUs) and ASICs can compute tens of billions of SHA-256 hashes per second. Even with a salt, an attacker with a leaked database dump can precompute brute-force attacks across trillions of password combinations in minutes. Password hashing requires adaptive, slow algorithms (like Argon2 or BCrypt) that introduce high computational and memory barriers.
</details>

<details>
<summary><strong>2. How does <code>DelegatingPasswordEncoder</code> work and why is it important?</strong></summary>

`DelegatingPasswordEncoder` prefixes stored hashes with an identifier enclosed in curly braces (e.g., `{bcrypt}$2a$...` or `{argon2}...`). When verifying a password, it reads the prefix, delegates to the corresponding encoder in its internal map, and verifies the hash. This allows applications to seamlessly support multiple legacy hash formats simultaneously and upgrade users to modern hashing algorithms without breaking existing logins.
</details>

<details>
<summary><strong>3. What makes Argon2id superior to BCrypt?</strong></summary>

Argon2id is both **CPU-hard and memory-hard**. While BCrypt requires very little memory to compute (allowing parallel cracking on specialized GPU memory), Argon2id forces the algorithm to allocate significant blocks of RAM (e.g. 19MB or 64MB per hash). Because GPUs and ASICs have limited memory per core, they cannot parallelize Argon2 cracking, rendering hardware-accelerated attacks ineffective.
</details>

<details>
<summary><strong>4. How does <code>UserDetailsPasswordService</code> automate password hash upgrades?</strong></summary>

When an application uses `DaoAuthenticationProvider`, it checks `passwordEncoder.upgradeEncoding(storedHash)`. If the hash was created with an older algorithm or lower cost factor, and the provider has a registered `UserDetailsPasswordService`, Spring Security automatically calls `updatePassword(user, newHash)` after a successful login, persisting the updated hash to the database without requiring user intervention.
</details>

<details>
<summary><strong>5. What is the purpose of the salt in password hashing?</strong></summary>

A salt is a unique, cryptographically random sequence of bytes added to the password before hashing. It ensures that two users with the identical password will have completely different stored hashes, preventing attackers from using precomputed rainbow tables or detecting shared passwords across accounts.
</details>

<details>
<summary><strong>6. Why should <code>UserDetails</code> implementations be immutable?</strong></summary>

`UserDetails` represents the user principal stored inside `SecurityContextHolder`. If the object is mutable, concurrent threads or downstream code could accidentally or maliciously modify user roles, email, or permissions, introducing privilege escalation bugs and concurrency race conditions. Using Java records guarantees immutability.
</details>

<details>
<summary><strong>7. How does Spring Security prevent timing attacks during password verification?</strong></summary>

Standard string equality comparisons (`str1.equals(str2)`) terminate early as soon as the first mismatched character is found, leaking password length and matching characters through microsecond timing differences. Spring's `PasswordEncoder` implementations use constant-time byte comparisons (`MessageDigest.isEqual`), which take the exact same time regardless of whether zero or all characters match.
</details>

<details>
<summary><strong>8. What error is thrown if an un-prefixed password hash is verified against a <code>DelegatingPasswordEncoder</code>?</strong></summary>

It throws an `IllegalArgumentException` with the message: `There is no PasswordEncoder mapped for the id "null"`. This occurs because the encoder expects `{id}` at the beginning of the hash. It can be fixed by configuring `setDefaultPasswordEncoderForMatches`.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1 (Easy): BCrypt Encoder Bean Configuration</strong></summary>

**Task**: Define a Spring `@Bean` returning a `BCryptPasswordEncoder` configured with a strength factor of 12.

```java
@Configuration
public class SecurityBeanConfig {
    @Bean
    public PasswordEncoder passwordEncoder() {
        return new BCryptPasswordEncoder(12);
    }
}
```
</details>

<details>
<summary><strong>Exercise 2 (Easy): Encoding and Matching Assertion Test</strong></summary>

**Task**: Write a unit test asserting that encoding `"MySecretP@ss"` produces a hash starting with `"$2a$"`, and matches the original password.

```java
@Test
void testBcryptEncoding() {
    PasswordEncoder encoder = new BCryptPasswordEncoder(10);
    String hash = encoder.encode("MySecretP@ss");

    assertThat(hash).startsWith("$2a$10$");
    assertThat(encoder.matches("MySecretP@ss", hash)).isTrue();
    assertThat(encoder.matches("WrongPass", hash)).isFalse();
}
```
</details>

<details>
<summary><strong>Exercise 3 (Medium): DelegatingPasswordEncoder with BCrypt and Argon2</strong></summary>

**Task**: Configure a `DelegatingPasswordEncoder` bean where `{argon2}` is the primary encoder for new passwords and `{bcrypt}` is supported for existing hashes.

```java
@Bean
public PasswordEncoder delegatingEncoder() {
    Map<String, PasswordEncoder> map = new HashMap<>();
    map.put("argon2", Argon2PasswordEncoder.defaultsForSpringSecurity_v5_8());
    map.put("bcrypt", new BCryptPasswordEncoder(12));
    return new DelegatingPasswordEncoder("argon2", map);
}
```
</details>

<details>
<summary><strong>Exercise 4 (Medium): Custom Principal Record Implementation</strong></summary>

**Task**: Create a record `UserPrincipal(Long id, String email, String passwordHash, String role)` implementing `UserDetails`.

```java
public record UserPrincipal(Long id, String email, String passwordHash, String role) implements UserDetails {
    @Override public Collection<? extends GrantedAuthority> getAuthorities() {
        return List.of(new SimpleGrantedAuthority("ROLE_" + role));
    }
    @Override public String getPassword() { return passwordHash; }
    @Override public String getUsername() { return email; }
    @Override public boolean isAccountNonExpired() { return true; }
    @Override public boolean isAccountNonLocked() { return true; }
    @Override public boolean isCredentialsNonExpired() { return true; }
    @Override public boolean isEnabled() { return true; }
}
```
</details>

<details>
<summary><strong>Exercise 5 (Hard): Automatic Password Upgrade Implementation</strong></summary>

**Task**: Implement a `UserDetailsPasswordService` bean that persists updated password hashes into a database repository whenever `updatePassword` is triggered.

```java
@Service
public class DatabasePasswordUpgradeService implements UserDetailsPasswordService {
    @Autowired private UserRepository userRepository;

    @Override
    public UserDetails updatePassword(UserDetails user, String newPassword) {
        userRepository.updatePasswordHash(user.getUsername(), newPassword);
        return user;
    }
}
```
</details>

<details>
<summary><strong>Exercise 6 (Hard): Benchmark BCrypt vs Argon2 Execution Times</strong></summary>

**Task**: Write a test measuring the execution time of BCrypt (strength 12) vs Argon2id, verifying both take between 40ms and 200ms per operation.

```java
@Test
void benchmarkHashingSpeed() {
    PasswordEncoder bcrypt = new BCryptPasswordEncoder(12);
    long start = System.currentTimeMillis();
    bcrypt.encode("password");
    long duration = System.currentTimeMillis() - start;

    assertThat(duration).isGreaterThanOrEqualTo(40);
}
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------------+
| Modern Standard: Argon2id (Memory & CPU hard) or BCrypt (strength factor 12)          |
| Delegating:      {id}hash prefix (e.g. {argon2}... or {bcrypt}...)                    |
| Upgrades:        Implement UserDetailsPasswordService for transparent background re-hash|
| Principals:      Implement UserDetails using immutable Java 21 Records                |
| Prohibited:      NEVER use MD5, SHA-1, or SHA-256 for passwords (easily GPU-cracked!) |
| Constant Time:   Always compare hashes using constant-time comparison methods         |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading
- [Spring Security Password Storage Documentation](https://docs.spring.io/spring-security/reference/features/authentication/password-storage.html)
- [OWASP Password Storage Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Password_Storage_Cheat_Sheet.html)
- [Password Hashing Competition (Argon2 Specification)](https://www.password-hashing.net/)
- [Baeldung: Spring Security Password Encoders](https://www.baeldung.com/spring-security-registration-password-encoding-bcrypt)
