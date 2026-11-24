# 14. Passkeys & WebAuthn / FIDO2 Authentication in Spring Boot

| Metadata | Value |
|---|---|
| **Level** | Advanced |
| **Prerequisites** | `01-spring-security-architecture.md`, `02-authentication-fundamentals.md`, Asymmetric Cryptography Basics |
| **Estimated Reading Time** | 35 minutes |
| **Status** | Complete |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | OpenJDK 21, Spring Boot 3.4.1, WebAuthn4J / Spring Security 6.4+ |

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
Passwords are the single weakest link in modern cybersecurity. Users inevitably choose predictable passwords, reuse them across multiple sites, and fall victim to credential stuffing, database leaks, and phishing attacks. Traditional Multi-Factor Authentication (SMS OTP, Email codes, and even TOTP authenticator apps) can still be intercepted by sophisticated real-time adversary-in-the-middle (AiTM) phishing proxies (e.g., Evilginx).

**Passkeys**, built on the **W3C WebAuthn** and **FIDO2** standards, represent a paradigm shift: **phishing-resistant, passwordless authentication**.
- The server never receives or stores a shared password or secret; it stores only the user's public key.
- The user authenticates locally using biometrics (TouchID, FaceID, Windows Hello) or a hardware security key (YubiKey).
- The cryptographic signature produced during login is cryptographically bound to the website's exact domain origin (`rpid`), making phishing physically impossible.

### Historical Evolution
- **FIDO 1.0 (U2F / UAF)**: Introduced hardware dongles (YubiKey) for second-factor authentication, requiring browser extensions.
- **W3C WebAuthn Level 1 & 2 (2019-2022)**: Brought native JavaScript browser APIs (`navigator.credentials.create()`, `navigator.credentials.get()`).
- **Passkeys & Multi-Device FIDO Credentials (2022-Present)**: Allowed encrypted synchronization of credentials across user devices via Apple iCloud Keychain, Google Password Manager, and 1Password.
- **Spring Security 6.4+ / 7.x**: Introduced first-class native WebAuthn/Passkey support directly into core Spring Security, eliminating the need for fragmented third-party libraries.

### Real-World Relevance
Enterprise identity systems and consumer web applications are aggressively adopting passkeys as the default sign-in mechanism to meet compliance standards (such as Executive Order 14028, NIST SP 800-63B AAL3, and PCI-DSS 4.0).

---

## 2. Core Concepts

### Architectural Contracts
- **Relying Party (RP)**: The server hosting the application (your Spring Boot backend). Defined by an `rpId` (e.g., `example.com`) and `rpName`.
- **Authenticator**: The client hardware or software managing private keys (Apple TouchID, Windows Hello, YubiKey).
- **Attestation**: The proof provided by an authenticator during registration confirming its hardware model, integrity, and authenticity.
- **Assertion**: The cryptographic signature provided during login proving possession of the private key.
- **User Presence (UP)**: Proof that a human physically touched the authenticator.
- **User Verification (UV)**: Proof that the specific authorized user authenticated locally via biometrics (fingerprint/facial scan) or local device PIN.

### The Mental Model
```
[ Browser / Authenticator ]                               [ Spring Boot (Relying Party) ]
           │                                                              │
           │ 1. POST /webauthn/register/options ─────────────────────────►│
           │                                                              ├── Generates Cryptographic Challenge
           │◄── 2. Challenge + RP Metadata ───────────────────────────────┤ (Stored in Session / Redis)
           │
     [ Local Biometric Check (TouchID) ]
     [ Generates KeyPair (Private Key in Secure Enclave) ]
           │
           │ 3. POST /webauthn/register (Attestation Object + ClientData)─►│
           │                                                              ├── Validates Challenge & Origin
           │                                                              └── Stores Public Key + Credential ID
           │◄── 4. Registration Succeeded (200 OK) ───────────────────────┤
```

### Invariants
1. The private key **never** leaves the user's hardware device (Secure Enclave / TPM / Security Key).
2. The browser automatically verifies that the current domain matches the `rpId`; a passkey registered for `bank.com` can **never** be presented to `bank-phishing.com`.
3. Every registration and authentication ceremony must use a fresh, high-entropy cryptographic challenge that expires quickly and can only be consumed once.

---

## 3. How It Works Internally

### Authentication (Assertion) Ceremony
1. **Challenge Request**: The client requests authentication options (`/webauthn/authenticate/options`).
2. **Challenge Creation**: The Relying Party generates a random 32-byte challenge, saves it in the user's session, and returns it with allowed credential IDs.
3. **Authenticator Assertion**:
   - The browser calls `navigator.credentials.get({ publicKey: options })`.
   - The authenticator prompts for biometrics (TouchID/FaceID).
   - Upon verification, the authenticator signs `clientDataHash + authenticatorData` using the private key.
4. **Signature Verification**:
   - The client submits the assertion response to Spring Boot.
   - Spring Boot verifies:
     1. The challenge matches the stored session challenge.
     2. The origin matches `https://example.com`.
     3. The signature is valid against the stored public key using SHA-256 and ECDSA (or RSA).
     4. The `signCount` is strictly greater than the stored count (detecting cloned authenticators).

---

## 4. Syntax & Usage

### 1. Spring Security 6.4+ WebAuthn DSL Configuration
```java
package com.example.security.config;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.security.config.Customizer;
import org.springframework.security.config.annotation.web.builders.HttpSecurity;
import org.springframework.security.config.annotation.web.configuration.EnableWebSecurity;
import org.springframework.security.web.SecurityFilterChain;

@Configuration
@EnableWebSecurity
public class PasskeySecurityConfig {

    @Bean
    public SecurityFilterChain passkeyFilterChain(HttpSecurity http) throws Exception {
        return http
            .authorizeHttpRequests(auth -> auth
                .requestMatchers("/", "/login", "/webauthn/**", "/error").permitAll()
                .anyRequest().authenticated()
            )
            .webAuthn(webAuthn -> webAuthn
                .rpId("example.com")
                .rpName("Acme Enterprise Portal")
                .allowedOrigins("https://example.com", "https://auth.example.com")
            )
            .formLogin(Customizer.withDefaults()) // Optional fallback
            .build();
    }
}
```

---

## 5. Practical Examples

### Example 1: WebAuthn Credential Entity for Relational Databases
Storing public keys, credential IDs, and signature counters for registered passkeys.

```java
package com.example.security.domain;

import jakarta.persistence.*;
import java.time.Instant;

@Entity
@Table(name = "user_passkeys")
public class UserPasskey {

    @Id
    @GeneratedValue(strategy = GenerationType.IDENTITY)
    private Long id;

    @Column(nullable = false)
    private String username;

    @Column(nullable = false, unique = true, length = 512)
    private String credentialId; // Base64URL-encoded

    @Lob
    @Column(nullable = false)
    private byte[] publicKeyCose; // COSE-formatted public key bytes

    @Column(nullable = false)
    private Long signatureCounter;

    @Column(nullable = false)
    private String userAgent;

    @Column(nullable = false)
    private Instant createdAt;

    // Constructors, Getters, and Setters
    public UserPasskey() {}

    public UserPasskey(String username, String credentialId, byte[] publicKeyCose, Long signatureCounter, String userAgent) {
        this.username = username;
        this.credentialId = credentialId;
        this.publicKeyCose = publicKeyCose;
        this.signatureCounter = signatureCounter;
        this.userAgent = userAgent;
        this.createdAt = Instant.now();
    }

    public Long getId() { return id; }
    public String getUsername() { return username; }
    public String getCredentialId() { return credentialId; }
    public byte[] getPublicKeyCose() { return publicKeyCose; }
    public Long getSignatureCounter() { return signatureCounter; }
    public void setSignatureCounter(Long signatureCounter) { this.signatureCounter = signatureCounter; }
}
```

### Example 2: Custom Passkey Registration Ceremony Controller
Handling registration option creation and attestation validation.

```java
package com.example.security.controller;

import jakarta.servlet.http.HttpSession;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.security.SecureRandom;
import java.util.Base64;
import java.util.List;
import java.util.Map;

@RestController
@RequestMapping("/api/webauthn/register")
public class PasskeyRegistrationController {

    private final SecureRandom secureRandom = new SecureRandom();

    @PostMapping("/options")
    public ResponseEntity<Map<String, Object>> generateRegistrationOptions(
            @RequestParam String username, 
            HttpSession session) {

        byte[] challengeBytes = new byte[32];
        secureRandom.nextBytes(challengeBytes);
        String challengeBase64 = Base64.getUrlEncoder().withoutPadding().encodeToString(challengeBytes);

        // Store challenge in session for ceremony verification
        session.setAttribute("WEBAUTHN_CHALLENGE", challengeBase64);
        session.setAttribute("WEBAUTHN_USER", username);

        Map<String, Object> options = Map.of(
            "challenge", challengeBase64,
            "rp", Map.of("name", "Acme Enterprise", "id", "example.com"),
            "user", Map.of(
                "id", Base64.getUrlEncoder().withoutPadding().encodeToString(username.getBytes()),
                "name", username,
                "displayName", username
            ),
            "pubKeyCredParams", List.of(
                Map.of("type", "public-key", "alg", -7),  // ES256 (ECDSA w/ SHA-256)
                Map.of("type", "public-key", "alg", -257) // RS256 (RSA w/ SHA-256)
            ),
            "authenticatorSelection", Map.of(
                "userVerification", "preferred",
                "residentKey", "required" // Required for discoverable credentials (passkeys)
            ),
            "timeout", 60000
        );

        return ResponseEntity.ok(options);
    }
}
```

### Example 3: Edge Case – Cloned Authenticator Detection via `signCount`
Detecting compromised or cloned authenticators by validating signature counters.

```java
package com.example.security.service;

import com.example.security.domain.UserPasskey;
import com.example.security.repository.UserPasskeyRepository;
import org.springframework.security.authentication.BadCredentialsException;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

@Service
public class PasskeyVerificationService {

    private final UserPasskeyRepository passkeyRepository;

    public PasskeyVerificationService(UserPasskeyRepository passkeyRepository) {
        this.passkeyRepository = passkeyRepository;
    }

    @Transactional
    public void verifySignatureCounter(String credentialId, long incomingSignCount) {
        UserPasskey passkey = passkeyRepository.findByCredentialId(credentialId)
            .orElseThrow(() -> new BadCredentialsException("Unknown passkey credential ID"));

        // If the authenticator supports counters (counter > 0), incoming MUST exceed stored!
        if (incomingSignCount > 0 && passkey.getSignatureCounter() > 0) {
            if (incomingSignCount <= passkey.getSignatureCounter()) {
                // CLONED AUTHENTICATOR DETECTED: Counter rollback or replay attack!
                // Revoke passkey immediately and alert security operations!
                passkeyRepository.delete(passkey);
                throw new BadCredentialsException("SECURITY ALERT: Authenticator cloning detected. Passkey revoked.");
            }
        }

        passkey.setSignatureCounter(incomingSignCount);
        passkeyRepository.save(passkey);
    }
}
```

### Example 4: Failure Anti-Pattern & Production Fix
**Anti-Pattern**: Omitting `rpId` configuration or setting it to a subpath (`example.com/api`), causing browser WebAuthn API calls to fail with `SecurityError: The Relying Party ID is not a valid domain string`.

```java
// BUGGY: rpId cannot contain protocol, port, or path!
.webAuthn(webAuthn -> webAuthn
    .rpId("https://example.com:8080/api") // FAILS BROWSER CHECKS!
)
```

**Production Fix**:
Configure `rpId` strictly as the registrable domain or subdomain without protocol or ports.

```java
// SECURE: Valid domain string matching window.location.hostname
.webAuthn(webAuthn -> webAuthn
    .rpId("example.com") // Matches example.com and any *.example.com subdomain
    .allowedOrigins("https://example.com", "https://app.example.com")
)
```

### Example 5: High-Performance Non-Blocking WebFlux Passkey Exchange
Supporting non-blocking passkey assertion verification in Spring Boot WebFlux.

```java
package com.example.security.reactive;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.security.config.annotation.web.reactive.EnableWebFluxSecurity;
import org.springframework.security.config.web.server.ServerHttpSecurity;
import org.springframework.security.web.server.SecurityWebFilterChain;

@Configuration
@EnableWebFluxSecurity
public class ReactivePasskeySecurityConfig {

    @Bean
    public SecurityWebFilterChain reactivePasskeyChain(ServerHttpSecurity http) {
        return http
            .authorizeExchange(exchanges -> exchanges
                .pathMatchers("/", "/login", "/webauthn/**").permitAll()
                .anyExchange().authenticated()
            )
            .formLogin(ServerHttpSecurity.FormLoginSpec::disable)
            .httpBasic(ServerHttpSecurity.HttpBasicSpec::disable)
            .build();
    }
}
```

---

## 6. Best Practices

1. **Mandate Resident Keys for Passwordless**: Configure `residentKey: "required"` (discoverable credentials) to enable 1-click passwordless logins where users don't even type their username.
2. **Strictly Validate Challenge Nonces**: Never reuse challenges. Always generate at least 32 cryptographically secure random bytes and expire challenges after 60 seconds.
3. **Verify Domain Origins Strictly**: Check that the `origin` inside `clientDataJSON` matches your application's exact HTTPS origin.
4. **Enforce `UserVerification` for Sensitive Operations**: Require `userVerification: "required"` (biometric check or device PIN) rather than mere presence (button press).
5. **Support Multiple Credentials per User**: Allow users to register multiple passkeys (e.g., iPhone FaceID + MacBook TouchID + Backup YubiKey) to prevent lockout if a device is lost.
6. **Track and Enforce `signCount`**: Monitor signature counters to detect cloned hardware authenticators.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Invalid `rpId` format | Setting `https://example.com` or `localhost:8080` causes browser `DOMException: SecurityError`. | Set `rpId` to bare hostname (e.g., `example.com` or `localhost`). |
| Reusing registration challenges | Attackers replay intercepted attestation packages. | Store challenges in short-lived sessions and delete upon first evaluation. |
| Neglecting COSE algorithm IDs | Client authenticators fail registration if required algorithms (`-7` ES256, `-257` RS256) are missing. | Always publish support for both `ES256` and `RS256` in `pubKeyCredParams`. |
| Disregarding User Verification | An attacker with physical access to an unlocked laptop can authorize actions without a biometric prompt. | Configure `userVerification: "required"`. |
| Single passkey user lockout | User loses their phone and has no fallback credential registered. | Provide account recovery flows or encourage registering backup security keys. |
| Failing on `signCount == 0` | Many synced passkeys (iCloud Keychain, Google Password Manager) do not increment counters and return `0`. | Only validate counter increases if `incomingCounter > 0` and `storedCounter > 0`. |

---

## 8. Performance & Security Notes

- **Zero Database Credential Risk**: Even if your database is breached, the attacker only acquires public keys and credential IDs. Without the private key inside the user's hardware secure enclave, no account can be compromised.
- **Phishing Immunity**: If an attacker sets up a clone site at `https://evil-bank.com`, the user's browser automatically uses `evil-bank.com` as the origin. The signature fails verification on `bank.com`, completely neutralizing phishing attacks.
- **CPU Overhead of Asymmetric Signatures**: WebAuthn assertions rely on standard ECDSA P-256 or RSA signatures. Verification takes less than 1ms per login, which has negligible impact on modern CPU architectures.

---

## 9. Comparison / When to Use What

| Mechanism | Phishing Resistant? | Hardware Bound? | Friction | Best Use Case |
|---|---|---|---|---|
| **Passkey (FIDO2/WebAuthn)**| **Yes (Cryptographically bound)**| Yes (Secure Enclave / TPM) | Very Low (Biometrics) | Modern web/mobile authentication |
| **TOTP Authenticator App**| No (Vulnerable to AiTM proxies)| No (Software secret) | Medium (Typing codes) | Legacy 2FA fallback |
| **SMS / Email OTP** | No (SIM swap / Phishing) | No | High | Low-security notification verification |
| **Passwords + BCrypt** | No (Vulnerable to all attacks) | No | High | Legacy systems (Phase out) |

---

## 10. Interview Questions

<details>
<summary>1. Why are Passkeys immune to phishing attacks while TOTP codes are not?</summary>

**Answer:**
TOTP codes are shared secrets typed into a browser form; an attacker using a reverse proxy (e.g., Evilginx) can intercept the 6-digit code and replay it against the real server in real-time.
Passkeys use public-key cryptography where the browser generates the signature. The browser automatically injects the verified top-level origin (`window.location.origin`) into the signed client data. If the user is on `phishing-site.com`, the signature is calculated for `phishing-site.com`. When the phishing site forwards this to `real-site.com`, the real server rejects it because the signed origin does not match the expected `rpId`.
</details>

<details>
<summary>2. What is the difference between User Presence (UP) and User Verification (UV)?</summary>

**Answer:**
- **User Presence (UP)**: Proves that a human was physically present and interacted with the device (e.g., touching the gold contact on a YubiKey or clicking "Continue"). It does not verify identity.
- **User Verification (UV)**: Proves that the authorized user verified their identity via a biometric sensor (TouchID, FaceID) or local device PIN.
</details>

<details>
<summary>3. What is a "Discoverable Credential" (Resident Key)?</summary>

**Answer:**
A discoverable credential (resident key) is a passkey where the private key and the user's account identifier are stored directly inside the authenticator's secure hardware. During login, the user does not need to enter a username; the authenticator displays a list of accounts matching the `rpId`, allowing true 1-click passwordless authentication.
</details>

<details>
<summary>4. What is the purpose of the signature counter (`signCount`) in WebAuthn?</summary>

**Answer:**
The signature counter helps detect cloned authenticators. Hardware tokens increment an internal counter every time an assertion is signed. If the server receives an incoming assertion with a counter less than or equal to the previously recorded counter, it indicates that a clone of the private key has been made, alerting the system to revoke the credential.
</details>

<details>
<summary>5. What are the COSE algorithm identifiers `-7` and `-257`?</summary>

**Answer:**
COSE (CBOR Object Signing and Encryption) defines numerical algorithm identifiers:
- `-7`: `ES256` (ECDSA using the P-256 curve and SHA-256 hash). Standard for mobile devices (Apple TouchID/FaceID, Android biometrics).
- `-257`: `RS256` (RSASSA-PKCS1-v1_5 using SHA-256). Common for Windows Hello and enterprise hardware tokens.
</details>

<details>
<summary>6. How does Apple iCloud Keychain sync passkeys securely across devices?</summary>

**Answer:**
Apple uses end-to-end encrypted hardware synchronization. Passkey private keys are encrypted on the device using keys derived from the user's iCloud Keychain credentials and Apple ID hardware elements. Apple cannot read the keys, but they synchronize transparently between the user's iPhone, iPad, and Mac.
</details>

<details>
<summary>7. What is an Attestation Statement in WebAuthn registration?</summary>

**Answer:**
An attestation statement is a cryptographically signed document from the authenticator's manufacturer (e.g., Yubico) proving that the newly generated key pair was created inside genuine, certified hardware. Enterprise deployments use attestation to mandate FIPS-compliant hardware tokens.
</details>

<details>
<summary>8. How do you handle account recovery if a user loses all registered passkey devices?</summary>

**Answer:**
Since private keys cannot be recovered by the server, applications must implement recovery mechanisms:
1. Registering multiple passkeys at onboarding (e.g., primary phone + backup laptop/YubiKey).
2. Generating one-time encrypted recovery codes stored offline.
3. Fallback verification via verified secondary channels (e.g., corporate identity verification or helpdesk workflows).
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Basic Challenge Generator (Easy)</summary>

**Task**: Write a utility method that generates a 32-byte cryptographically secure random challenge and returns it as a Base64URL string without padding.

**Solution**:
```java
public String generateChallenge() {
    byte[] bytes = new byte[32];
    new SecureRandom().nextBytes(bytes);
    return Base64.getUrlEncoder().withoutPadding().encodeToString(bytes);
}
```
</details>

<details>
<summary>Exercise 2: COSE Algorithm Configuration (Easy)</summary>

**Task**: Create a Java map representing the `pubKeyCredParams` list advertising support for ES256 (`-7`) and RS256 (`-257`).

**Solution**:
```java
public List<Map<String, Object>> getSupportedAlgorithms() {
    return List.of(
        Map.of("type", "public-key", "alg", -7),  // ES256
        Map.of("type", "public-key", "alg", -257) // RS256
    );
}
```
</details>

<details>
<summary>Exercise 3: Origin Verification Check (Medium)</summary>

**Task**: Implement a method that validates whether an incoming origin string matches the permitted HTTPS domain or its trusted subdomains.

**Solution**:
```java
public boolean isOriginPermitted(String clientOrigin, List<String> permittedOrigins) {
    if (clientOrigin == null || !clientOrigin.startsWith("https://")) {
        return false;
    }
    return permittedOrigins.contains(clientOrigin);
}
```
</details>

<details>
<summary>Exercise 4: Signature Counter Validator (Medium)</summary>

**Task**: Implement a method that validates signature counter progression and flags an error if a counter rollback occurs on non-zero values.

**Solution**:
```java
public boolean validateCounter(long storedCount, long incomingCount) {
    if (storedCount > 0 && incomingCount > 0) {
        return incomingCount > storedCount; // Must strictly increase
    }
    return true; // Passkey sync services (e.g., Apple) may omit counters
}
```
</details>

<details>
<summary>Exercise 5: Spring Security 6.4 WebAuthn Filter Configuration (Hard)</summary>

**Task**: Configure a `SecurityFilterChain` with native `webAuthn()` support, configuring `rpId`, `rpName`, allowed origins, and permitting all `/webauthn/**` paths.

**Solution**:
```java
@Bean
public SecurityFilterChain webAuthnChain(HttpSecurity http) throws Exception {
    return http
        .authorizeHttpRequests(auth -> auth
            .requestMatchers("/webauthn/**", "/login").permitAll()
            .anyRequest().authenticated()
        )
        .webAuthn(webAuthn -> webAuthn
            .rpId("company.com")
            .rpName("Corporate Portal")
            .allowedOrigins("https://company.com", "https://app.company.com")
        )
        .build();
}
```
</details>

<details>
<summary>Exercise 6: MockMvc Test for WebAuthn Options Endpoint (Hard)</summary>

**Task**: Write an integration test using MockMvc verifying that calling `/api/webauthn/register/options` returns a valid challenge, correct `rpId`, and stores the challenge in the HTTP session.

**Solution**:
```java
@WebMvcTest(controllers = PasskeyRegistrationController.class)
@Import(TestSecurityConfig.class)
class PasskeyRegistrationTest {

    @Autowired
    private MockMvc mockMvc;

    @Test
    void shouldReturnRegistrationOptionsAndSaveSessionChallenge() throws Exception {
        mockMvc.perform(post("/api/webauthn/register/options")
                .param("username", "alice"))
            .andExpect(status().isOk())
            .andExpect(jsonPath("$.challenge").isString())
            .andExpect(jsonPath("$.rp.id").value("example.com"))
            .andExpect(request().sessionAttribute("WEBAUTHN_USER", "alice"));
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **Phishing Resistant**: Browsers bind credentials directly to the exact domain origin (`rpId`), defeating spoofing and proxy attacks.
- **No Shared Secrets**: The server stores only public keys; private keys remain permanently inside client hardware secure enclaves.
- **Passkeys are Discoverable**: Use `residentKey: "required"` to enable 1-click passwordless authentication.
- **Track Signature Counters**: Inspect `signCount` to detect cloned authenticators.
- **Spring Security 6.4+ Support**: Native `http.webAuthn(...)` DSL standardizes passkey adoption without third-party frameworks.

---

## 13. Further Reading
- [W3C Web Authentication: An API for accessing Public Key Credentials Level 2](https://www.w3.org/TR/webauthn-2/)
- [FIDO Alliance Passkey Guidelines](https://fidoalliance.org/passkeys/)
- [Spring Security Reference: WebAuthn](https://docs.spring.io/spring-security/reference/servlet/authentication/passkeys.html)
- [WebAuthn.io: Interactive Developer Sandbox](https://webauthn.io/)
- [NIST SP 800-63B: Digital Identity Guidelines - Authentication and Lifecycle Management](https://pages.nist.gov/800-63-3/sp800-63b.html)
