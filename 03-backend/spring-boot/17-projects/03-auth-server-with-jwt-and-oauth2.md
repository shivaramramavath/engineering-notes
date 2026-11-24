# Production Reference Architecture: Auth Server with JWT & OAuth2

## Metadata & Production Baseline

| Attribute | Specification |
|:---|:---|
| **Project Code** | `SPRING-PROJ-003` |
| **Spring Boot Version** | `3.4.x` / `4.x` |
| **Spring Security Version** | `6.4.x` / `7.0.x` |
| **Java Baseline** | `Java 21` / `Java 25 LTS` |
| **Build Tool** | `Maven 3.9+` |
| **Identity & Storage** | `PostgreSQL 16+`, `Spring Data JPA`, `Redis 7+` (Blocklist & Refresh Tokens) |
| **Cryptography** | `Argon2id` (Password hashing), `RSA 2048` / `ECDSA` (JWT signing) |
| **Testing** | `JUnit 5.11+`, `Testcontainers 1.20+`, `MockMvc` |
| **Complexity Level** | Intermediate to Advanced |

---

## 1. Architectural Overview & Domain Design

### 1.1 System Architecture & Data Flow

```
                      +---------------------------------------+
                      |         Client / Single Page App      |
                      +-------------------+-------------------+
                                          |
                                          | 1. POST /api/v1/auth/login
                                          v
                      +---------------------------------------+
                      |        AuthenticationController       |
                      +-------------------+-------------------+
                                          |
                                          | 2. Validates Argon2 Password
                                          v
                      +---------------------------------------+
                      |       AuthenticationManager (JPA)     |
                      +-------------------+-------------------+
                                          |
                          +---------------+---------------+
                          |                               |
                          | 3. Emits Access Token (JWT)   | 4. Stores Refresh Token
                          v                               v
             +-------------------------+     +-------------------------+
             |   Signed JWT (Stateless)|     |   Redis Key-Value Store |
             |   RS256 / 15m expiration|     |   7-Day TTL + Blocklist |
             +-------------------------+     +-------------------------+
```

### 1.2 Directory Layout

```
03-auth-server-with-jwt-and-oauth2/
|-- pom.xml
|-- Dockerfile
|-- compose.yaml
`-- src/
    |-- main/
    |   |-- java/com/example/auth/
    |   |   |-- AuthApplication.java
    |   |   |-- config/
    |   |   |   |-- SecurityConfig.java
    |   |   |   `-- RsaKeyProperties.java
    |   |   |-- domain/
    |   |   |   |-- User.java
    |   |   |   `-- Role.java
    |   |   |-- dto/
    |   |   |   |-- LoginRequest.java
    |   |   |   |-- TokenResponse.java
    |   |   |   `-- RefreshTokenRequest.java
    |   |   |-- service/
    |   |   |   |-- JwtTokenService.java
    |   |   |   `-- UserService.java
    |   |   `-- web/
    |   |       `-- AuthController.java
    |   `-- resources/
    |       |-- application.yaml
    |       |-- certs/
    |       |   |-- public.pem
    |       |   `-- private.pem
    |       `-- db/migration/
    |           `-- V1__init_auth_schema.sql
    `-- test/
        `-- java/com/example/auth/
            `-- AuthIntegrationTest.java
```

---

## 2. Complete Production Implementation

### 2.1 Build Configuration (`pom.xml`)

```xml
<?xml version="1.0" encoding="UTF-8"?>
<project xmlns="http://maven.apache.org/POM/4.0.0"
         xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
         xsi:schemaLocation="http://maven.apache.org/POM/4.0.0 https://maven.apache.org/xsd/maven-4.0.0.xsd">
    <modelVersion>4.0.0</modelVersion>

    <parent>
        <groupId>org.springframework.boot</groupId>
        <artifactId>spring-boot-starter-parent</artifactId>
        <version>3.4.1</version>
        <relativePath/>
    </parent>

    <groupId>com.example</groupId>
    <artifactId>auth-server-jwt</artifactId>
    <version>1.0.0</version>
    <name>auth-server-jwt</name>

    <properties>
        <java.version>21</java.version>
    </properties>

    <dependencies>
        <!-- Security & OAuth2 Resource Server -->
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-security</artifactId>
        </dependency>
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-oauth2-resource-server</artifactId>
        </dependency>
        <dependency>
            <groupId>org.bouncycastle</groupId>
            <artifactId>bcprov-jdk18on</artifactId>
            <version>1.78.1</version>
        </dependency>

        <!-- Web & Data -->
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-web</artifactId>
        </dependency>
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-validation</artifactId>
        </dependency>
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-data-jpa</artifactId>
        </dependency>
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-data-redis</artifactId>
        </dependency>
        <dependency>
            <groupId>org.postgresql</groupId>
            <artifactId>postgresql</artifactId>
            <scope>runtime</scope>
        </dependency>
        <dependency>
            <groupId>org.flywaydb</groupId>
            <artifactId>flyway-core</artifactId>
        </dependency>
        <dependency>
            <groupId>org.flywaydb</groupId>
            <artifactId>flyway-database-postgresql</artifactId>
        </dependency>

        <!-- Testing -->
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-test</artifactId>
            <scope>test</scope>
        </dependency>
        <dependency>
            <groupId>org.springframework.security</groupId>
            <artifactId>spring-security-test</artifactId>
            <scope>test</scope>
        </dependency>
        <dependency>
            <groupId>org.testcontainers</groupId>
            <artifactId>postgresql</artifactId>
            <scope>test</scope>
        </dependency>
    </dependencies>

    <build>
        <plugins>
            <plugin>
                <groupId>org.springframework.boot</groupId>
                <artifactId>spring-boot-maven-plugin</artifactId>
            </plugin>
        </plugins>
    </build>
</project>
```

### 2.2 Security Configuration & RSA Key Management

```java
package com.example.auth.config;

import com.nimbusds.jose.jwk.JWK;
import com.nimbusds.jose.jwk.JWKSet;
import com.nimbusds.jose.jwk.RSAKey;
import com.nimbusds.jose.jwk.source.ImmutableJWKSet;
import com.nimbusds.jose.jwk.source.JWKSource;
import com.nimbusds.jose.proc.SecurityContext;
import org.springframework.boot.context.properties.ConfigurationProperties;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.security.authentication.AuthenticationManager;
import org.springframework.security.authentication.ProviderManager;
import org.springframework.security.authentication.dao.DaoAuthenticationProvider;
import org.springframework.security.config.annotation.method.configuration.EnableMethodSecurity;
import org.springframework.security.config.annotation.web.builders.HttpSecurity;
import org.springframework.security.config.annotation.web.configuration.EnableWebSecurity;
import org.springframework.security.config.http.SessionCreationPolicy;
import org.springframework.security.core.userdetails.UserDetailsService;
import org.springframework.security.crypto.argon2.Argon2PasswordEncoder;
import org.springframework.security.crypto.password.PasswordEncoder;
import org.springframework.security.oauth2.jwt.JwtDecoder;
import org.springframework.security.oauth2.jwt.JwtEncoder;
import org.springframework.security.oauth2.jwt.NimbusJwtDecoder;
import org.springframework.security.oauth2.jwt.NimbusJwtEncoder;
import org.springframework.security.web.SecurityFilterChain;

import java.security.interfaces.RSAPrivateKey;
import java.security.interfaces.RSAPublicKey;

@ConfigurationProperties(prefix = "rsa")
record RsaKeyProperties(RSAPublicKey publicKey, RSAPrivateKey privateKey) {}

@Configuration
@EnableWebSecurity
@EnableMethodSecurity
public class SecurityConfig {

    private final RsaKeyProperties rsaKeys;

    public SecurityConfig(RsaKeyProperties rsaKeys) {
        this.rsaKeys = rsaKeys;
    }

    @Bean
    public PasswordEncoder passwordEncoder() {
        // Industry gold-standard password hashing: Argon2id
        return Argon2PasswordEncoder.defaultsForSpringSecurity_v5_8();
    }

    @Bean
    public AuthenticationManager authenticationManager(UserDetailsService userDetailsService, PasswordEncoder passwordEncoder) {
        DaoAuthenticationProvider authProvider = new DaoAuthenticationProvider();
        authProvider.setUserDetailsService(userDetailsService);
        authProvider.setPasswordEncoder(passwordEncoder);
        return new ProviderManager(authProvider);
    }

    @Bean
    public SecurityFilterChain securityFilterChain(HttpSecurity http) throws Exception {
        return http
            .csrf(csrf -> csrf.disable())
            .sessionManagement(sm -> sm.sessionCreationPolicy(SessionCreationPolicy.STATELESS))
            .authorizeHttpRequests(auth -> auth
                .requestMatchers("/api/v1/auth/**", "/actuator/health/**").permitAll()
                .anyRequest().authenticated()
            )
            .oauth2ResourceServer(oauth2 -> oauth2.jwt(jwt -> jwt.decoder(jwtDecoder())))
            .build();
    }

    @Bean
    public JwtDecoder jwtDecoder() {
        return NimbusJwtDecoder.withPublicKey(rsaKeys.publicKey()).build();
    }

    @Bean
    public JwtEncoder jwtEncoder() {
        JWK jwk = new RSAKey.Builder(rsaKeys.publicKey()).privateKey(rsaKeys.privateKey()).build();
        JWKSource<SecurityContext> jwkSource = new ImmutableJWKSet<>(new JWKSet(jwk));
        return new NimbusJwtEncoder(jwkSource);
    }
}
```

### 2.3 JWT Generation & Refresh Token Service

```java
package com.example.auth.service;

import org.springframework.data.redis.core.StringRedisTemplate;
import org.springframework.security.core.Authentication;
import org.springframework.security.core.GrantedAuthority;
import org.springframework.security.oauth2.jwt.JwtClaimsSet;
import org.springframework.security.oauth2.jwt.JwtEncoder;
import org.springframework.security.oauth2.jwt.JwtEncoderParameters;
import org.springframework.stereotype.Service;

import java.time.Duration;
import java.time.Instant;
import java.time.temporal.ChronoUnit;
import java.util.UUID;
import java.util.stream.Collectors;

public record TokenPair(String accessToken, String refreshToken, long expiresInSeconds) {}

@Service
public class JwtTokenService {

    private final JwtEncoder encoder;
    private final StringRedisTemplate redisTemplate;

    public JwtTokenService(JwtEncoder encoder, StringRedisTemplate redisTemplate) {
        this.encoder = encoder;
        this.redisTemplate = redisTemplate;
    }

    public TokenPair generateTokens(Authentication authentication) {
        Instant now = Instant.now();
        String scope = authentication.getAuthorities().stream()
                .map(GrantedAuthority::getAuthority)
                .collect(Collectors.joining(" "));

        // 1. Generate Stateless Access Token (15-minute expiration)
        JwtClaimsSet claims = JwtClaimsSet.builder()
                .issuer("https://auth.example.com")
                .issuedAt(now)
                .expiresAt(now.plus(15, ChronoUnit.MINUTES))
                .subject(authentication.getName())
                .claim("scope", scope)
                .build();

        String accessToken = this.encoder.encode(JwtEncoderParameters.from(claims)).getTokenValue();

        // 2. Generate Refresh Token and store in Redis with 7-day TTL
        String refreshToken = UUID.randomUUID().toString();
        redisTemplate.opsForValue().set("refresh:" + refreshToken, authentication.getName(), Duration.ofDays(7));

        return new TokenPair(accessToken, refreshToken, 900);
    }

    public void revokeRefreshToken(String refreshToken) {
        redisTemplate.delete("refresh:" + refreshToken);
    }
}
```

### 2.4 REST Controller

```java
package com.example.auth.web;

import com.example.auth.service.JwtTokenService;
import com.example.auth.service.TokenPair;
import jakarta.validation.Valid;
import jakarta.validation.constraints.NotBlank;
import org.springframework.http.ResponseEntity;
import org.springframework.security.authentication.AuthenticationManager;
import org.springframework.security.authentication.UsernamePasswordAuthenticationToken;
import org.springframework.security.core.Authentication;
import org.springframework.web.bind.annotation.*;

public record LoginRequest(@NotBlank String username, @NotBlank String password) {}
public record RefreshRequest(@NotBlank String refreshToken) {}

@RestController
@RequestMapping("/api/v1/auth")
public class AuthController {

    private final AuthenticationManager authManager;
    private final JwtTokenService tokenService;

    public AuthController(AuthenticationManager authManager, JwtTokenService tokenService) {
        this.authManager = authManager;
        this.tokenService = tokenService;
    }

    @PostMapping("/login")
    public ResponseEntity<TokenPair> login(@Valid @RequestBody LoginRequest request) {
        Authentication authentication = authManager.authenticate(
            new UsernamePasswordAuthenticationToken(request.username(), request.password())
        );
        return ResponseEntity.ok(tokenService.generateTokens(authentication));
    }

    @PostMapping("/logout")
    public ResponseEntity<Void> logout(@Valid @RequestBody RefreshRequest request) {
        tokenService.revokeRefreshToken(request.refreshToken());
        return ResponseEntity.noContent().build();
    }
}
```

---

## 3. Automated Testing Suite

```java
package com.example.auth;

import com.example.auth.web.LoginRequest;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.boot.test.web.client.TestRestTemplate;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;

import static org.assertj.core.api.Assertions.assertThat;

@SpringBootTest(webEnvironment = SpringBootTest.WebEnvironment.RANDOM_PORT)
class AuthIntegrationTest {

    @Autowired private TestRestTemplate restTemplate;

    @Test
    void shouldRejectInvalidCredentials() {
        LoginRequest badLogin = new LoginRequest("nonexistent_user", "wrong_password");

        ResponseEntity<String> response = restTemplate.postForEntity(
                "/api/v1/auth/login", badLogin, String.class
        );

        assertThat(response.getStatusCode()).isEqualTo(HttpStatus.UNAUTHORIZED);
    }
}
```

---

## 4. Production Infrastructure & Deployment

### 4.1 Multi-Stage `Dockerfile`

```dockerfile
FROM eclipse-temurin:21-jdk-alpine AS builder
WORKDIR /app
COPY pom.xml .
COPY src src
RUN ./mvnw clean package -DskipTests

FROM eclipse-temurin:21-jre-alpine
WORKDIR /app
RUN addgroup -S authgrp && adduser -S authusr -G authgrp
USER authusr:authgrp
COPY --from=builder /app/target/*.jar app.jar
ENV JAVA_TOOL_OPTIONS="-XX:+UseG1GC -XX:MaxRAMPercentage=75.0"
EXPOSE 8080
ENTRYPOINT ["java", "-jar", "app.jar"]
```

### 4.2 `compose.yaml`

```yaml
services:
  auth-service:
    build: .
    ports:
      - "8080:8080"
    environment:
      - SPRING_DATASOURCE_URL=jdbc:postgresql://postgres-auth:5432/auth_db
      - SPRING_DATA_REDIS_HOST=redis-auth
      - RSA_PUBLIC_KEY=classpath:certs/public.pem
      - RSA_PRIVATE_KEY=classpath:certs/private.pem
    depends_on:
      - postgres-auth
      - redis-auth

  postgres-auth:
    image: postgres:16-alpine
    environment:
      - POSTGRES_DB=auth_db
      - POSTGRES_USER=auth_user
      - POSTGRES_PASSWORD=auth_pass

  redis-auth:
    image: redis:7.2-alpine
    ports:
      - "6379:6379"
```

---

## 5. Operational Runbook & Verification

```bash
# 1. Login and receive JWT access + refresh tokens
curl -X POST http://localhost:8080/api/v1/auth/login \
     -H "Content-Type: application/json" \
     -d '{"username": "admin@example.com", "password": "SuperSecretPassword123!"}' | jq .

# 2. Access protected endpoint with Bearer token
curl -H "Authorization: Bearer <access_token>" http://localhost:8080/api/v1/protected-data
```
