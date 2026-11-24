# 08. OAuth2 Client & Social Login in Spring Boot

| Metadata | Value |
|---|---|
| **Level** | Intermediate |
| **Prerequisites** | `01-spring-security-architecture.md`, `02-authentication-fundamentals.md`, RFC 6749 (OAuth 2.0), RFC 7636 (PKCE) |
| **Estimated Reading Time** | 35 minutes |
| **Status** | Complete |
| **Spring Boot Version** | 3.4.x / 4.x |
| **Spring Framework Version** | 6.2.x / 7.x |
| **Java Baseline** | Java 21 / 25 LTS |
| **Build Tool** | Maven 3.9+ / Gradle 8.10+ |
| **Tested With** | OpenJDK 21, Spring Boot 3.4.1, GitHub / Google OAuth2 |

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
Traditional web authentication forces users to register and manage unique credentials (username/password) for every application. This creates friction, encourages weak password reuse, and burdens developers with the operational risks of credential storage, password resets, and MFA enforcement.

**Social Login and OAuth 2.0 Client capabilities** allow applications to delegate identity management to trusted third-party providers (Identity Providers / IdPs) such as Google, GitHub, Microsoft, or Keycloak using the standard **Authorization Code Grant with PKCE (Proof Key for Code Exchange)**.

### Historical Evolution
- **Spring Security OAuth (Legacy 2.x)**: Required complex XML and `@EnableOAuth2Sso` annotations with fragmented client token stores.
- **Spring Security 5.x**: Unified the OAuth 2.0 stack into core framework modules (`spring-security-oauth2-client`). Introduced auto-configured well-known providers (Google, GitHub, Facebook, Okta) and the Lambda DSL (`http.oauth2Login(...)`).
- **Spring Security 6.x / 7.x**: Enforces RFC 7636 (PKCE) by default for all authorization code flows, deprecates legacy authorization request resolvers, simplifies `OAuth2AuthorizedClient` management, and standardizes custom user mapping for both OAuth2 and OpenID Connect (OIDC).

### Real-World Relevance
Modern web applications, SaaS dashboards, and consumer portals rely on social login as the primary onboarding funnel. Furthermore, backend services acting as OAuth2 Clients routinely call downstream protected third-party APIs (such as Google Drive, GitHub Repos, or Slack Webhooks) on behalf of authenticated users using secured access tokens.

---

## 2. Core Concepts

### Architectural Contracts
- **`org.springframework.security.oauth2.client.registration.ClientRegistration`**: Immutable representation of an OAuth 2.0 / OIDC provider configuration (client ID, client secret, authorization URI, token URI, JWK set URI, redirect URI, and scopes).
- **`org.springframework.security.oauth2.client.registration.ClientRegistrationRepository`**: Repository for retrieving `ClientRegistration` instances by their registration ID (e.g., `google`, `github`).
- **`org.springframework.security.oauth2.client.OAuth2AuthorizedClient`**: Associates an authenticated user (principal) with an authorized `ClientRegistration` and their corresponding `OAuth2AccessToken` and optional `OAuth2RefreshToken`.
- **`org.springframework.security.oauth2.client.OAuth2AuthorizedClientService`**: Persistence mechanism for storing `OAuth2AuthorizedClient` instances (in-memory or JDBC/Redis).
- **`org.springframework.security.oauth2.client.userinfo.OAuth2UserService`**: Strategy for fetching and constructing user details (`OAuth2User` or `OidcUser`) once an access token is exchanged.

### The Mental Model
```
[ End User ] ──► 1. Accesses /oauth2/authorization/github ──────────────────────────┐
      │                                                                              │
      │ 2. Redirected (302) with PKCE code_challenge & state                         ▼
      ▼                                                                 [ Spring Boot App ]
[ GitHub / Google Login ] ── 3. Authenticates & Approves Scopes               (OAuth2 Client)
      │                                                                              │
      │ 4. Redirects to /login/oauth2/code/github?code=XYZ&state=ABC ◄───────────────┘
      ▼
[ OAuth2LoginAuthenticationFilter ]
      │
      ├── 5. Exchanges Code + code_verifier for Access Token at Token Endpoint
      ├── 6. Calls UserInfo Endpoint with Access Token
      ├── 7. Maps attributes to OAuth2User / OidcUser
      └── 8. Creates OAuth2AuthenticationToken in SecurityContextHolder
```

### Invariants
1. OAuth 2.0 is an **authorization** protocol; OpenID Connect (OIDC) is the **authentication** layer built on top of OAuth 2.0.
2. The `state` parameter must always be cryptographically random and verified upon return to prevent Login CSRF attacks.
3. PKCE (`code_challenge` / `code_verifier`) protects against authorization code interception attacks, even when using confidential clients.

---

## 3. How It Works Internally

### The OAuth2 Login Filter Pipeline
1. **Initiation**: The user visits `/oauth2/authorization/{registrationId}`.
   - `OAuth2AuthorizationRequestRedirectFilter` intercepts the request.
   - Generates a cryptographically random `state` nonce and a PKCE `code_verifier`.
   - Computes `code_challenge = BASE64URL-ENCODE(SHA256(code_verifier))`.
   - Persists the authorization request in `AuthorizationRequestRepository` (default HTTP session).
   - Issues an HTTP 302 redirect to the provider's authorization endpoint.
2. **Callback**: The provider redirects the user back to `/login/oauth2/code/{registrationId}` with `code` and `state`.
3. **Filter Processing**: `OAuth2LoginAuthenticationFilter` intercepts the callback:
   - Validates that the returned `state` matches the stored authorization request.
   - Emits an unauthenticated `OAuth2LoginAuthenticationToken`.
4. **Token Exchange**: `OAuth2LoginAuthenticationProvider` delegates to `OAuth2AuthorizationCodeAuthenticationProvider`:
   - Makes a `POST` request to the provider's token endpoint with the authorization code and original `code_verifier`.
   - Obtains the `OAuth2AccessToken` and optional `OAuth2RefreshToken`.
5. **User Profile Retrieval**:
   - For standard OAuth2 (e.g., GitHub), delegates to `DefaultOAuth2UserService` to query the `/user` endpoint.
   - For OIDC (e.g., Google), delegates to `OidcUserService` to validate the `id_token` signature and query the `/userinfo` endpoint.
6. **Persistence**: Saves the `OAuth2AuthorizedClient` in `OAuth2AuthorizedClientRepository` and sets the resulting `OAuth2AuthenticationToken` in `SecurityContextHolder`.

---

## 4. Syntax & Usage

### 1. Minimal Configuration (`application.yml`)
Spring Boot provides pre-configured templates for common social providers:

```yaml
spring:
  security:
    oauth2:
      client:
        registration:
          google:
            client-id: ${GOOGLE_CLIENT_ID}
            client-secret: ${GOOGLE_CLIENT_SECRET}
            scope:
              - openid
              - profile
              - email
          github:
            client-id: ${GITHUB_CLIENT_ID}
            client-secret: ${GITHUB_CLIENT_SECRET}
            scope:
              - read:user
              - user:email
```

### 2. Security Configuration Bean
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
public class OAuth2LoginSecurityConfig {

    @Bean
    public SecurityFilterChain securityFilterChain(HttpSecurity http) throws Exception {
        return http
            .authorizeHttpRequests(auth -> auth
                .requestMatchers("/", "/login", "/error", "/webjars/**").permitAll()
                .anyRequest().authenticated()
            )
            .oauth2Login(oauth2 -> oauth2
                .loginPage("/login")
                .defaultSuccessUrl("/dashboard", true)
            )
            .logout(logout -> logout
                .logoutSuccessUrl("/")
                .invalidateHttpSession(true)
                .deleteCookies("JSESSIONID")
            )
            .build();
    }
}
```

---

## 5. Practical Examples

### Example 1: Custom User Entity Synchronization on Social Login
When a user authenticates via OAuth2, synchronize their social profile attributes into a local JPA `User` entity and assign internal application roles.

```java
package com.example.security.service;

import com.example.security.domain.UserEntity;
import com.example.security.repository.UserRepository;
import org.springframework.security.core.authority.SimpleGrantedAuthority;
import org.springframework.security.oauth2.client.userinfo.DefaultOAuth2UserService;
import org.springframework.security.oauth2.client.userinfo.OAuth2UserRequest;
import org.springframework.security.oauth2.core.OAuth2AuthenticationException;
import org.springframework.security.oauth2.core.user.DefaultOAuth2User;
import org.springframework.security.oauth2.core.user.OAuth2User;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.util.Collections;
import java.util.Map;

@Service
public class CustomOAuth2UserService extends DefaultOAuth2UserService {

    private final UserRepository userRepository;

    public CustomOAuth2UserService(UserRepository userRepository) {
        this.userRepository = userRepository;
    }

    @Override
    @Transactional
    public OAuth2User loadUser(OAuth2UserRequest userRequest) throws OAuth2AuthenticationException {
        OAuth2User oAuth2User = super.loadUser(userRequest);
        String registrationId = userRequest.getClientRegistration().getRegistrationId();
        Map<String, Object> attributes = oAuth2User.getAttributes();

        String email = switch (registrationId) {
            case "google" -> (String) attributes.get("email");
            case "github" -> (String) attributes.get("email");
            default -> throw new OAuth2AuthenticationException("Unsupported provider: " + registrationId);
        };

        String name = (String) attributes.getOrDefault("name", "Unknown");

        UserEntity user = userRepository.findByEmail(email)
            .map(existing -> {
                existing.setFullName(name);
                return userRepository.save(existing);
            })
            .orElseGet(() -> {
                UserEntity newUser = new UserEntity();
                newUser.setEmail(email);
                newUser.setFullName(name);
                newUser.setRole("ROLE_USER");
                return userRepository.save(newUser);
            });

        return new DefaultOAuth2User(
            Collections.singleton(new SimpleGrantedAuthority(user.getRole())),
            attributes,
            registrationId.equals("google") ? "sub" : "id"
        );
    }
}
```

### Example 2: Calling Downstream Protected APIs using `@RegisteredOAuth2AuthorizedClient`
In modern microservices or portals, you often need to use the user's delegated OAuth2 access token to call an external API (e.g., GitHub Repositories API).

```java
package com.example.security.controller;

import org.springframework.security.core.annotation.AuthenticationPrincipal;
import org.springframework.security.oauth2.client.OAuth2AuthorizedClient;
import org.springframework.security.oauth2.client.annotation.RegisteredOAuth2AuthorizedClient;
import org.springframework.security.oauth2.core.user.OAuth2User;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RestController;
import org.springframework.web.client.RestClient;

import java.util.List;
import java.util.Map;

@RestController
public class GitHubIntegrationController {

    private final RestClient restClient;

    public GitHubIntegrationController(RestClient.Builder restClientBuilder) {
        this.restClient = restClientBuilder.baseUrl("https://api.github.com").build();
    }

    @GetMapping("/api/my-repos")
    public List<Map<String, Object>> getUserRepositories(
            @RegisteredOAuth2AuthorizedClient("github") OAuth2AuthorizedClient authorizedClient,
            @AuthenticationPrincipal OAuth2User oauth2User) {

        String accessToken = authorizedClient.getAccessToken().getTokenValue();

        @SuppressWarnings("unchecked")
        List<Map<String, Object>> repositories = restClient.get()
            .uri("/user/repos?sort=updated&per_page=5")
            .header("Authorization", "Bearer " + accessToken)
            .header("Accept", "application/vnd.github.v3+json")
            .retrieve()
            .body(List.class);

        return repositories;
    }
}
```

### Example 3: Edge Case – Custom Authorization Server with Generic Provider Registration
When integrating with an enterprise internal OpenID Connect server that lacks Spring Boot starter defaults.

```yaml
spring:
  security:
    oauth2:
      client:
        registration:
          corp-idp:
            client-id: portal-app
            client-secret: secret-credentials-xyz
            client-authentication-method: client_secret_basic
            authorization-grant-type: authorization_code
            redirect-uri: "{baseUrl}/login/oauth2/code/{registrationId}"
            scope:
              - openid
              - profile
              - enterprise:read
            client-name: Corporate SSO
        provider:
          corp-idp:
            issuer-uri: https://sso.internal.corp/auth/realms/enterprise
            user-name-attribute: preferred_username
```

### Example 4: Failure Anti-Pattern & Production Fix
**Anti-Pattern**: Storing the raw `OAuth2AuthorizationRequest` in memory or neglecting HTTPS redirect URI enforcement in production, resulting in `redirect_uri_mismatch` errors behind reverse proxies (Nginx/Cloudflare/AWS ALB).

```java
// BUGGY: Behind AWS ALB or Nginx, Spring sees http://localhost:8080 and generates
// redirect_uri=http://localhost:8080/login/oauth2/code/google instead of https://app.com/...
// Provider immediately fails with "redirect_uri_mismatch"!
```

**Production Fix**:
Enable Spring Boot Forwarded Header handling and configure server proxy properties.

```yaml
# application.yml
server:
  forward-headers-strategy: framework
```
```java
// SECURE: Enforce HTTPS scheme in authorization redirect URI resolver
@Bean
public SecurityFilterChain secureOAuth2FilterChain(HttpSecurity http) throws Exception {
    return http
        .oauth2Login(oauth2 -> oauth2
            .authorizationEndpoint(auth -> auth
                .authorizationRequestResolver(customResolver())
            )
        )
        .build();
}

private OAuth2AuthorizationRequestResolver customResolver() {
    DefaultOAuth2AuthorizationRequestResolver resolver = 
        new DefaultOAuth2AuthorizationRequestResolver(clientRegistrationRepository, "/oauth2/authorization");
    resolver.setAuthorizationRequestCustomizer(OAuth2AuthorizationRequestCustomizers.withPkce());
    return resolver;
}
```

### Example 5: High-Performance Non-Blocking WebFlux Reactive OAuth2 Login
In reactive web applications, social login is handled via `ServerOAuth2AuthorizedClientRepository` and `ReactiveOAuth2UserService`.

```java
package com.example.security.reactive;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.security.config.annotation.web.reactive.EnableWebFluxSecurity;
import org.springframework.security.config.web.server.ServerHttpSecurity;
import org.springframework.security.oauth2.client.userinfo.DefaultReactiveOAuth2UserService;
import org.springframework.security.oauth2.client.userinfo.OAuth2UserRequest;
import org.springframework.security.oauth2.client.userinfo.ReactiveOAuth2UserService;
import org.springframework.security.oauth2.core.user.OAuth2User;
import org.springframework.security.web.server.SecurityWebFilterChain;
import reactor.core.publisher.Mono;

@Configuration
@EnableWebFluxSecurity
public class ReactiveOAuth2SecurityConfig {

    @Bean
    public SecurityWebFilterChain reactiveSecurityWebFilterChain(ServerHttpSecurity http) {
        return http
            .authorizeExchange(exchanges -> exchanges
                .pathMatchers("/", "/public/**").permitAll()
                .anyExchange().authenticated()
            )
            .oauth2Login(oauth2 -> oauth2
                .userService(reactiveOAuth2UserService())
            )
            .build();
    }

    @Bean
    public ReactiveOAuth2UserService<OAuth2UserRequest, OAuth2User> reactiveOAuth2UserService() {
        DefaultReactiveOAuth2UserService delegate = new DefaultReactiveOAuth2UserService();
        return request -> delegate.loadUser(request)
            .flatMap(user -> {
                // Enrich reactive user or log authentication asynchronously
                return Mono.just(user);
            });
    }
}
```

---

## 6. Best Practices

1. **Always Use PKCE**: Proof Key for Code Exchange (RFC 7636) prevents authorization code theft. Ensure `withPkce()` is active (default in modern Spring Security).
2. **Handle Reverse Proxies Gracefully**: Always set `server.forward-headers-strategy=framework` when deploying behind ALBs, Cloudflare, or Nginx to ensure redirect URIs preserve the `https` scheme.
3. **Persist Authorized Clients Reliably**: In clustered multi-node deployments, replace the default in-memory `OAuth2AuthorizedClientService` with `JdbcOAuth2AuthorizedClientService` to survive pod restarts.
4. **Never Expose Client Secrets to Browsers**: Maintain confidential client credentials securely on the backend server. Never embed secrets in client-side JS or mobile apps.
5. **Implement User Info Caching**: If user information is fetched on subsequent requests, cache profile data or use ID tokens to avoid rate-limiting from providers like GitHub.
6. **Gracefully Handle Missing Emails**: Providers like GitHub allow users to make their email private. Always configure scopes (`user:email`) and handle fallback logic if the primary email attribute is null.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| `redirect_uri_mismatch` error | Reverse proxy drops TLS termination headers, causing Spring to emit `http://` instead of `https://`. | Set `server.forward-headers-strategy=framework` and ensure proxy sends `X-Forwarded-Proto`. |
| GitHub returns null email | User has marked email as private in GitHub account settings. | Fetch `/user/emails` endpoint explicitly in custom `OAuth2UserService` to retrieve primary email. |
| In-memory authorized client loss | Pod restarts wipe stored OAuth2 tokens, breaking scheduled background client jobs. | Configure `JdbcOAuth2AuthorizedClientService` backed by a relational database schema. |
| Missing `openid` scope on Google | Google treats the flow as raw OAuth2 rather than OIDC, omitting `id_token` and `OidcUser` attributes. | Ensure `scope: [openid, profile, email]` is explicitly included. |
| Login CSRF via unvalidated `state` | Disabling session storage or breaking the `state` verification leaves users vulnerable to account hijacking. | Retain `HttpSessionOAuth2AuthorizationRequestRepository` or cookie-backed request repository. |
| Token refresh failure on expiration | Downstream API call fails with 401 when the access token expires. | Use `OAuth2AuthorizedClientManager` which handles token refresh via refresh token automatically. |

---

## 8. Performance & Security Notes

- **Network Hops During Authentication**: A standard OAuth2 login involves at least 2 remote HTTP network hops during the callback: (1) Code-for-token exchange, and (2) UserInfo endpoint query. In high-traffic deployments, configure HTTP client connection pooling (`RestClient` / `WebClient`) with short connection timeouts (e.g., 2000ms).
- **Session Bloat**: The default `HttpSessionOAuth2AuthorizationRequestRepository` stores pending authorization requests in the HTTP session. Ensure sessions expire properly to prevent memory leaks from abandoned logins.
- **Virtual Threads Integration**: Network calls to external token and userinfo endpoints during the login handshake block carrier threads on JDK 17. On JDK 21+ with Virtual Threads enabled (`spring.threads.virtual.enabled=true`), these network blocks unmount efficiently.

---

## 9. Comparison / When to Use What

| Feature / Flow | Standard OAuth 2.0 | OpenID Connect (OIDC) | SAML 2.0 |
|---|---|---|---|
| **Primary Goal** | Delegated Authorization (API Access) | Federated Authentication (Identity) | Enterprise SSO & Identity |
| **Data Format** | JSON / URL-encoded | JSON Web Token (`id_token`) | XML (Assertions, Signatures) |
| **Transport** | REST / HTTPS | REST / HTTPS | SOAP / HTTP POST Binding |
| **Typical Providers** | GitHub, LinkedIn, Spotify | Google, Keycloak, Auth0, Okta | Microsoft ADFS, Ping Identity, Okta |
| **Spring Class** | `OAuth2User`, `OAuth2UserService` | `OidcUser`, `OidcUserService` | `Saml2AuthenticatedPrincipal` |

---

## 10. Interview Questions

<details>
<summary>1. What is the fundamental difference between OAuth 2.0 and OpenID Connect (OIDC)?</summary>

**Answer:**
OAuth 2.0 is strictly an **authorization** framework that issues Access Tokens so clients can access protected resources on behalf of a user. It does not provide identity or user authentication standards.
OpenID Connect (OIDC) is an **identity layer** built directly on top of OAuth 2.0. It introduces an `id_token` (a digitally signed JWT) containing user identity claims (`sub`, `email`, `name`) and standardizes a `/userinfo` endpoint.
</details>

<details>
<summary>2. Why was PKCE originally designed for public clients, and why is it now recommended for confidential clients?</summary>

**Answer:**
PKCE (RFC 7636) was originally created to protect mobile and SPA clients (public clients) that cannot store a client secret securely. PKCE prevents attackers from intercepting authorization codes via custom OS URI schemes. It is now standard practice for confidential (backend) clients as well because it defends against code injection and authorization code leakage across internal infrastructure.
</details>

<details>
<summary>3. What happens if the `state` parameter does not match upon OAuth2 callback?</summary>

**Answer:**
Spring Security rejects the request immediately by raising an `OAuth2AuthenticationException` ("authorization_request_not_found" or "invalid_state"). This prevents **Login CSRF**, an attack where an attacker tricks a victim into authenticating with the attacker's social credentials, allowing the attacker to monitor the victim's subsequent actions on the platform.
</details>

<details>
<summary>4. How does Spring Boot auto-configure `ClientRegistration` for Google and GitHub?</summary>

**Answer:**
Spring Boot includes built-in metadata definitions in `CommonOAuth2Provider` (an enum containing authorization URIs, token URIs, and user-info URIs for Google, GitHub, Facebook, and Okta). When you supply `spring.security.oauth2.client.registration.google.client-id`, Spring Boot detects the registration ID `google` and automatically merges the missing provider endpoints.
</details>

<details>
<summary>5. How does Spring Security refresh an expired OAuth2 Access Token when calling a third-party API?</summary>

**Answer:**
By using `OAuth2AuthorizedClientManager` (specifically `DefaultOAuth2AuthorizedClientManager`). When an authorized client is requested, the manager checks the token expiration against the current clock time. If expired and a refresh token is present, it issues a refresh token grant request to the token endpoint, updates the `OAuth2AuthorizedClientRepository`, and returns the refreshed token transparently.
</details>

<details>
<summary>6. What is the role of `OAuth2AuthorizedClientService`?</summary>

**Answer:**
It is the service interface responsible for persisting and retrieving `OAuth2AuthorizedClient` instances, which tie a specific user principal to their client tokens. By default, Spring Boot configures an `InMemoryOAuth2AuthorizedClientService`, but production clusters typically override this with `JdbcOAuth2AuthorizedClientService`.
</details>

<details>
<summary>7. How can you extract user details from both Google (OIDC) and GitHub (OAuth2) in a controller?</summary>

**Answer:**
You can inject `@AuthenticationPrincipal OAuth2User principal`. Since `OidcUser` extends `OAuth2User`, this annotation works polymorphically across both pure OAuth2 providers (GitHub) and OIDC providers (Google):
```java
@GetMapping("/user")
public Map<String, Object> user(@AuthenticationPrincipal OAuth2User principal) {
    return principal.getAttributes();
}
```
</details>

<details>
<summary>8. How do you implement logout with OpenID Connect (RP-Initiated Logout)?</summary>

**Answer:**
OIDC defines RP-Initiated Logout (Relying Party Logout). In Spring Security, configure `OidcClientInitiatedLogoutSuccessHandler`:
```java
@Bean
public SecurityFilterChain filterChain(HttpSecurity http, ClientRegistrationRepository clientRepo) throws Exception {
    OidcClientInitiatedLogoutSuccessHandler logoutHandler = 
        new OidcClientInitiatedLogoutSuccessHandler(clientRepo);
    logoutHandler.setPostLogoutRedirectUri("{baseUrl}/");

    return http
        .logout(logout -> logout.logoutSuccessHandler(logoutHandler))
        .build();
}
```
This terminates both the local Spring Boot session and the remote session on the IdP (e.g., Keycloak or Okta).
</details>

---

## 11. Practice Exercises

<details>
<summary>Exercise 1: Configure Multi-Provider Social Login (Easy)</summary>

**Task**: Write an `application.yml` configuration snippet that configures GitHub and Google OAuth2 login for a Spring Boot application using environment variables for client IDs and secrets.

**Solution**:
```yaml
spring:
  security:
    oauth2:
      client:
        registration:
          google:
            client-id: ${GOOGLE_ID}
            client-secret: ${GOOGLE_SECRET}
            scope: [openid, profile, email]
          github:
            client-id: ${GITHUB_ID}
            client-secret: ${GITHUB_SECRET}
            scope: [read:user, user:email]
```
</details>

<details>
<summary>Exercise 2: Securing Routes with OAuth2 Login (Easy)</summary>

**Task**: Create a `SecurityFilterChain` where `/` and `/public/**` are permitted to anyone, but all other requests require OAuth2 login. Redirect successful logins to `/home`.

**Solution**:
```java
@Configuration
public class SimpleOAuth2SecurityConfig {

    @Bean
    public SecurityFilterChain filterChain(HttpSecurity http) throws Exception {
        return http
            .authorizeHttpRequests(auth -> auth
                .requestMatchers("/", "/public/**").permitAll()
                .anyRequest().authenticated()
            )
            .oauth2Login(oauth2 -> oauth2
                .defaultSuccessUrl("/home", true)
            )
            .build();
    }
}
```
</details>

<details>
<summary>Exercise 3: Custom Authority Mapper for Social Users (Medium)</summary>

**Task**: Implement a `GrantedAuthoritiesMapper` bean that checks if the logged-in user's email ends with `@company.com`. If it does, assign `ROLE_EMPLOYEE`; otherwise, assign `ROLE_EXTERNAL`.

**Solution**:
```java
@Bean
public GrantedAuthoritiesMapper userAuthoritiesMapper() {
    return authorities -> {
        Set<GrantedAuthority> mapped = new HashSet<>();
        for (GrantedAuthority authority : authorities) {
            if (authority instanceof OAuth2UserAuthority oauth2Auth) {
                Map<String, Object> attrs = oauth2Auth.getAttributes();
                String email = (String) attrs.get("email");
                if (email != null && email.endsWith("@company.com")) {
                    mapped.add(new SimpleGrantedAuthority("ROLE_EMPLOYEE"));
                } else {
                    mapped.add(new SimpleGrantedAuthority("ROLE_EXTERNAL"));
                }
            }
        }
        return mapped;
    };
}
```
</details>

<details>
<summary>Exercise 4: Dedicated Client with RestClient Interceptor (Medium)</summary>

**Task**: Build a Spring `RestClient` configured with an HTTP request interceptor that automatically attaches the `Authorization: Bearer <token>` from `OAuth2AuthorizedClientManager` whenever calling an external API.

**Solution**:
```java
@Bean
public RestClient externalApiClient(OAuth2AuthorizedClientManager authorizedClientManager) {
    return RestClient.builder()
        .baseUrl("https://api.thirdparty.com")
        .requestInterceptor((request, body, execution) -> {
            OAuth2AuthorizeRequest authorizeRequest = OAuth2AuthorizeRequest
                .withClientRegistrationId("my-service-client")
                .principal("internal-system")
                .build();
            OAuth2AuthorizedClient authorizedClient = authorizedClientManager.authorize(authorizeRequest);
            if (authorizedClient != null && authorizedClient.getAccessToken() != null) {
                request.getHeaders().setBearerAuth(authorizedClient.getAccessToken().getTokenValue());
            }
            return execution.execute(request, body);
        })
        .build();
}
```
</details>

<details>
<summary>Exercise 5: Persistent Authorized Client Service with JDBC (Hard)</summary>

**Task**: Configure a production `JdbcOAuth2AuthorizedClientService` bean with a custom `JdbcTemplate` and `ClientRegistrationRepository` to persist user tokens across clustered server instances.

**Solution**:
```java
@Configuration
public class JdbcOAuth2Config {

    @Bean
    public OAuth2AuthorizedClientService authorizedClientService(
            JdbcOperations jdbcOperations,
            ClientRegistrationRepository clientRegistrationRepository) {
        return new JdbcOAuth2AuthorizedClientService(jdbcOperations, clientRegistrationRepository);
    }

    @Bean
    public OAuth2AuthorizedClientRepository authorizedClientRepository(
            OAuth2AuthorizedClientService authorizedClientService) {
        return new AuthenticatedPrincipalOAuth2AuthorizedClientRepository(authorizedClientService);
    }
}
```
</details>

<details>
<summary>Exercise 6: MockMvc Unit Testing Social Login Controllers (Hard)</summary>

**Task**: Write a `@WebMvcTest` verifying that a controller endpoint returning the logged-in user's name functions correctly when tested with `oauth2Login()`.

**Solution**:
```java
@WebMvcTest(controllers = ProfileController.class)
class ProfileControllerTest {

    @Autowired
    private MockMvc mockMvc;

    @Test
    void shouldReturnProfileWhenAuthenticatedViaOAuth2() throws Exception {
        mockMvc.perform(get("/api/profile")
                .with(oauth2Login()
                    .attributes(attrs -> {
                        attrs.put("name", "Alice Smith");
                        attrs.put("email", "alice@example.com");
                    })
                    .authorities(new SimpleGrantedAuthority("ROLE_USER"))
                ))
            .andExpect(status().isOk())
            .andExpect(jsonPath("$.name").value("Alice Smith"))
            .andExpect(jsonPath("$.email").value("alice@example.com"));
    }
}
```
</details>

---

## 12. Summary / Cheat Sheet

- **OAuth2 vs OIDC**: Use OAuth2 for API access tokens; use OIDC when you need verified user identity (`id_token`).
- **PKCE is Mandatory**: Always use PKCE (`code_challenge` / `code_verifier`) for code grant flows.
- **Reverse Proxy Headers**: Set `server.forward-headers-strategy=framework` to prevent `redirect_uri_mismatch` on HTTPS load balancers.
- **Custom Mapping with `OAuth2UserService`**: Extend `DefaultOAuth2UserService` or `OidcUserService` to persist social users into local databases.
- **Token Management**: Use `@RegisteredOAuth2AuthorizedClient` in MVC controllers or configure `DefaultOAuth2AuthorizedClientManager` for background token refresh.

---

## 13. Further Reading
- [Spring Security Reference: OAuth 2.0 Client](https://docs.spring.io/spring-security/reference/servlet/oauth2/client/index.html)
- [RFC 6749: The OAuth 2.0 Authorization Framework](https://datatracker.ietf.org/doc/html/rfc6749)
- [RFC 7636: Proof Key for Code Exchange by OAuth Public Clients (PKCE)](https://datatracker.ietf.org/doc/html/rfc7636)
- [OpenID Connect Core 1.0 Specification](https://openid.net/specs/openid-connect-core-1_0.html)
- [RFC 7009: OAuth 2.0 Token Revocation](https://datatracker.ietf.org/doc/html/rfc7009)
