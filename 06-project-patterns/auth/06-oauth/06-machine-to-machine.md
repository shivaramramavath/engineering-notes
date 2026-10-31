# Machine-to-Machine (Client Credentials)

When no user is involved (a cron job, a backend service calling another service, a partner's server calling your API) there is no one to log in or consent. The **Client Credentials grant** lets a service authenticate **as itself** and receive a short-lived access token.

**Prerequisites:** [00-oauth2-fundamentals.md](./00-oauth2-fundamentals.md), [07-jwt-algorithms-and-keys.md](../03-tokens/07-jwt-algorithms-and-keys.md)

---

## The flow

```text
Service A (client)                 Authorization Server              Service B (resource server)
       │                                   │                                   │
       │ POST /token                       │                                   │
       │ grant_type=client_credentials     │                                   │
       │ + client authentication           │                                   │
       │ + scope / audience                │                                   │
       │ ────────────────────────────────► │                                   │
       │ ◄──── access_token (short-lived) ─│                                   │
       │                                                                       │
       │ GET /api/orders   Authorization: Bearer <token>                       │
       │ ────────────────────────────────────────────────────────────────────► │
       │                                              verify signature, aud, scope
```

Differences from the user flows:

| | Authorization Code | Client Credentials |
|---|---|---|
| Subject | A user | The service itself |
| Browser/redirects | Yes | None |
| Refresh token | Usually | **No**: just request a new token |
| Consent | User approves | Pre-agreed by administrators |
| Client type | Public or confidential | **Confidential only** (a secret must be kept) |
| Token `sub` | User id | Client id |

Never use this grant from a browser or mobile app: the client secret would be extractable.

---

## Token request

```http
POST /oauth/token HTTP/1.1
Host: auth.example.com
Authorization: Basic base64(client_id:client_secret)
Content-Type: application/x-www-form-urlencoded

grant_type=client_credentials&scope=orders:read&audience=https://api.example.com
```

```json
{ "access_token": "eyJ...", "token_type": "Bearer", "expires_in": 600, "scope": "orders:read" }
```

### Client authentication methods

| Method | How | Strength |
|---|---|---|
| `client_secret_basic` | HTTP Basic header (id and secret form-URL-encoded, then base64) | Common default |
| `client_secret_post` | `client_id` / `client_secret` in the body | Works, slightly less preferred |
| `private_key_jwt` | Client signs a short-lived JWT assertion with its private key | **Stronger**: no shared secret at the AS, easy rotation |
| mutual TLS | Client certificate | Strong; more infrastructure |

`audience` is a provider-specific parameter (Auth0 uses `audience`; the standard way is Resource Indicators, RFC 8707, `resource`); check your provider's docs.

---

## Calling side: get and cache the token

Don't request a token for every API call. Cache until just before expiry and share one in-flight request:

```js
// clients/service-token.js
let cached = null;      // { token, expiresAt }
let inflight = null;

export async function getServiceToken() {
  if (cached && Date.now() < cached.expiresAt - 30_000) return cached.token;   // refresh 30 s early
  inflight ??= fetchToken().finally(() => { inflight = null; });
  return inflight;
}

async function fetchToken() {
  const res = await fetch(`${process.env.AS_URL}/oauth/token`, {
    method: "POST",
    headers: {
      Authorization: "Basic " + Buffer.from(`${encodeURIComponent(process.env.CLIENT_ID)}:${encodeURIComponent(process.env.CLIENT_SECRET)}`).toString("base64"),
      "Content-Type": "application/x-www-form-urlencoded",
    },
    body: new URLSearchParams({ grant_type: "client_credentials", scope: "orders:read", audience: process.env.API_AUDIENCE }),
  });
  if (!res.ok) throw new Error(`Token request failed: ${res.status}`);   // don't log the body or credentials
  const { access_token, expires_in } = await res.json();
  cached = { token: access_token, expiresAt: Date.now() + expires_in * 1000 };
  return access_token;
}
```

Also handle `401` from the API once by clearing the cache and retrying (the token may have been revoked or the key rotated).

---

## Resource server: verify tokens and enforce scopes

```js
import { createRemoteJWKSet, jwtVerify } from "jose";

const JWKS = createRemoteJWKSet(new URL(`${process.env.AS_URL}/.well-known/jwks.json`));

export const authenticateService = async (req, res, next) => {
  const header = req.get("authorization");
  if (!header?.startsWith("Bearer ")) return res.status(401).set("WWW-Authenticate", "Bearer").end();

  try {
    const { payload } = await jwtVerify(header.slice(7), JWKS, {
      issuer: process.env.AS_ISSUER,
      audience: "https://api.example.com",      // is this token meant for ME?
      algorithms: ["EdDSA"],
    });
    req.client = { id: payload.sub, scopes: String(payload.scope ?? "").split(" ").filter(Boolean) };
    next();
  } catch {
    res.status(401).set("WWW-Authenticate", 'Bearer error="invalid_token"').end();
  }
};

export const requireScope = (scope) => (req, res, next) =>
  req.client.scopes.includes(scope)
    ? next()
    : res.status(403).set("WWW-Authenticate", `Bearer error="insufficient_scope", scope="${scope}"`).json({ message: "Insufficient scope" });

router.get("/orders", authenticateService, requireScope("orders:read"), listOrders);
```

The `aud` check matters: without it, a token minted for service C works on service B (confused deputy). `scope` limits *what* the client may do; the endpoint still decides what data is in range (for example, which tenant's orders).

---

## Using an existing provider vs issuing your own

| Option | When |
|---|---|
| **Managed/standard AS** (Auth0, Cognito, Keycloak, Okta, Ory Hydra) | Default choice: standard endpoints, client management, key rotation, auditing |
| **Cloud-native identity** (AWS IAM roles, GCP service accounts, Azure managed identities, Kubernetes service account tokens) | Services in the same cloud: no secrets to manage at all; prefer where available |
| **Your own minimal token endpoint** | A handful of internal clients and you don't want another system |

A minimal internal token endpoint, to see the moving parts:

```js
// clients stored as: { clientId, secretHash (SHA-256 of 256-bit random secret), allowedScopes, audience, active }
router.post("/oauth/token", rateLimit, express.urlencoded({ extended: false }), async (req, res) => {
  const { clientId, clientSecret } = parseBasicAuth(req);                       // or from body
  const client = clientId && await ServiceClient.findOne({ clientId, active: true });

  // Always compare against SOMETHING (dummy hash) so timing doesn't reveal valid client ids
  const ok = client && safeCompare(hashToken(clientSecret ?? ""), client.secretHash);
  if (!ok || req.body.grant_type !== "client_credentials") {
    return res.status(401).set("WWW-Authenticate", "Basic").json({ error: "invalid_client" });
  }

  const requested = (req.body.scope ?? "").split(" ").filter(Boolean);
  const granted = requested.length ? requested.filter((s) => client.allowedScopes.includes(s)) : client.allowedScopes;
  if (requested.length && granted.length !== requested.length) return res.status(400).json({ error: "invalid_scope" });

  const token = await new SignJWT({ scope: granted.join(" "), client_id: client.clientId })
    .setProtectedHeader({ alg: "EdDSA", kid: CURRENT_KID })
    .setSubject(client.clientId)
    .setIssuer(ISSUER)
    .setAudience(client.audience)
    .setJti(crypto.randomUUID())
    .setIssuedAt()
    .setExpirationTime("10m")
    .sign(privateKey);

  res.set("Cache-Control", "no-store").json({ access_token: token, token_type: "Bearer", expires_in: 600, scope: granted.join(" ") });
});
```

Notes: errors follow RFC 6749 (`invalid_client`, `invalid_scope`, `unsupported_grant_type`); responses carry `Cache-Control: no-store`; secrets are random 256-bit values stored **hashed** ([01-token-hashing.md](../03-tokens/01-token-hashing.md)); keys and JWKS follow [07-jwt-algorithms-and-keys.md](../03-tokens/07-jwt-algorithms-and-keys.md). For anything beyond a few internal clients, prefer a real AS.

---

## Operating machine credentials

| Practice | Why |
|---|---|
| Separate client per service and per environment | Limits blast radius; precise audit trails |
| Least-privilege scopes | A leaked credential does limited harm |
| Short token lifetimes (5-15 min) | Machine tokens are easy to fetch again; no need for long ones |
| Store secrets in a secret manager, never in the repo/image | [01-env-and-secrets.md](../00-setup/01-env-and-secrets.md) |
| Rotation with overlap: two active secrets (or keys), switch clients, retire the old | No downtime |
| Prefer `private_key_jwt`/mTLS/workload identity over shared secrets | No secret to leak or rotate at the AS |
| Rate-limit the token endpoint; alert on repeated `invalid_client` | Credential stuffing against client secrets |
| Log client id, scopes, outcome; never secrets or tokens | Auditing without leaks |

### API keys vs client credentials

| | API key | Client credentials |
|---|---|---|
| Format | Static random string | Short-lived signed token obtained with a secret |
| Lifetime | Long-lived | Minutes |
| Standardization | None | OAuth 2.0 |
| Leak impact | Valid until revoked | Credential still valuable, but each token expires |
| Simplicity | Very simple | More setup |

API keys are fine for simple third-party access (generate randomly, show once, store hashed, scope, allow revocation, prefix for identification: [01-credential-types.md](../01-fundamentals/01-credential-types.md)). Move to client credentials when you need expiry, scopes, standard tooling, or many integrations.

---

## Common mistakes

| Mistake | Consequence |
|---|---|
| Using client credentials in a SPA/mobile app | Secret extracted |
| No `aud` validation at the resource server | A token for one API works on another |
| Scopes issued but not enforced | Every client is effectively admin |
| Requesting a new token on every call | Latency, load on the AS, rate-limit hits |
| Long-lived machine tokens (days) | Weak revocation, bigger theft window |
| Sharing one client across services/environments | No isolation or accountability |
| Secrets in git, env dumps, logs, or CI output | Credential theft |
| Accepting user tokens and machine tokens interchangeably | Privilege confusion; check `sub`/client type and required scopes |

## Debugging

| Symptom | Cause |
|---|---|
| `invalid_client` | Wrong id/secret, wrong auth method (Basic vs body), unencoded special characters in the secret |
| `invalid_scope` | Requested scope not allowed for that client |
| `401` from the API with a valid-looking token | `aud`/`iss` mismatch, expired token (clock skew), wrong JWKS URL |
| Intermittent `401` after key rotation | Resource server cached old JWKS; keys rotated without overlap |
| Token works on API A but not B | Different required audience |
| Works once, then fails | Cached token expired and the refresh path (re-request) is missing |

---

## Quick summary

- Client Credentials = a service authenticates as itself and gets a short-lived access token; no user, no refresh token, confidential clients only.
- Callers cache tokens and re-request before expiry; resource servers verify signature, `iss`, **`aud`**, `exp` and enforce **scopes**.
- Prefer a standard AS or cloud workload identity; build your own only for small internal cases.
- Per-service credentials, least privilege, short lifetimes, secrets in a manager, rotation with overlap.

**Next:** [Authorization: RBAC](../07-authorization/00-rbac.md)
