# 10. OpenID Connect & Keycloak Integration in Spring Boot

| Metadata | Value |
|---|---|
| **Level** | Advanced |
| **Prerequisites** | `01-spring-security-architecture.md`, `07-JWT-resource-server.md`, `08-oauth2-client-and-social-login.md`, Keycloak Basics |
| **Estimated Reading Time** | 35 minutes |
| **Status** | Complete |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | OpenJDK 21, Spring Boot 3.4.1, Keycloak 26 (Quarkus distribution) |

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
Enterprise identity demands sophisticated features that go far beyond basic username/password authentication: Single Sign-On (SSO), multi-factor authentication (MFA/TOTP), Kerberos/LDAP federation, social identity bridging, fine-grained user management consoles, and automated token revocation. Building these features in-house diverts immense engineering effort away from core business domains.

**Red Hat Keycloak** is the industry-standard, open-source identity and access management (IAM) platform. However, integrating Keycloak with modern Spring Boot applications presents distinct challenges:
- Older tutorials recommend deprecated Keycloak Spring Boot adapters (which were officially retired in Keycloak 19+).
- Keycloak structures roles in nested JSON claims (`realm_access` and `resource_access`), which standard Spring Security converters fail to parse.
- Applications often need to function simultaneously as an **OAuth2 Client** (for browser-based SSO login) and a **JWT Resource Server** (for API token verification).

### Historical Evolution
- **Keycloak 1.x - 18.x (Legacy)**: Provided proprietary `keycloak-spring-boot-starter` and `KeycloakWebSecurityConfigurerAdapter`. Heavily coupled to Tomcat/Undertow internals.
- **Keycloak 19+ & Spring Boot 3.x/4.x**: Red Hat completely deprecated and eliminated proprietary adapters in favor of **standard Spring Security OAuth 2.0 / OpenID Connect starters** (`spring-boot-starter-oauth2-resource-server` and `spring-boot-starter-oauth2-client`).
- **Keycloak 26+ (Quarkus-based)**: Fully standardized on OIDC Core 1.0, RFC 7519, and RFC 8414.

### Real-World Relevance
Integrating Spring Boot with Keycloak is the dominant enterprise architecture across banking, government, telecom, and healthcare sectors, providing hardened, sovereign single sign-on without vendor lock-in.

---

## 2. Core Concepts

### Architectural Contracts
- **Keycloak Realm**: An isolated domain of identities, applications, roles, and groups. An application authenticates against a specific realm endpoint: `http://keycloak:8080/realms/{realm-name}`.
- **Client**: An entity (microservice or frontend) registered in Keycloak with specific protocol settings (Bearer-Only, Confidential, or Public).
- **Realm Roles vs Client Roles**:
  - *Realm Roles*: Global permissions across the entire realm (e.g., `realm_access.roles: ["SUPER_ADMIN"]`).
  - *Client Roles*: Scoped permissions restricted to a specific application (e.g., `resource_access.order-service.roles: ["ORDER_CREATOR"]`).
- **OpenID Connect Discovery**: Keycloak automatically exposes metadata at `/.well-known/openid-configuration`, allowing Spring Boot to discover JWKS URIs, token endpoints, and issuer URLs dynamically.

### The Mental Model
```
[ Browser / User ] ──► 1. Login via Keycloak SSO (/realms/corp/protocol/openid-connect/auth)
       │
       ├── Keycloak authenticates user (Form / MFA / LDAP)
       ├── Issues ID Token (user info) + Access Token (JWT)
       │
       ▼
[ Spring Boot API (Resource Server) ] ◄── Bearer <JWT>
       │
       ├── Discovers JWKS from Keycloak (/realms/corp/protocol/openid-connect/certs)
       ├── Validates Signature (RS256) & Issuer
       ├── KeycloakAuthoritiesConverter extracts:
       │     realm_access.roles ──► ROLE_ADMIN
       │     resource_access.order-service.roles ──► ROLE_ORDER_CREATOR
       │
       ▼
[ SecurityContextHolder ] ──► Authorized Controller Invocation
```

---

## 3. How It Works Internally

### Role Extraction & Claim Anatomy
A standard Keycloak access token contains the following JSON structure:
```json
{
  "exp": 1700000000,
  "iat": 1699996400,
  "iss": "http://localhost:8080/realms/enterprise",
  "sub": "b2c6e881-3091-4cf4-91f8-0853dc47f978",
  "preferred_username": "john.doe",
  "email": "john.doe@company.com",
  "realm_access": {
    "roles": [
      "default-roles-enterprise",
      "offline_access",
      "admin"
    ]
  },
  "resource_access": {
    "order-service": {
      "roles": [
        "order-read",
        "order-write"
      ]
    }
  },
  "scope": "openid email profile"
}
```

Standard Spring Security only parses `scope`. Without a custom converter, `admin` and `order-write` are completely invisible to Spring's authorization engine (`hasRole("ADMIN")`).

---

## 4. Syntax & Usage

### 1. Starter Dependencies (`pom.xml`)
No proprietary Keycloak JARs are required! Use standard Spring Security:

```xml
<dependencies>
    <dependency>
        <groupId>org.springframework.boot</groupId>
        <artifactId>spring-boot-starter-security</artifactId>
    </dependency>
    <dependency>
        <groupId>org.springframework.boot</groupId>
        <artifactId>spring-boot-starter-oauth2-resource-server</artifactId>
    </dependency>
</dependencies>
```

### 2. Configuration (`application.yml`)
```yaml
spring:
  security:
    oauth2:
      resourceserver:
        jwt:
          issuer-uri: http://localhost:8080/realms/enterprise
          jwk-set-uri: http://localhost:8080/realms/enterprise/protocol/openid-connect/certs
```

---

## 5. Practical Examples

### Example 1: Robust Keycloak Combined Realm and Client Role Converter
A production converter that traverses both `realm_access` and `resource_access` structures, sanitizes role names, and prepends `ROLE_`.

```java
package com.example.security.keycloak;

import org.springframework.beans.factory.annotation.Value;
import org.springframework.core.convert.converter.Converter;
import org.springframework.security.authentication.AbstractAuthenticationToken;
import org.springframework.security.core.GrantedAuthority;
import org.springframework.security.core.authority.SimpleGrantedAuthority;
import org.springframework.security.oauth2.jwt.Jwt;
import org.springframework.security.oauth2.server.resource.authentication.JwtAuthenticationToken;
import org.springframework.security.oauth2.server.resource.authentication.JwtGrantedAuthoritiesConverter;
import org.springframework.stereotype.Component;

import java.util.*;
import java.util.stream.Collectors;
import java.util.stream.Stream;

@Component
public class KeycloakJwtAuthenticationConverter implements Converter<Jwt, AbstractAuthenticationToken> {

    private final JwtGrantedAuthoritiesConverter defaultAuthoritiesConverter = new JwtGrantedAuthoritiesConverter();

    @Value("${application.security.client-id:order-service}")
    private String resourceClientId;

    @Override
    public AbstractAuthenticationToken convert(Jwt jwt) {
        Collection<GrantedAuthority> authorities = Stream.concat(
            defaultAuthoritiesConverter.convert(jwt).stream(), // Maps SCOPE_ claims
            extractKeycloakRoles(jwt).stream()                 // Maps Keycloak ROLE_ claims
        ).collect(Collectors.toSet());

        String principalName = jwt.getClaimAsString("preferred_username");
        if (principalName == null) {
            principalName = jwt.getSubject();
        }

        return new JwtAuthenticationToken(jwt, authorities, principalName);
    }

    @SuppressWarnings("unchecked")
    private Collection<GrantedAuthority> extractKeycloakRoles(Jwt jwt) {
        Set<GrantedAuthority> roles = new HashSet<>();

        // 1. Extract Realm Roles
        Map<String, Object> realmAccess = jwt.getClaimAsMap("realm_access");
        if (realmAccess != null && realmAccess.containsKey("roles")) {
            List<String> realmRoles = (List<String>) realmAccess.get("roles");
            realmRoles.stream()
                .map(r -> new SimpleGrantedAuthority("ROLE_" + r.replace("-", "_").toUpperCase()))
                .forEach(roles::add);
        }

        // 2. Extract Client Specific Roles
        Map<String, Object> resourceAccess = jwt.getClaimAsMap("resource_access");
        if (resourceAccess != null && resourceAccess.containsKey(resourceClientId)) {
            Map<String, Object> clientAccess = (Map<String, Object>) resourceAccess.get(resourceClientId);
            if (clientAccess != null && clientAccess.containsKey("roles")) {
                List<String> clientRoles = (List<String>) clientAccess.get("roles");
                clientRoles.stream()
                    .map(r -> new SimpleGrantedAuthority("ROLE_" + r.replace("-", "_").toUpperCase()))
                    .forEach(roles::add);
            }
        }

        return roles;
    }
}
```

### Example 2: Complete Spring Security Configuration
Wiring the Keycloak converter into the `SecurityFilterChain`.

```java
package com.example.security.config;

import com.example.security.keycloak.KeycloakJwtAuthenticationConverter;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.security.config.annotation.method.configuration.EnableMethodSecurity;
import org.springframework.security.config.annotation.web.builders.HttpSecurity;
import org.springframework.security.config.annotation.web.configuration.EnableWebSecurity;
import org.springframework.security.config.http.SessionCreationPolicy;
import org.springframework.security.web.SecurityFilterChain;

@Configuration
@EnableWebSecurity
@EnableMethodSecurity(prePostEnabled = true)
public class KeycloakSecurityConfig {

    private final KeycloakJwtAuthenticationConverter keycloakConverter;

    public KeycloakSecurityConfig(KeycloakJwtAuthenticationConverter keycloakConverter) {
        this.keycloakConverter = keycloakConverter;
    }

    @Bean
    public SecurityFilterChain filterChain(HttpSecurity http) throws Exception {
        return http
            .csrf(csrf -> csrf.disable())
            .sessionManagement(session -> session.sessionCreationPolicy(SessionCreationPolicy.STATELESS))
            .authorizeHttpRequests(auth -> auth
                .requestMatchers("/api/public/**", "/actuator/health").permitAll()
                .requestMatchers("/api/admin/**").hasRole("ADMIN")
                .requestMatchers("/api/orders/**").hasRole("ORDER_READ")
                .anyRequest().authenticated()
            )
            .oauth2ResourceServer(oauth2 -> oauth2
                .jwt(jwt -> jwt.jwtAuthenticationConverter(keycloakConverter))
            )
            .build();
    }
}
```

### Example 3: Programmatic Keycloak Admin Client Integration
Using the official `keycloak-admin-client` to manage users, assign roles, and trigger password resets from a Spring Boot service.

```java
package com.example.security.admin;

import org.keycloak.admin.client.Keycloak;
import org.keycloak.admin.client.KeycloakBuilder;
import org.keycloak.representations.idm.UserRepresentation;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Service;

import java.util.List;

@Service
public class KeycloakAdminService {

    @Value("${keycloak.server-url:http://localhost:8080}")
    private String serverUrl;

    @Value("${keycloak.realm:enterprise}")
    private String realm;

    @Value("${keycloak.admin.client-id:admin-cli}")
    private String adminClientId;

    @Value("${keycloak.admin.client-secret:super-admin-secret}")
    private String adminClientSecret;

    private Keycloak getKeycloakClient() {
        return KeycloakBuilder.builder()
            .serverUrl(serverUrl)
            .realm("master") // Connect to master realm for admin privileges
            .clientId(adminClientId)
            .clientSecret(adminClientSecret)
            .grantType("client_credentials")
            .build();
    }

    public List<UserRepresentation> findUsersByEmail(String email) {
        try (Keycloak keycloak = getKeycloakClient()) {
            return keycloak.realm(realm).users().searchByEmail(email, true);
        }
    }
}
```

### Example 4: Failure Anti-Pattern & Production Fix
**Anti-Pattern**: Using container-internal hostnames (`http://keycloak:8080`) for `issuer-uri` when frontend browsers receive tokens with `http://localhost:8080`, leading to instant token rejection with `JwtIssuerValidator` failure.

```yaml
# BUGGY: Docker compose internal networking mismatch!
# Keycloak issues token with iss="http://localhost:8080/realms/enterprise"
# Spring Boot checks against iss="http://keycloak:8080/realms/enterprise"
# FAILS: Issuer mismatch error on every API request!
spring:
  security:
    oauth2:
      resourceserver:
        jwt:
          issuer-uri: http://keycloak:8080/realms/enterprise
```

**Production Fix**:
Explicitly specify separate `jwk-set-uri` and decouple internal network resolution from issuer validation.

```yaml
# SECURE: Explicit decoupling of network fetching and token issuer verification
spring:
  security:
    oauth2:
      resourceserver:
        jwt:
          issuer-uri: http://localhost:8080/realms/enterprise # Public external issuer matching token 'iss'
          jwk-set-uri: http://keycloak:8080/realms/enterprise/protocol/openid-connect/certs # Internal Docker DNS for fetching JWKS
```

### Example 5: High-Performance Reactive Keycloak WebFlux Resource Server
Non-blocking token resolution and role parsing in Spring Boot WebFlux.

```java
package com.example.security.reactive;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.core.convert.converter.Converter;
import org.springframework.security.authentication.AbstractAuthenticationToken;
import org.springframework.security.config.annotation.web.reactive.EnableWebFluxSecurity;
import org.springframework.security.config.web.server.ServerHttpSecurity;
import org.springframework.security.core.authority.SimpleGrantedAuthority;
import org.springframework.security.oauth2.jwt.Jwt;
import org.springframework.security.oauth2.server.resource.authentication.JwtAuthenticationToken;
import org.springframework.security.oauth2.server.resource.authentication.ReactiveJwtAuthenticationConverterAdapter;
import org.springframework.security.web.server.SecurityWebFilterChain;
import reactor.core.publisher.Mono;

import java.util.List;
import java.util.Map;
import java.util.stream.Collectors;

@Configuration
@EnableWebFluxSecurity
public class ReactiveKeycloakConfig {

    @Bean
    public SecurityWebFilterChain reactiveFilterChain(ServerHttpSecurity http) {
        return http
            .csrf(ServerHttpSecurity.CsrfSpec::disable)
            .authorizeExchange(exchanges -> exchanges
                .pathMatchers("/public/**").permitAll()
                .anyExchange().authenticated()
            )
            .oauth2ResourceServer(oauth2 -> oauth2
                .jwt(jwt -> jwt.jwtAuthenticationConverter(
                    new ReactiveJwtAuthenticationConverterAdapter(new ReactiveKeycloakRoleConverter())
                ))
            )
            .build();
    }

    static class ReactiveKeycloakRoleConverter implements Converter<Jwt, AbstractAuthenticationToken> {
        @Override
        public AbstractAuthenticationToken convert(Jwt jwt) {
            Map<String, Object> realmAccess = jwt.getClaimAsMap("realm_access");
            List<String> roles = (realmAccess != null && realmAccess.containsKey("roles"))
                ? (List<String>) realmAccess.get("roles")
                : List.of();

            var authorities = roles.stream()
                .map(role -> new SimpleGrantedAuthority("ROLE_" + role.toUpperCase()))
                .collect(Collectors.toList());

            return new JwtAuthenticationToken(jwt, authorities, jwt.getSubject());
        }
    }
}
```

---

## 6. Best Practices

1. **Never Use Deprecated Keycloak Adapters**: Strictly rely on standard Spring Security `spring-boot-starter-oauth2-resource-server` and `spring-boot-starter-oauth2-client`.
2. **Standardize Role Conventions**: Translate Keycloak kebab-case roles (`order-admin`) to Spring uppercase underscore (`ROLE_ORDER_ADMIN`) in your converter.
3. **Decouple Internal Network DNS from Issuer URL**: Use `jwk-set-uri` for internal container-to-container traffic while keeping `issuer-uri` aligned with the public browser-facing URL.
4. **Use Confidential Clients for Backend Microservices**: Never configure public clients for machine-to-machine backend services; mandate client credentials with client secrets.
5. **Set Short Token Lifespans**: Configure Keycloak Access Token Lifespan to 5 minutes, relying on Refresh Tokens or SSO sessions.
6. **Graceful Handling of Clock Skew**: Keycloak and Spring Boot servers must have synchronized system clocks (NTP) to prevent false-positive token expiration rejections.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Still using `keycloak-spring-boot-starter` | Broken on Spring Boot 3/4 and Jakarta EE 10; completely unmaintained. | Migrate immediately to native Spring Security OAuth2 starters. |
| Missing `hasRole()` permissions | Keycloak puts roles in `realm_access.roles`, which Spring's default converter ignores. | Implement a custom `Converter<Jwt, AbstractAuthenticationToken>`. |
| Docker issuer mismatch (`localhost` vs `keycloak`) | Container cannot resolve `localhost`, while host browser cannot resolve `http://keycloak`. | Set `jwk-set-uri` to container DNS and `issuer-uri` to the external URL. |
| Inability to parse Client Roles | Application only inspects `realm_access` and ignores `resource_access.{client-id}.roles`. | Update converter to parse both realm and client-scoped role claims. |
| Forgetting `ROLE_` prefix | Spring Security's `hasRole("ADMIN")` strictly expects authority `ROLE_ADMIN`. | Prepend `ROLE_` to extracted role names during conversion. |
| Exposing Admin Client credentials in git | Attackers compromise entire Keycloak realm management. | Inject admin credentials via environment variables or secret managers. |

---

## 8. Performance & Security Notes

- **JWKS Cache Hit Rate**: `NimbusJwtDecoder` caches JWKS public keys. Ensure your application does not create a new `JwtDecoder` bean per request. Under normal operation, key verification occurs entirely in-memory with zero network overhead.
- **Token Size Bloat**: If a Keycloak realm contains dozens of composite roles and groups, access tokens can swell to 10+ KB. In microservice meshes, large tokens consume network bandwidth and HTTP header buffer capacity (`server.max-http-request-header-size`). Filter out unnecessary client roles in Keycloak token mappers.
- **Revocation Delay**: Since JWTs are verified statelessly against the cached JWKS, terminating a user in Keycloak will not immediately revoke an existing access token until its TTL expires. Keep access token TTL low (e.g., 5 minutes).

---

## 9. Comparison / When to Use What

| Feature | Keycloak | Auth0 / Okta | Spring Authorization Server |
|---|---|---|---|
| **Hosting Model** | Self-hosted (Docker/K8s) or Cloud | Fully Managed SaaS | Embedded in Spring Boot |
| **Open Source** | Yes (Apache 2.0) | Proprietary | Yes (Apache 2.0) |
| **Admin Web Console** | Full-featured included | Full-featured SaaS | Must build custom UI |
| **Federation (LDAP/AD)** | Native out-of-the-box | Enterprise add-on | Custom user service code |
| **Resource Footprint** | Moderate (Quarkus JVM / Native) | Zero (External) | Very light (Shared with app) |

---

## 10. Interview Questions

<details>
<summary>1. Why did the Keycloak team deprecate their Spring Boot adapter in favor of Spring Security?</summary>

**Answer:**
Maintaining proprietary adapters for every Java framework (Spring Boot, WildFly, Quarkus, Tomcat) created immense duplication and lagged behind framework releases. With Spring Security 5/6 providing first-class, standard-compliant OAuth 2.0 Resource Server and Client support, proprietary adapters became redundant. Standardizing on Spring Security ensures seamless compatibility, modern Jakarta EE support, and zero lag on framework updates.
</details>

<details>
<summary>2. How do you resolve the Docker DNS issuer mismatch when running Keycloak and Spring Boot locally?</summary>

**Answer:**
When a browser authenticates against Keycloak at `http://localhost:8080`, the token `iss` claim is set to `http://localhost:8080/realms/myrealm`. In Spring Boot running inside Docker, contacting `localhost:8080` fails. The solution is:
1. Configure `spring.security.oauth2.resourceserver.jwt.issuer-uri=http://localhost:8080/realms/myrealm` (for claims validation).
2. Configure `spring.security.oauth2.resourceserver.jwt.jwk-set-uri=http://keycloak:8080/realms/myrealm/protocol/openid-connect/certs` (for network key retrieval).
</details>

<details>
<summary>3. Where does Keycloak store roles within an access token?</summary>

**Answer:**
Keycloak structures roles hierarchically:
- Realm-wide roles are placed in `realm_access.roles` (an array of strings).
- Client-specific roles are placed under `resource_access.{client_id}.roles`.
Neither of these are parsed by Spring Security's default `JwtGrantedAuthoritiesConverter` (which only looks for `scope` or `scp`).
</details>

<details>
<summary>4. What is the difference between an ID Token and an Access Token in Keycloak OIDC?</summary>

**Answer:**
- **ID Token**: Intended for the **client application** (frontend or client backend). Contains user profile identity assertions (`name`, `email`, `sub`). It is used to render user interfaces and confirm authentication. It should **never** be sent to Resource Server APIs.
- **Access Token**: Intended for the **Resource Server API**. Contains authorization claims, scopes, and roles. It is passed in the `Authorization: Bearer <token>` header to authorize API operations.
</details>

<details>
<summary>5. How can you map Keycloak user groups to Spring Security authorities?</summary>

**Answer:**
In the Keycloak Admin Console, configure a **Token Mapper** of type "Group Membership" on the Client Scope. Configure it to map group paths into a custom token claim (e.g., `groups`). In Spring Boot, extract the `groups` array in your custom `Converter<Jwt, AbstractAuthenticationToken>` and create `SimpleGrantedAuthority` objects (e.g., `GROUP_FINANCE`).
</details>

<details>
<summary>6. How does Keycloak handle token revocation?</summary>

**Answer:**
Keycloak supports RFC 7009 Token Revocation (`/realms/{realm}/protocol/openid-connect/revoke`). In addition, Keycloak supports "Not Before" revocation policies: an administrator can set a revocation timestamp on a user or client. Any token issued prior to that timestamp is considered revoked by Keycloak. However, stateless JWT resource servers will only recognize this if they perform introspection or verify against a distributed revocation list.
</details>

<details>
<summary>7. Can a single Spring Boot application act as both an OAuth2 Client and a Resource Server for Keycloak?</summary>

**Answer:**
Yes. A typical enterprise BFF (Backend-For-Frontend) application acts as an OAuth2 Client for browser sessions (`http.oauth2Login(...)`) and as a Resource Server (`http.oauth2ResourceServer(...)`) for external microservice API calls using Bearer tokens. This is configured by chaining both DSL builders on the same or segmented `SecurityFilterChain` beans.
</details>

<details>
<summary>8. How do you test Keycloak-secured endpoints without running a live Keycloak server?</summary>

**Answer:**
1. In unit/slice tests (`@WebMvcTest`), use `SecurityMockMvcRequestPostProcessors.jwt()` to mock the `JwtAuthenticationToken` with custom claims and authorities.
2. In integration tests (`@SpringBootTest`), use **Testcontainers** with the official Keycloak container image (`quay.io/keycloak/keycloak`), or use WireMock to stub the Keycloak OpenID configuration and JWKS endpoints.
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Minimal Keycloak Resource Server Config (Easy)</summary>

**Task**: Write an `application.yml` file configuring Spring Boot as a JWT Resource Server against a Keycloak realm named `production` running on `https://iam.example.com`.

**Solution**:
```yaml
spring:
  security:
    oauth2:
      resourceserver:
        jwt:
          issuer-uri: https://iam.example.com/realms/production
```
</details>

<details>
<summary>Exercise 2: Simple Realm Role Extractor (Easy)</summary>

**Task**: Implement a simple converter method that reads `realm_access.roles` from a `Jwt` and returns a list of `SimpleGrantedAuthority` objects with the `ROLE_` prefix.

**Solution**:
```java
public List<SimpleGrantedAuthority> extractRealmRoles(Jwt jwt) {
    Map<String, Object> realmAccess = jwt.getClaimAsMap("realm_access");
    if (realmAccess == null || !realmAccess.containsKey("roles")) {
        return List.of();
    }
    List<String> roles = (List<String>) realmAccess.get("roles");
    return roles.stream()
        .map(role -> new SimpleGrantedAuthority("ROLE_" + role.toUpperCase()))
        .toList();
}
```
</details>

<details>
<summary>Exercise 3: Client-Scoped Role Mapper (Medium)</summary>

**Task**: Create a custom `Converter<Jwt, Collection<GrantedAuthority>>` that extracts roles only from `resource_access.payment-service.roles` and maps them to authorities prefixed with `PAYMENT_`.

**Solution**:
```java
@Component
public class PaymentClientRoleConverter implements Converter<Jwt, Collection<GrantedAuthority>> {
    @Override
    public Collection<GrantedAuthority> convert(Jwt jwt) {
        Map<String, Object> resourceAccess = jwt.getClaimAsMap("resource_access");
        if (resourceAccess == null || !resourceAccess.containsKey("payment-service")) {
            return List.of();
        }
        Map<String, Object> paymentAccess = (Map<String, Object>) resourceAccess.get("payment-service");
        List<String> roles = (List<String>) paymentAccess.get("roles");
        if (roles == null) return List.of();

        return roles.stream()
            .map(role -> (GrantedAuthority) new SimpleGrantedAuthority("PAYMENT_" + role.toUpperCase()))
            .toList();
    }
}
```
</details>

<details>
<summary>Exercise 4: Keycloak OpenID Connect Logout Handler (Medium)</summary>

**Task**: Configure Spring Security so that logging out of the Spring Boot application triggers RP-Initiated Logout against Keycloak to terminate the user's Keycloak SSO session.

**Solution**:
```java
@Bean
public SecurityFilterChain filterChain(HttpSecurity http, ClientRegistrationRepository clientRepo) throws Exception {
    OidcClientInitiatedLogoutSuccessHandler logoutSuccessHandler = 
        new OidcClientInitiatedLogoutSuccessHandler(clientRepo);
    logoutSuccessHandler.setPostLogoutRedirectUri("{baseUrl}/");

    return http
        .authorizeHttpRequests(auth -> auth.anyRequest().authenticated())
        .oauth2Login(Customizer.withDefaults())
        .logout(logout -> logout
            .logoutSuccessHandler(logoutSuccessHandler)
            .invalidateHttpSession(true)
            .clearAuthentication(true)
            .deleteCookies("JSESSIONID")
        )
        .build();
}
```
</details>

<details>
<summary>Exercise 5: Dynamic Multi-Realm Routing by URL (Hard)</summary>

**Task**: Implement an `AuthenticationManagerResolver<HttpServletRequest>` that validates JWTs against different Keycloak realms (`/realms/internal` vs `/realms/external`) depending on the request header `X-Tenant-Realm`.

**Solution**:
```java
@Configuration
public class MultiRealmConfig {

    private final Map<String, AuthenticationManager> managers = new ConcurrentHashMap<>();
    private final String keycloakBase = "http://localhost:8080/realms/";

    @Bean
    public SecurityFilterChain multiRealmFilterChain(HttpSecurity http) throws Exception {
        AuthenticationManagerResolver<HttpServletRequest> resolver = request -> {
            String realm = request.getHeader("X-Tenant-Realm");
            if (realm == null) realm = "internal";

            return managers.computeIfAbsent(realm, r -> {
                JwtDecoder decoder = JwtDecoders.fromIssuerLocation(keycloakBase + r);
                return new JwtAuthenticationProvider(decoder)::authenticate;
            });
        };

        return http
            .authorizeHttpRequests(auth -> auth.anyRequest().authenticated())
            .oauth2ResourceServer(oauth2 -> oauth2.authenticationManagerResolver(resolver))
            .build();
    }
}
```
</details>

<details>
<summary>Exercise 6: Testcontainers Keycloak Integration Test (Hard)</summary>

**Task**: Write an integration test using Testcontainers `keycloak/keycloak:26.0` that obtains an access token for a test user via OAuth2 Resource Owner Password Credentials / Direct Access Grants and successfully invokes a protected endpoint.

**Solution**:
```java
@SpringBootTest(webEnvironment = SpringBootTest.WebEnvironment.RANDOM_PORT)
@Testcontainers
class KeycloakIntegrationTest {

    @Container
    static KeycloakContainer keycloak = new KeycloakContainer("quay.io/keycloak/keycloak:26.0")
        .withRealmImportFile("test-realm.json");

    @DynamicPropertySource
    static void registerProperties(DynamicPropertyRegistry registry) {
        registry.add("spring.security.oauth2.resourceserver.jwt.issuer-uri", 
            () -> keycloak.getAuthServerUrl() + "/realms/test-realm");
    }

    @Autowired
    private TestRestTemplate restTemplate;

    @Test
    void shouldAccessProtectedEndpointWithKeycloakToken() {
        String token = obtainKeycloakToken("test-user", "password");

        HttpHeaders headers = new HttpHeaders();
        headers.setBearerAuth(token);
        HttpEntity<Void> entity = new HttpEntity<>(headers);

        ResponseEntity<String> response = restTemplate.exchange("/api/orders", HttpMethod.GET, entity, String.class);
        assertThat(response.getStatusCode()).isEqualTo(HttpStatus.OK);
    }

    private String obtainKeycloakToken(String username, String password) {
        // Exchange credentials for token using Keycloak client
        return keycloak.getKeycloakAdminClient().tokenManager().grantToken().getToken();
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **No Proprietary Adapters**: Use standard `spring-boot-starter-oauth2-resource-server` and `spring-boot-starter-oauth2-client`.
- **Custom Converter is Mandatory**: Keycloak nests roles in `realm_access` and `resource_access`. Implement `Converter<Jwt, AbstractAuthenticationToken>` to map them to `ROLE_` authorities.
- **Separate Issuer and JWKS URIs in Docker**: Use `issuer-uri` for the external browser address and `jwk-set-uri` for internal container-to-container network calls.
- **RP-Initiated Logout**: Use `OidcClientInitiatedLogoutSuccessHandler` to ensure local logout also invalidates the Keycloak SSO session.
- **Test with Testcontainers**: Use `org.testcontainers:keycloak` for end-to-end integration tests without mock stubs.

---

## 13. Further Reading
- [Keycloak Official Documentation (v26)](https://www.keycloak.org/documentation)
- [Spring Security Reference: OAuth 2.0 Resource Server](https://docs.spring.io/spring-security/reference/servlet/oauth2/resource-server/index.html)
- [OpenID Connect RP-Initiated Logout 1.0](https://openid.net/specs/openid-connect-rpinitiated-1_0.html)
- [Testcontainers Keycloak Module](https://java.testcontainers.org/modules/keycloak/)
- [RFC 8414: OAuth 2.0 Authorization Server Metadata](https://datatracker.ietf.org/doc/html/rfc8414)
