# Password Hashing in Java

> Definitive guide to secure authentication credential storage in modern Java: Argon2id, PBKDF2 with HMAC-SHA512, BCrypt, adaptive work factors, and memory-hard key derivation functions.

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced / Enterprise Security |
| **Prerequisites** | [01_cryptography](01_cryptography.md), [00_input-validation-and-injection](00_input-validation-and-injection.md) |
| **Time to Read** | 35 minutes |
| **Status** | Verified Production Guide |
| **Java Baseline** | Java 17 LTS / Java 21 LTS / Java 25 LTS |

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
10. [Interview Questions & Answers](#10-interview-questions--answers)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### What is Password Hashing?
Password hashing is the one-way transformation of plaintext passwords into irreversible, fixed-length digests designed specifically to protect user credentials stored in databases.

Unlike general-purpose cryptographic hash functions (such as SHA-256 or BLAKE3), password hashing functions are deliberately engineered to be **computationally expensive, memory-intensive, and tunable over time**.

### Why General Hash Functions Fail for Passwords
General cryptographic hash functions (SHA-256, SHA-512, MD5) prioritize high throughput and low latency:
- A modern GPU cluster (e.g., 8x NVIDIA RTX 4090) can calculate over **150 billion SHA-256 hashes per second**.
- If a database containing SHA-256 password hashes is compromised, even salted hashes can be cracked via brute-force or dictionary attacks within hours.
- A dedicated password hashing algorithm (like **Argon2id**) forces each hash calculation to consume substantial RAM (e.g., 64 MB) and hundreds of milliseconds of CPU time. This makes parallel GPU and ASIC attacks financially and computationally infeasible.

### Password Hashing Standards (OWASP & NIST)
Current OWASP and NIST guidelines mandate:
1. **Argon2id**: Winner of the Password Hashing Competition (PHC) and primary industry standard. Provides resistance against both GPU brute-force and side-channel cache attacks.
2. **scrypt**: Strong memory-hard alternative.
3. **bcrypt**: Legacy industry standard. Resistant to GPU parallelism, but limited to 72 bytes of password input.
4. **PBKDF2-HMAC-SHA512**: Supported out-of-the-box by the JDK standard library without external dependencies. Requires a high iteration count (600,000+ iterations).

---

## 2. Core Concepts

### Salt
A salt is a cryptographically strong, random byte sequence (minimum 16 bytes) generated uniquely for every user password:
- Prevents precomputation and rainbow table attacks.
- Ensures two identical passwords produce completely different hash values.
- Stored alongside the hash in plaintext (e.g., `$argon2id$v=19$m=65536,t=3,p=4$...`).

### Work Factors (Tunability / Adaptability)
Moore\'s Law and GPU advancements continuously decrease cracking costs. Secure password hashes incorporate configurable work factors that can be increased over time as hardware evolves:
- **Time Cost ($t$ / Iterations)**: Number of computation passes over the state.
- **Memory Cost ($m$)**: Amount of RAM required per hash computation (prevents massive parallelization on GPUs/FPGAs).
- **Parallelism ($p$)**: Number of CPU threads running concurrently.

### Pepper (Optional Defense-in-Depth)
A pepper is an application-wide secret key held outside the database (e.g., in a Hardware Security Module or AWS Secrets Manager) and combined with the password before hashing. If the database is dumped, the hashes cannot be attacked without also compromising the secret pepper.

---

## 3. How It Works Internally

### Memory-Hard Functions vs. ASIC Attacks
Traditional ASICs (Application-Specific Integrated Circuits) and GPUs pack thousands of tiny arithmetic logic units (ALUs) onto a single silicon chip:
- For SHA-256, each ALU requires less than a few kilobytes of cache. ASICs can run millions of attempts in parallel.
- For **Argon2id**, each attempt requires filling and random-accessing a 64 MB matrix in RAM. Modern GPUs only have 24 GB of VRAM, limiting concurrent parallel guesses to fewer than 380 attempts at any given time.

```
+--------------------------------------------------------------------------+
|                     GPU / ASIC SHA-256 PARALLEL ATTACK                   |
|  [ALU 1] [ALU 2] [ALU 3] ... [ALU 10,000]                                |
|  Fast memory access -> 150+ Billion Hashes / Second                       |
+--------------------------------------------------------------------------+
|                     ARGON2ID MEMORY-HARD BOTTLENECK                      |
|  Thread 1 needs 64 MB RAM <----+                                         |
|  Thread 2 needs 64 MB RAM <----+-- Memory bus saturated                  |
|  Thread 3 needs 64 MB RAM <----+   Hardware cost explodes for attacker  |
+--------------------------------------------------------------------------+
```

### PBKDF2 Internal Mechanism (RFC 8018)
PBKDF2 applies a pseudorandom function (PRF, typically HMAC-SHA512) repeatedly over thousands of iterations:

```
U_1 = HMAC(Password, Salt || INT_32_BE(i))
U_2 = HMAC(Password, U_1)
U_3 = HMAC(Password, U_2)
...
U_c = HMAC(Password, U_{c-1})

Output Block = U_1 ^ U_2 ^ U_3 ^ ... ^ U_c
```
Because $U_n$ depends strictly on $U_{n-1}$, PBKDF2 cannot be parallelized internally within a single password check.

---

## 4. Syntax & Usage

### 1. JDK Standard PBKDF2-HMAC-SHA512 (No External Dependencies)
Java standard library provides PBKDF2 via `SecretKeyFactory`:

```java
import javax.crypto.SecretKeyFactory;
import javax.crypto.spec.PBEKeySpec;
import java.security.NoSuchAlgorithmException;
import java.security.spec.InvalidKeySpecException;

public class StandardPbkdf2Hasher {
    private static final String ALGORITHM = "PBKDF2WithHmacSHA512";
    private static final int ITERATIONS = 600_000; // OWASP recommendation for SHA-512
    private static final int KEY_LENGTH_BITS = 256;

    public static byte[] hash(char[] password, byte[] salt) 
            throws NoSuchAlgorithmException, InvalidKeySpecException {
        PBEKeySpec spec = new PBEKeySpec(password, salt, ITERATIONS, KEY_LENGTH_BITS);
        SecretKeyFactory factory = SecretKeyFactory.getInstance(ALGORITHM);
        return factory.generateSecret(spec).getEncoded();
    }
}
```

---

## 5. Practical Examples

### Example 1: Production PBKDF2 Password Hasher with Salt and Verification
**Goal:** Self-contained production password hashing class using Java standard library with zero external dependencies.

```java
import javax.crypto.SecretKeyFactory;
import javax.crypto.spec.PBEKeySpec;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.security.SecureRandom;
import java.security.spec.InvalidKeySpecException;
import java.util.Arrays;
import java.util.Base64;

public class SecurePbkdf2Service {
    private static final String ALGORITHM = "PBKDF2WithHmacSHA512";
    private static final int ITERATIONS = 600_000;
    private static final int SALT_BYTES = 16;
    private static final int HASH_BYTES = 32; // 256-bit output
    private static final SecureRandom RANDOM = new SecureRandom();

    /**
     * Hashes a password and returns a formatted string:
     * $pbkdf2-sha512$iterations$base64Salt$base64Hash
     */
    public static String hashPassword(char[] password) {
        byte[] salt = new byte[SALT_BYTES];
        RANDOM.nextBytes(salt);

        try {
            byte[] hash = computeHash(password, salt, ITERATIONS);
            return String.format("$pbkdf2-sha512$%d$%s$%s",
                    ITERATIONS,
                    Base64.getEncoder().encodeToString(salt),
                    Base64.getEncoder().encodeToString(hash));
        } finally {
            // Clean up password chars from caller if applicable
            Arrays.fill(password, '\0');
        }
    }

    public static boolean verifyPassword(char[] password, String storedRecord) {
        String[] parts = storedRecord.split("\\$");
        if (parts.length != 5 || !parts[1].equals("pbkdf2-sha512")) {
            throw new IllegalArgumentException("Unsupported or corrupt password record format");
        }

        int iterations = Integer.parseInt(parts[2]);
        byte[] salt = Base64.getDecoder().decode(parts[3]);
        byte[] expectedHash = Base64.getDecoder().decode(parts[4]);

        try {
            byte[] actualHash = computeHash(password, salt, iterations);
            // Constant-time comparison to prevent timing attacks
            return MessageDigest.isEqual(actualHash, expectedHash);
        } finally {
            Arrays.fill(password, '\0');
        }
    }

    private static byte[] computeHash(char[] password, byte[] salt, int iterations) {
        try {
            PBEKeySpec spec = new PBEKeySpec(password, salt, iterations, HASH_BYTES * 8);
            SecretKeyFactory skf = SecretKeyFactory.getInstance(ALGORITHM);
            return skf.generateSecret(spec).getEncoded();
        } catch (NoSuchAlgorithmException | InvalidKeySpecException e) {
            throw new IllegalStateException("Crypto error during PBKDF2 hash computation", e);
        }
    }
}
```

### Example 2: Modern Argon2id Hasher (Using Bouncy Castle or Argon2-JVM)
**Goal:** Implement Argon2id password hashing adhering to OWASP recommendations ($m=65536\text{ KiB}, t=3, p=4$).

```java
import org.bouncycastle.crypto.generators.Argon2BytesGenerator;
import org.bouncycastle.crypto.params.Argon2Parameters;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.SecureRandom;
import java.util.Arrays;
import java.util.Base64;

public class ModernArgon2idService {
    // OWASP Recommended settings:
    private static final int ITERATIONS = 3;       // t = 3
    private static final int MEMORY_KIB = 65536;   // m = 64 MB
    private static final int PARALLELISM = 4;      // p = 4 threads
    private static final int HASH_LENGTH = 32;     // 32-byte output
    private static final SecureRandom RANDOM = new SecureRandom();

    public static String hash(char[] password) {
        byte[] salt = new byte[16];
        RANDOM.nextBytes(salt);

        Argon2Parameters params = new Argon2Parameters.Builder(Argon2Parameters.ARGON2_id)
                .withVersion(Argon2Parameters.ARGON2_VERSION_13)
                .withIterations(ITERATIONS)
                .withMemoryAsKB(MEMORY_KIB)
                .withParallelism(PARALLELISM)
                .withSalt(salt)
                .build();

        Argon2BytesGenerator generator = new Argon2BytesGenerator();
        generator.init(params);

        byte[] hashResult = new byte[HASH_LENGTH];
        // Convert char[] to bytes
        byte[] passwordBytes = charsToBytes(password);
        try {
            generator.generateBytes(passwordBytes, hashResult, 0, hashResult.length);
            return String.format("$argon2id$v=19$m=%d,t=%d,p=%d$%s$%s",
                    MEMORY_KIB, ITERATIONS, PARALLELISM,
                    Base64.getEncoder().encodeToString(salt),
                    Base64.getEncoder().encodeToString(hashResult));
        } finally {
            Arrays.fill(passwordBytes, (byte) 0);
            Arrays.fill(password, '\0');
        }
    }

    private static byte[] charsToBytes(char[] chars) {
        byte[] b = new byte[chars.length * 2];
        for (int i = 0; i < chars.length; i++) {
            b[i * 2] = (byte) (chars[i] >> 8);
            b[i * 2 + 1] = (byte) chars[i];
        }
        return b;
    }
}
```

### Example 3: Automatic Work Factor Upgrade on Login
**Goal:** Transparently upgrade password hashes when an authenticating user logs in with an older, weaker iteration count or obsolete algorithm.

```java
public class PasswordUpgradeAuthenticator {
    private static final int TARGET_PBKDF2_ITERATIONS = 600_000;

    public static boolean authenticateAndMigrate(
            String username, 
            char[] suppliedPassword, 
            String storedHash, 
            java.util.function.BiConsumer<String, String> hashUpdater) {

        boolean matches = SecurePbkdf2Service.verifyPassword(suppliedPassword, storedHash);
        if (!matches) {
            return false;
        }

        // Check if hash needs upgrading
        String[] parts = storedHash.split("\\$");
        int currentIterations = Integer.parseInt(parts[2]);
        if (currentIterations < TARGET_PBKDF2_ITERATIONS) {
            // Re-hash using current strong parameters and update database
            String newHash = SecurePbkdf2Service.hashPassword(suppliedPassword);
            hashUpdater.accept(username, newHash);
        }

        return true;
    }
}
```

### Example 4: Failure Case — Insecure SHA-256 Hashing
```java
// CRITICAL SECURITY FLAW - NEVER STORE PASSWORDS LIKE THIS:
MessageDigest md = MessageDigest.getInstance("SHA-256");
byte[] badHash = md.digest(passwordString.getBytes()); 
// VULNERABLE! Millions of times too fast; vulnerable to GPU brute-force!
```

---

## 6. Best Practices

1. **Use Argon2id as First Choice**: For modern systems, use Argon2id with at least 64 MB memory and 3 iterations.
2. **If Restricted to JDK Standard Library, Use PBKDF2-HMAC-SHA512**: Enforce at least 600,000 iterations.
3. **Use `char[]` Instead of `String`**: Passwords stored in `java.lang.String` cannot be zeroed out and persist in the JVM garbage collector heap.
4. **Always Generate a Unique Salt**: Never reuse salts. Each password must have its own 16-byte cryptographically secure random salt.
5. **Always Use Constant-Time Comparison**: Verify password hashes using `MessageDigest.isEqual()` to prevent timing side-channels.
6. **Implement Adaptive Hash Migration**: Check stored iteration counts or algorithm markers during successful user logins, upgrading outdated records automatically.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Secure Fix |
| :--- | :--- | :--- |
| **Using SHA-256, SHA-512, or MD5** | General hashes run in nanoseconds; GPUs calculate billions per second. | Use Argon2id or PBKDF2 with high iterations. |
| **Using a static or application-wide salt** | Enables cross-user dictionary and rainbow table attacks. | Generate a fresh 16-byte salt via `SecureRandom` per user. |
| **Comparing hashes with `==` or `String.equals()`** | Leaks character positions via timing side-channels. | Always use `MessageDigest.isEqual()`. |
| **Passing passwords as immutable `String`** | Strings reside in JVM heap/intern pool indefinitely until GC sweeps. | Use `char[]` and zero out immediately with `Arrays.fill()`. |
| **Low PBKDF2 iteration count (e.g. 1000)** | Modern CPUs/GPUs crack 1,000 PBKDF2 iterations in seconds. | Enforce minimum 600,000 iterations for SHA-512. |
| **Truncating passwords at 72 bytes in BCrypt** | BCrypt silently ignores characters beyond 72 bytes. | Pre-hash with SHA-512 or use Argon2id for long passwords. |

---

## 8. Performance & Security Notes

### Server Sizing and DoS Protection
Password hashing intentionally consumes significant CPU and memory. A malicious actor can launch a Denial of Service (DoS) attack by flooding the login endpoint with bogus authentication attempts, starving the server\'s CPU cores:
- **Rate Limiting**: Enforce strict IP and account-based rate limiting on login endpoints (e.g., maximum 5 login requests per second per IP).
- **Separate Thread Pool / Virtual Threads**: Run password verification on dedicated background thread pools so incoming web request handling is not starved of threads.
- **Fail Early on Input Bounds**: Reject passwords that are blank or unreasonably large ($> 1024$ characters) before invoking the expensive hash function.

---

## 9. Comparison / When to Use What

| Algorithm | JDK Support | Memory-Hard | GPU Resistant | Recommended Iterations / Parameters |
| :--- | :--- | :--- | :--- | :--- |
| **Argon2id** | Third-party (Bouncy Castle) | Yes (Configurable) | Exceptional | $m=64\text{ MB}, t=3, p=4$ |
| **PBKDF2-HMAC-SHA512**| Native JDK | No | Moderate | $600,000\text{ iterations}$ |
| **bcrypt** | Third-party | Low (4 KB) | High | Work factor 12–14 |
| **scrypt** | Third-party | Yes | High | $N=32768, r=8, p=1$ |
| **SHA-256** | Native JDK | No | Zero (Unsafe!) | DO NOT USE FOR PASSWORDS |

---

## 10. Interview Questions & Answers

### Q1: Why is Argon2id superior to PBKDF2 and BCrypt?
<details>
<summary>View Answer</summary>
Argon2id provides memory-hardness, meaning it requires significant RAM (e.g., 64 MB) for each calculation. This neutralizes the economic advantage of GPUs and ASICs, which rely on thousands of parallel arithmetic cores sharing small caches. Furthermore, Argon2id combines Argon2d (data-dependent access, resistant to GPU attacks) and Argon2i (data-independent access, resistant to side-channel cache timing attacks), providing comprehensive security against both threat models.
</details>

### Q2: Why must passwords in Java be handled as `char[]` rather than `String`?
<details>
<summary>View Answer</summary>
`java.lang.String` objects are immutable. Once created, a String\'s underlying character array cannot be modified or cleared by application code; it resides in memory until garbage collection occurs. If the JVM takes a heap dump or an attacker inspects unallocated memory, plaintext passwords can be extracted. In contrast, primitive `char[]` or `byte[]` buffers can be explicitly overwritten with zeros (`Arrays.fill(password, '\0')`) as soon as the hashing or verification operation completes.
</details>

### Q3: What is the purpose of a cryptographic salt, and why is it not secret?
<details>
<summary>View Answer</summary>
A salt is a unique, cryptographically random value generated for every password. Its purpose is to ensure that two identical passwords hash to different values, preventing attackers from using precomputed rainbow tables or batch-cracking identical hashes across multiple accounts. The salt does not need to be secret because its security value lies in its uniqueness, not its confidentiality; it is stored openly alongside the hash digest in the database.
</details>

### Q4: What is a password "pepper" and how does it differ from a salt?
<details>
<summary>View Answer</summary>
While a salt is unique per user and stored openly in the database, a pepper is a secret cryptographic key shared across the application and stored securely *outside* the database (e.g., in an HSM or Vault). During hashing, the password is combined with both the salt and the pepper. If an attacker dumps the database via SQL injection, they cannot crack the hashes because they lack the external pepper key.
</details>

### Q5: What is BCrypt\'s 72-byte limitation?
<details>
<summary>View Answer</summary>
BCrypt\'s underlying Blowfish cipher operates on a key schedule that only uses up to 72 bytes of input. Any characters in a password beyond 72 bytes are silently ignored by standard BCrypt implementations. If an attacker inputs a password with identical first 72 bytes and different trailing bytes, BCrypt treats them as identical. Modern systems resolve this by pre-hashing long passwords with SHA-512 before passing to BCrypt, or by using Argon2id which has no such limit.
</details>

### Q6: How do you protect a login service from CPU exhaustion caused by password hashing?
<details>
<summary>View Answer</summary>
Because password hashing algorithms are intentionally CPU-intensive, attackers can attempt Denial of Service by sending thousands of concurrent login requests. Mitigations include: (1) enforcing IP-based and user-based rate limiting at the API gateway layer, (2) validating input constraints (e.g. rejecting inputs $> 128$ chars) before invoking the hasher, (3) executing hashing on dedicated thread pools with bounded queues, and (4) utilizing CAPTCHAs after repeated failed attempts.
</details>

### Q7: Why must `MessageDigest.isEqual()` be used when verifying password hashes?
<details>
<summary>View Answer</summary>
Standard string or byte comparison loops return `false` on the first mismatching byte. This variance in execution time allows an attacker to mount a remote timing attack, measuring response latency to deduce the stored hash byte-by-byte. `MessageDigest.isEqual()` performs a constant-time comparison that always inspects all bytes regardless of mismatches, eliminating the timing side-channel.
</details>

### Q8: How should an application transition from an obsolete hashing algorithm (e.g., MD5 or low-iteration SHA) to Argon2id without resetting user passwords?
<details>
<summary>View Answer</summary>
Implement "lazy upgrade on successful authentication". When a user submits their plaintext password during login, verify it against the old hash algorithm. If verification succeeds, immediately hash the plaintext password using the new Argon2id algorithm and overwrite the database record with the new hash. Over time, all active users are migrated seamlessly without forced password resets.
</details>

---

## 11. Practice Exercises

### Exercise 1: Implement Constant-Time Byte Verification
**Task:** Write a custom constant-time byte array comparison method without using `MessageDigest.isEqual()`, explaining the bitwise logic that prevents early loop termination.
<details>
<summary>View Solution</summary>

```java
public class ConstantTimeComparator {
    public static boolean constantTimeEquals(byte[] a, byte[] b) {
        if (a == null || b == null) {
            return false;
        }
        if (a.length != b.length) {
            return false;
        }

        int result = 0;
        // Bitwise OR accumulator: result remains 0 only if all bytes match exactly
        for (int i = 0; i < a.length; i++) {
            result |= (a[i] ^ b[i]);
        }
        return result == 0;
    }
}
```
</details>

### Exercise 2: Salt Generator with Entropy Verification
**Task:** Write a helper that generates a 16-byte salt using `SecureRandom` and checks that the generated salt does not consist of all zeros or identical bytes.
<details>
<summary>View Solution</summary>

```java
import java.security.SecureRandom;

public class SaltGenerator {
    private static final SecureRandom RANDOM = new SecureRandom();

    public static byte[] generate16ByteSalt() {
        byte[] salt = new byte[16];
        RANDOM.nextBytes(salt);

        // Sanity check for zeroed buffer
        boolean allZero = true;
        for (byte b : salt) {
            if (b != 0) {
                allZero = false;
                break;
            }
        }
        if (allZero) {
            throw new IllegalStateException("CSPRNG returned all zeros - entropy pool failure");
        }
        return salt;
    }
}
```
</details>

### Exercise 3: Password Character Wiper
**Task:** Create an `AutoCloseable` wrapper for password `char[]` buffers that automatically zeroes out the array when exiting a try-with-resources block.
<details>
<summary>View Solution</summary>

```java
import java.util.Arrays;

public record ClearedPassword(char[] password) implements AutoCloseable {
    @Override
    public void close() {
        if (password != null) {
            Arrays.fill(password, '\0');
        }
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Recommended Algorithm**: Argon2id ($m=64\text{ MB}, t=3, p=4$).
- **JDK Standard Fallback**: PBKDF2-HMAC-SHA512 with 600,000+ iterations.
- **Never Use**: Plain SHA-256, SHA-512, MD5, or single-iteration hashes.
- **Data Types**: Always accept and hold passwords as `char[]`, never `String`.
- **Memory Hygiene**: Overwrite `char[]` with `\0` immediately after verification.
- **Comparisons**: Always use `MessageDigest.isEqual()` for constant-time evaluation.
- **Migration**: Check hash metadata on login and upgrade legacy hashes transparently.

---

## 13. Further Reading
- Password Hashing Competition (PHC): [Argon2 Specification](https://github.com/P-H-C/phc-winner-argon2)
- OWASP Cheat Sheet Series: [Password Storage Cheat Sheet](https://cheatsheetseries.owasp.org/cheatsheets/Password_Storage_Cheat_Sheet.html)
- RFC 8018: [PKCS #5: Password-Based Cryptography Specification Version 2.1](https://datatracker.ietf.org/doc/html/rfc8018)
- NIST Special Publication 800-63B: [Digital Identity Guidelines: Authentication and Lifecycle Management](https://pages.nist.gov/800-63-3/sp800-63b.html)
