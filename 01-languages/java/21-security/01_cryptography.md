# Cryptography in Java (JCA / JCE)

> Authoritative guide to modern, secure cryptography in Java using the Java Cryptography Architecture (JCA): symmetric encryption (AES-GCM), asymmetric encryption (RSA, ECC), digital signatures, and cryptographic hashing.

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced / Enterprise Security |
| **Prerequisites** | [00_input-validation-and-injection](00_input-validation-and-injection.md), [08_collections](../08-collections/README.md), [11_io](../11-io-and-networking/README.md) |
| **Time to Read** | 40 minutes |
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

### What is the Java Cryptography Architecture (JCA)?
The Java Cryptography Architecture (JCA) is a standardized, extensible framework integrated directly into the Java Standard Edition runtime (`java.security` and `javax.crypto` packages). It provides a provider-based architecture for implementing cryptographic operations, including:
- **Symmetric Key Encryption**: Confidentiality using shared private keys (AES-GCM, ChaCha20-Poly1305).
- **Asymmetric Key Encryption**: Key exchange and encryption using public/private key pairs (RSA, ECC).
- **Cryptographic Hash Functions**: Deterministic one-way digest algorithms (SHA-256, SHA-384, SHA-512, SHA3-256).
- **Digital Signatures**: Non-repudiation and integrity verification (Ed25519, ECDSA, RSA-PSS).
- **Key Agreement**: Secure shared secret establishment over insecure channels (ECDH, X25519).
- **Message Authentication Codes (MAC)**: Symmetric integrity and authenticity (HMAC-SHA256).
- **Cryptographically Secure Pseudorandom Number Generation**: Unpredictable entropy sources (`SecureRandom`).

### Why Cryptography Matters in Enterprise Java
Unencrypted data at rest and in transit exposes organizations to data breaches, compliance violations (GDPR, PCI-DSS, HIPAA), and identity theft. However, implementing flawed or outdated cryptography (such as DES, MD5, SHA-1, ECB mode, or insecure random generators) provides false security and is actively exploited in production systems.

Modern enterprise Java applications require cryptographically sound standards:
1. **Authenticated Encryption with Associated Data (AEAD)**: Guaranteeing both confidentiality and ciphertext authenticity simultaneously (AES-GCM).
2. **Post-Compromise Security**: Elliptic curve cryptography (Ed25519, X25519) providing faster operations and smaller key sizes.
3. **Hardware Acceleration**: Utilizing CPU instructions (Intel AES-NI, ARMv8 Cryptography Extensions) via HotSpot intrinsics.

---

## 2. Core Concepts

### Provider-Based Architecture
JCA decouples cryptographic API consumers from actual algorithm implementations using a Service Provider Interface (SPI).
- Developers interact with engine classes: `Cipher`, `MessageDigest`, `Signature`, `KeyFactory`, `KeyPairGenerator`, `Mac`, `SecureRandom`.
- The JDK ships with default providers (`SUN`, `SunJCE`, `SunEC`, `SunPKCS11`).
- Organizations can plug in certified third-party providers (such as Bouncy Castle or Amazon Corretto Crypto Provider) without changing application code.

### Symmetric vs. Asymmetric Encryption
```
+--------------------------------------------------------------------------+
|                           SYMMETRIC ENCRYPTION                           |
| Plaintext ----[ Encrypt with Key K ]----> Ciphertext                     |
| Ciphertext ---[ Decrypt with Key K ]----> Plaintext                      |
| (Fast, ideal for large payloads, requires secure key pre-sharing)        |
+--------------------------------------------------------------------------+
|                          ASYMMETRIC ENCRYPTION                           |
| Plaintext ----[ Encrypt with Public Key ]----> Ciphertext                |
| Ciphertext ---[ Decrypt with Private Key ]---> Plaintext                 |
| (Slower, ideal for key exchange, TLS handshakes, digital signatures)     |
+--------------------------------------------------------------------------+
```

### AEAD: Authenticated Encryption with Associated Data
Traditional block cipher modes like CBC (Cipher Block Chaining) only provide confidentiality. They are vulnerable to padding oracle attacks (e.g., POODLE) unless combined with a MAC using the encrypt-then-MAC pattern.

Modern standards mandate **AEAD (such as AES-GCM)**:
- Provides confidentiality (encryption).
- Generates an authentication tag (typically 128 bits) over the ciphertext and optional unencrypted header metadata (Associated Data).
- Any tampering with the ciphertext causes decryption to throw an `AEADBadTagException` before releasing decrypted bytes.

---

## 3. How It Works Internally

### AES-GCM Internal Architecture
AES-GCM (Galois/Counter Mode) combines counter mode (CTR) for encryption with GMAC (Galois Message Authentication Code) for authentication:

```
        Initialization Vector (IV, 96-bit) + 32-bit Counter
                           |
                        [ AES ] <--- Secret Key (256-bit)
                           |
        Plaintext Block --(+)-----> Ciphertext Block ----+
                           |                             |
                           +----[ Galois Field Mult ]<---+
                                         |
                                         v
                            Authentication Tag (128-bit)
```

1. **Initialization Vector (IV / Nonce)**: Must be unique for every single encryption operation performed under the same key. The recommended length is 96 bits (12 bytes). Repeating an IV with the same key in GCM mode destroys the cryptographic authenticity guarantees!
2. **CTR Mode Encryption**: Generates a keystream by encrypting sequential counter values with AES, then XORs the keystream with the plaintext.
3. **GMAC Authentication**: Multiplies blocks over a Galois finite field ($GF(2^{128})$) to compute a 128-bit authentication tag.

### Hardware Acceleration (AES-NI)
The JVM HotSpot C2 JIT compiler automatically detects hardware cryptographic instructions (`aesenc`, `aesdec` on x86/x64). HotSpot replaces Java bytecode loops with native SIMD instructions, achieving speeds exceeding 4 GB/sec per CPU core.

```
+----------------------------------------------------------------------+
| Java Code: cipher.doFinal(plaintext)                                 |
|   |                                                                  |
|   v                                                                  |
| HotSpot C2 Compiler: Replaces com.sun.crypto.provider with Intrinsics|
|   |                                                                  |
|   v                                                                  |
| Hardware Level: Intel AES-NI / ARMv8 Crypto Assembly Instructions    |
+----------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### 1. Generating Secure Random Bytes
Never use `java.util.Random` or `ThreadLocalRandom` for security or tokens. Always use `java.security.SecureRandom`:

```java
import java.security.SecureRandom;

public class SecurityTokenGenerator {
    private static final SecureRandom SECURE_RANDOM = new SecureRandom();

    public static byte[] generateRandomBytes(int length) {
        byte[] bytes = new byte[length];
        SECURE_RANDOM.nextBytes(bytes); // High-entropy CSPRNG
        return bytes;
    }
}
```

### 2. Computing Cryptographic Hashes (SHA-256)
```java
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.HexFormat;

public class HashUtil {
    public static String sha256Hex(String input) {
        try {
            MessageDigest digest = MessageDigest.getInstance("SHA-256");
            byte[] hashBytes = digest.digest(input.getBytes(StandardCharsets.UTF_8));
            return HexFormat.of().formatHex(hashBytes); // Java 17+ HexFormat
        } catch (NoSuchAlgorithmException e) {
            throw new IllegalStateException("JVM missing SHA-256 provider", e);
        }
    }
}
```

---

## 5. Practical Examples

### Example 1: Basic & Complete AES-256-GCM Encryption and Decryption
**Goal:** Encrypt sensitive strings into self-contained payload packets containing `[12-byte IV || Ciphertext || 16-byte Tag]`.

```java
import javax.crypto.Cipher;
import javax.crypto.KeyGenerator;
import javax.crypto.SecretKey;
import javax.crypto.spec.GCMParameterSpec;
import javax.crypto.spec.SecretKeySpec;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.security.SecureRandom;
import java.util.Base64;

public class AesGcmCryptoService {
    private static final String ALGORITHM = "AES/GCM/NoPadding";
    private static final int GCM_IV_LENGTH_BYTES = 12; // 96 bits recommended for GCM
    private static final int GCM_TAG_LENGTH_BITS = 128; // 128-bit authentication tag
    private static final SecureRandom SECURE_RANDOM = new SecureRandom();

    public static SecretKey generateKey() throws Exception {
        KeyGenerator keyGen = KeyGenerator.getInstance("AES");
        keyGen.init(256, SECURE_RANDOM);
        return keyGen.generateKey();
    }

    public static String encrypt(String plaintext, SecretKey secretKey) throws Exception {
        // Step 1: Generate unique 96-bit IV
        byte[] iv = new byte[GCM_IV_LENGTH_BYTES];
        SECURE_RANDOM.nextBytes(iv);

        // Step 2: Initialize Cipher in GCM mode
        Cipher cipher = Cipher.getInstance(ALGORITHM);
        GCMParameterSpec parameterSpec = new GCMParameterSpec(GCM_TAG_LENGTH_BITS, iv);
        cipher.init(Cipher.ENCRYPT_MODE, secretKey, parameterSpec);

        // Step 3: Encrypt plaintext (ciphertext includes 16-byte GCM tag at end)
        byte[] plaintextBytes = plaintext.getBytes(StandardCharsets.UTF_8);
        byte[] ciphertext = cipher.doFinal(plaintextBytes);

        // Step 4: Pack IV and Ciphertext together into a single byte payload
        ByteBuffer byteBuffer = ByteBuffer.allocate(iv.length + ciphertext.length);
        byteBuffer.put(iv);
        byteBuffer.put(ciphertext);

        return Base64.getEncoder().encodeToString(byteBuffer.array());
    }

    public static String decrypt(String base64Payload, SecretKey secretKey) throws Exception {
        byte[] packedBytes = Base64.getDecoder().decode(base64Payload);

        // Step 1: Extract IV
        if (packedBytes.length < GCM_IV_LENGTH_BYTES + 16) {
            throw new IllegalArgumentException("Corrupt ciphertext payload: insufficient length");
        }
        ByteBuffer byteBuffer = ByteBuffer.wrap(packedBytes);
        byte[] iv = new byte[GCM_IV_LENGTH_BYTES];
        byteBuffer.get(iv);

        // Step 2: Extract remaining ciphertext
        byte[] ciphertext = new byte[byteBuffer.remaining()];
        byteBuffer.get(ciphertext);

        // Step 3: Decrypt and verify tag
        Cipher cipher = Cipher.getInstance(ALGORITHM);
        GCMParameterSpec parameterSpec = new GCMParameterSpec(GCM_TAG_LENGTH_BITS, iv);
        cipher.init(Cipher.DECRYPT_MODE, secretKey, parameterSpec);

        byte[] decryptedBytes = cipher.doFinal(ciphertext);
        return new String(decryptedBytes, StandardCharsets.UTF_8);
    }
}
```

### Example 2: Real-World Digital Signatures with Modern Ed25519 (Java 15+)
**Goal:** Sign and verify API payloads or transactions using modern Edwards-curve Digital Signature Algorithm (Ed25519, JEP 339).

```java
import java.nio.charset.StandardCharsets;
import java.security.*;
import java.util.Base64;

public class DigitalSignatureService {
    public static KeyPair generateEd25519KeyPair() throws NoSuchAlgorithmException {
        KeyPairGenerator kpg = KeyPairGenerator.getInstance("Ed25519");
        return kpg.generateKeyPair();
    }

    public static String signPayload(String payload, PrivateKey privateKey) throws Exception {
        Signature signature = Signature.getInstance("Ed25519");
        signature.initSign(privateKey);
        signature.update(payload.getBytes(StandardCharsets.UTF_8));
        byte[] signedBytes = signature.sign();
        return Base64.getEncoder().encodeToString(signedBytes);
    }

    public static boolean verifySignature(String payload, String base64Signature, PublicKey publicKey) 
            throws Exception {
        Signature signature = Signature.getInstance("Ed25519");
        signature.initVerify(publicKey);
        signature.update(payload.getBytes(StandardCharsets.UTF_8));
        byte[] signatureBytes = Base64.getDecoder().decode(base64Signature);
        return signature.verify(signatureBytes);
    }
}
```

### Example 3: Edge Case — Authenticated Encryption with Associated Data (AAD)
**Goal:** Bind plaintext metadata (e.g., tenant ID, creation timestamp) to the ciphertext so that changing the tenant ID causes decryption to fail.

```java
import javax.crypto.Cipher;
import javax.crypto.SecretKey;
import javax.crypto.spec.GCMParameterSpec;
import java.nio.charset.StandardCharsets;
import java.security.SecureRandom;

public class AadEncryptionService {
    public static byte[] encryptWithTenant(byte[] plaintext, SecretKey key, String tenantId) throws Exception {
        byte[] iv = new byte[12];
        new SecureRandom().nextBytes(iv);

        Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
        cipher.init(Cipher.ENCRYPT_MODE, key, new GCMParameterSpec(128, iv));

        // Supply Associated Data BEFORE doFinal
        cipher.updateAAD(tenantId.getBytes(StandardCharsets.UTF_8));
        byte[] ciphertext = cipher.doFinal(plaintext);

        // Prepend IV
        byte[] result = new byte[iv.length + ciphertext.length];
        System.arraycopy(iv, 0, result, 0, iv.length);
        System.arraycopy(ciphertext, 0, result, iv.length, ciphertext.length);
        return result;
    }
}
```

### Example 4: Failure Case — Insecure ECB Mode
```java
// CRITICAL SECURITY FLAW - NEVER USE ECB MODE:
Cipher cipher = Cipher.getInstance("AES/ECB/PKCS5Padding"); // VULNERABLE!
// ECB mode encrypts identical plaintext blocks into identical ciphertext blocks.
// An attacker can see patterns (e.g. ECB Penguin) and replay or reorder blocks!
```

---

## 6. Best Practices

1. **Always Use AES-GCM (AEAD)**: Do not use AES-CBC unless strictly combined with HMAC in an Encrypt-then-MAC sequence. Never use AES-ECB.
2. **Never Reuse an IV in GCM**: Reusing a 96-bit IV with the same AES key in GCM mode allows an attacker to recover the authentication key and tamper with ciphertexts.
3. **Use 256-bit AES Keys**: Always specify 256-bit keys (`AES-256`) for long-term security.
4. **Use Ed25519 or ECDSA for Digital Signatures**: Prefer Ed25519 (since Java 15) over older RSA-1024 or RSA-2048 keys.
5. **Wipe Sensitive Keys from Memory**: Clear byte arrays holding private keys or plaintexts when done using `Arrays.fill(bytes, (byte) 0)`.
6. **Use Constant-Time Comparison**: When verifying tokens or hashes, use `MessageDigest.isEqual()` to prevent timing attacks.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Secure Fix |
| :--- | :--- | :--- |
| **Using `AES/ECB/PKCS5Padding`** | Identical blocks produce identical ciphertext; leaks structural data. | Use `AES/GCM/NoPadding`. |
| **Hardcoding static IVs in AES-GCM** | Repeating IV destroys Galois authentication security. | Generate a fresh 12-byte IV using `SecureRandom` for every encryption. |
| **Using `new Random()` for keys or tokens** | Linear congruential generator is predictable; internal state can be deduced. | Always use `java.security.SecureRandom`. |
| **Comparing hashes with `String.equals()`** | Short-circuits on first mismatched character, exposing timing side-channel. | Use `MessageDigest.isEqual(byte[], byte[])` (constant-time). |
| **Using MD5 or SHA-1 for security** | Mathematically broken; collision attacks can be generated cheaply. | Use SHA-256, SHA-384, SHA-512, or SHA3-256. |
| **Using raw RSA without padding** | Textbook RSA is deterministic and vulnerable to mathematical attacks. | Use `RSA/ECB/OAEPWithSHA-256AndMGF1Padding`. |

---

## 8. Performance & Security Notes

### Constant-Time Equality Check
Standard string comparison `str1.equals(str2)` compares characters sequentially from index 0. If the first character mismatches, it returns in 5 nanoseconds; if the 30th character mismatches, it returns in 25 nanoseconds. Attackers exploit this timing differential over thousands of HTTP requests to reconstruct secret tokens character by character.

Always use constant-time comparison:
```java
boolean valid = MessageDigest.isEqual(
    token.getBytes(StandardCharsets.UTF_8), 
    expected.getBytes(StandardCharsets.UTF_8)
);
```

### JIT Intrinsics and Memory Zeroing
Strings in Java are immutable and placed in the JVM Heap or String Pool. They cannot be reliably zeroed out and persist until garbage collection.
- Always store sensitive data (keys, passwords, PINs) in `byte[]` or `char[]`.
- Explicitly zero out the array immediately after use:
```java
byte[] sensitiveKey = fetchKey();
try {
    process(sensitiveKey);
} finally {
    java.util.Arrays.fill(sensitiveKey, (byte) 0);
}
```

---

## 9. Comparison / When to Use What

| Algorithm | Type | Security Level | Ideal Use Case |
| :--- | :--- | :--- | :--- |
| **AES-GCM (256-bit)** | Symmetric AEAD | Extremely High | Data at rest, message encryption, file encryption |
| **ChaCha20-Poly1305** | Symmetric Stream AEAD | Extremely High | Mobile devices or hardware without AES-NI chips |
| **Ed25519** | Asymmetric Signature | Modern High Speed | Microservice token signing, git commits, API verification |
| **RSA (3072+ bit) OAEP** | Asymmetric Encryption | Legacy Standard | Encrypting small keys for asymmetric recipients |
| **SHA-256 / SHA3-256** | Hash Function | High Integrity | File integrity verification, digital signature hashing |
| **HMAC-SHA256** | Symmetric Authenticator | High Authenticity | API request signatures, webhook integrity |

---

## 10. Interview Questions & Answers

### Q1: What is AEAD and why is AES-GCM preferred over AES-CBC?
<details>
<summary>View Answer</summary>
AEAD stands for Authenticated Encryption with Associated Data. Traditional modes like AES-CBC only provide confidentiality. Without a separate Message Authentication Code (MAC), an attacker can tamper with ciphertext bits in transit. Furthermore, CBC mode implementations frequently suffer from padding oracle attacks (e.g., POODLE). AES-GCM combines counter-mode encryption with Galois field authentication, guaranteeing confidentiality, integrity, and authenticity in a single unified primitive. Decryption automatically fails with `AEADBadTagException` if even a single bit of ciphertext or associated metadata is altered.
</details>

### Q2: Why is `MessageDigest.isEqual()` critical when checking cryptographic signatures or HMACs?
<details>
<summary>View Answer</summary>
Standard comparison methods (`String.equals` or `Arrays.equals`) terminate early on the first mismatched byte. This variable execution time creates a timing side-channel attack. Remote attackers measure the microsecond or nanosecond response latency of an application across thousands of requests to deduce valid characters one by one. `MessageDigest.isEqual()` performs a bitwise constant-time comparison that always inspects all bytes regardless of where differences occur.
</details>

### Q3: What happens if an Initialization Vector (IV) is reused with the same key in AES-GCM?
<details>
<summary>View Answer</summary>
Reusing an IV in GCM mode is catastrophic. Because GCM uses counter mode for encryption, repeating an IV produces identical keystreams; XORing two ciphertexts encrypted with the same key and IV yields the XOR of the plaintexts ($C_1 \oplus C_2 = P_1 \oplus P_2$). Furthermore, repeating an IV allows an attacker to solve linear equations over the Galois finite field to recover the GCM authentication subkey, completely destroying both confidentiality and message authenticity.
</details>

### Q4: Why should passwords never be hashed with standard SHA-256?
<details>
<summary>View Answer</summary>
Standard hash functions like SHA-256, SHA-512, and MD5 are designed to be extremely fast in hardware, capable of calculating billions of hashes per second on modern GPUs. If a database is breached, attackers use precomputed rainbow tables or brute-force GPUs to crack plain SHA-256 password hashes within hours. Passwords must be hashed using slow, memory-hard Key Derivation Functions (KDFs) like Argon2id, PBKDF2, or BCrypt, which enforce computational and memory costs.
</details>

### Q5: How do JCA Providers work and how can you inspect registered providers at runtime?
<details>
<summary>View Answer</summary>
JCA uses a pluggable provider model. Engine classes like `Cipher` look up algorithm implementations through registered `Provider` instances. Providers are ranked by priority. At runtime, you can inspect installed providers using `Security.getProviders()`, add a provider with `Security.addProvider(new BouncyCastleProvider())`, or insert one at highest priority with `Security.insertProviderAt(provider, 1)`.
</details>

### Q6: What is the purpose of Associated Data (AAD) in AES-GCM?
<details>
<summary>View Answer</summary>
Associated Data (AAD) is metadata that must remain unencrypted (such as protocol headers, account IDs, or sequence numbers) but must be cryptographically bound to the ciphertext. During decryption, GCM includes the AAD in the Galois hash calculation. If an attacker modifies the unencrypted AAD in transit, the GCM tag verification fails, preventing ciphertext replay or parameter swapping attacks.
</details>

### Q7: Why is `SecureRandom` superior to `java.util.Random`?
<details>
<summary>View Answer</summary>
`java.util.Random` uses a simple 48-bit Linear Congruential Formula (LCG). Given just two consecutive 32-bit random integers, the entire internal state can be mathematically computed, allowing an attacker to predict all past and future values. `SecureRandom` utilizes operating system entropy pools (`/dev/urandom` on Unix or CryptGenRandom/BCrypt on Windows) and cryptographically secure PRNG algorithms (like SHA1PRNG or NativePRNG) whose outputs cannot be predicted in polynomial time.
</details>

### Q8: What is Ed25519 and why is it preferred over RSA for digital signatures?
<details>
<summary>View Answer</summary>
Ed25519 is an Edwards-curve Digital Signature Algorithm operating over Curve25519 (standardized in Java 15 via JEP 339). It offers equivalent security to 3072-bit RSA while using 256-bit public keys and 512-bit signatures. It is immune to several side-channel attacks by design (no branch instructions or table lookups dependent on secret keys) and provides vastly superior signing and verification throughput compared to RSA.
</details>

---

## 11. Practice Exercises

### Exercise 1: Implement an HMAC-SHA256 Request Signer
**Task:** Implement a utility class that signs an HTTP payload using HMAC-SHA256 and verifies incoming requests against timing attacks.
<details>
<summary>View Solution</summary>

```java
import javax.crypto.Mac;
import javax.crypto.spec.SecretKeySpec;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.Base64;

public class HmacSigner {
    private static final String HMAC_ALGO = "HmacSHA256";

    public static String sign(String payload, byte[] secretKey) throws Exception {
        Mac mac = Mac.getInstance(HMAC_ALGO);
        mac.init(new SecretKeySpec(secretKey, HMAC_ALGO));
        byte[] rawHmac = mac.doFinal(payload.getBytes(StandardCharsets.UTF_8));
        return Base64.getEncoder().encodeToString(rawHmac);
    }

    public static boolean verify(String payload, String base64Signature, byte[] secretKey) throws Exception {
        String expectedSignature = sign(payload, secretKey);
        // Constant-time comparison
        return MessageDigest.isEqual(
            base64Signature.getBytes(StandardCharsets.UTF_8),
            expectedSignature.getBytes(StandardCharsets.UTF_8)
        );
    }
}
```
</details>

### Exercise 2: Secure Envelope Encryption Key Generator
**Task:** Envelope encryption encrypts data with a Data Encryption Key (DEK), and encrypts the DEK with a Key Encryption Key (KEK). Implement the local generation and wrapping of a 256-bit AES DEK using an RSA public key.
<details>
<summary>View Solution</summary>

```java
import javax.crypto.Cipher;
import javax.crypto.KeyGenerator;
import javax.crypto.SecretKey;
import java.security.PublicKey;
import java.security.SecureRandom;

public class EnvelopeEncryption {
    public static byte[] wrapDek(SecretKey dek, PublicKey kekPublicKey) throws Exception {
        Cipher cipher = Cipher.getInstance("RSA/ECB/OAEPWithSHA-256AndMGF1Padding");
        cipher.init(Cipher.WRAP_MODE, kekPublicKey);
        return cipher.wrap(dek);
    }

    public static SecretKey generateDek() throws Exception {
        KeyGenerator kg = KeyGenerator.getInstance("AES");
        kg.init(256, new SecureRandom());
        return kg.generateKey();
    }
}
```
</details>

### Exercise 3: File Streaming Encryption with CipherInputStream
**Task:** Write an efficient file encryption method that encrypts large multi-gigabyte files using AES-GCM without loading the entire file into memory.
<details>
<summary>View Solution</summary>

```java
import javax.crypto.Cipher;
import javax.crypto.CipherOutputStream;
import javax.crypto.SecretKey;
import javax.crypto.spec.GCMParameterSpec;
import java.io.*;
import java.security.SecureRandom;

public class StreamEncryptor {
    public static void encryptFile(File source, File destination, SecretKey key) throws Exception {
        byte[] iv = new byte[12];
        new SecureRandom().nextBytes(iv);

        Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
        cipher.init(Cipher.ENCRYPT_MODE, key, new GCMParameterSpec(128, iv));

        try (OutputStream fos = new BufferedOutputStream(new FileOutputStream(destination))) {
            fos.write(iv); // Write IV as header
            try (CipherOutputStream cos = new CipherOutputStream(fos, cipher);
                 InputStream fis = new BufferedInputStream(new FileInputStream(source))) {
                byte[] buffer = new byte[8192];
                int bytesRead;
                while ((bytesRead = fis.read(buffer)) != -1) {
                    cos.write(buffer, 0, bytesRead);
                }
            }
        }
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Symmetric Encryption**: Always default to `AES/GCM/NoPadding` with a 256-bit key and fresh 96-bit (12-byte) IV.
- **IV / Nonce**: Never reuse an IV with the same AES key in GCM mode.
- **Random Numbers**: Always use `java.security.SecureRandom`. Never use `java.util.Random`.
- **Digital Signatures**: Use `Ed25519` (Java 15+) or ECDSA over P-256/P-384. Avoid legacy RSA-1024/2048.
- **Comparisons**: Always use `MessageDigest.isEqual()` to prevent timing side-channels.
- **Memory Hygiene**: Use `byte[]` or `char[]` for secrets and call `Arrays.fill(arr, (byte) 0)` after processing.
- **Password Hashes**: Never use SHA-256 for passwords; use Argon2id, PBKDF2, or BCrypt.

---

## 13. Further Reading
- Oracle Java Security Standard Algorithm Names: [Standard Algorithm Name Documentation](https://docs.oracle.com/en/java/javase/21/docs/specs/security/standard-names.html)
- NIST SP 800-38D: [Recommendation for Block Cipher Modes of Operation: Galois/Counter Mode (GCM)](https://csrc.nist.gov/publications/detail/sp/800-38d/final)
- RFC 8439: [ChaCha20 and Poly1305 for IETF Protocols](https://datatracker.ietf.org/doc/html/rfc8439)
- JEP 339: [Edwards-Curve Digital Signature Algorithm (EdDSA)](https://openjdk.org/jeps/339)
