# JWT Algorithms and Keys

How the JWT signature actually works, which algorithm to pick, and how to manage and rotate keys, including publishing them with JWKS so other services can verify tokens without sharing a secret.

**Prerequisites:** [02-jwt-access-tokens.md](./02-jwt-access-tokens.md), [03-crypto-primitives.md](../01-fundamentals/03-crypto-primitives.md)

---

## Symmetric vs asymmetric

```text
Symmetric (HS256)                    Asymmetric (RS256, ES256, EdDSA)
─────────────────                    ────────────────────────────────
One shared secret                    Private key signs, public key verifies
Signer == verifier                   Many verifiers can exist safely
Secret leaks → anyone can forge      Public key leak is harmless
Fast, simple                         Slightly more setup
```

| Algorithm | Type | Notes |
|---|---|---|
| `HS256` | HMAC-SHA256 (symmetric) | Great for a single API that both signs and verifies |
| `RS256` | RSA-SHA256 | Most widely supported; large keys/signatures |
| `ES256` | ECDSA P-256 | Small, fast, widely supported |
| `EdDSA` | Ed25519 | Small, fast, modern; support slightly less universal |
| `none` | no signature | **Never accept** |

### Choosing

- **One monolith signs and verifies its own tokens:** `HS256` with a strong secret is simple and fine. That is this repo's default.
- **Several services verify tokens, or third parties do:** asymmetric. Only the auth service holds the private key; everyone else gets the public key. A compromised downstream service can't mint tokens.
- **Interoperability concerns** (older libraries, third-party verifiers): `RS256`/`ES256`. Greenfield with control over clients: `EdDSA` is a good choice.

HS256 secrets must be random and at least 256 bits; a short or human-chosen secret can be brute-forced offline from any token you issue.

---

## Algorithm confusion and `alg: none`

The `alg` in the header is supplied by the attacker. If your verifier trusts it:

- **`none`:** attacker sends an unsigned token and the library "verifies" it.
- **RS256 → HS256 confusion:** you verify with an RSA public key (which is public). The attacker sets `alg: HS256` and signs with that public key as if it were an HMAC secret. A naive verifier uses the same key bytes for HMAC and accepts it.

Defense is one line: **always pass an explicit `algorithms` allowlist** and use a key type that matches.

```js
jwt.verify(token, key, { algorithms: ["HS256"] });
```

---

## HS256 key rotation with `kid`

The `kid` (key id) header says which key signed the token, allowing overlapping keys during rotation:

```js
const keys = {
  k2: process.env.ACCESS_SECRET_K2,   // current: signs and verifies
  k1: process.env.ACCESS_SECRET_K1,   // previous: verifies only, remove after one TTL
};
const CURRENT = "k2";

export const sign = (payload, opts) =>
  jwt.sign(payload, keys[CURRENT], { ...opts, algorithm: "HS256", keyid: CURRENT });

export const verify = (token, opts) => {
  const header = jwt.decode(token, { complete: true })?.header;
  const key = keys[header?.kid];
  if (!key) throw new Error("Unknown key id");   // unknown kid → reject
  return jwt.verify(token, key, { ...opts, algorithms: ["HS256"] });
};
```

Reading the header with `decode` just to pick a key is safe **because** you then do a real `verify` with an allowlisted algorithm and an explicit key map. Never use `kid` to build a file path or DB query without validation; treat it as untrusted input.

Rotation schedule: add `k2`, switch signing to `k2`, wait longer than the access TTL (10 min), delete `k1`. See also [env-and-secrets](../00-setup/01-env-and-secrets.md).

---

## Asymmetric keys and JWKS with `jose`

`jose` is a modern library supporting all of the above with a clean async API and JWKS handling.

```bash
npm i jose
```

### Generate a key pair

```js
import { generateKeyPair, exportJWK } from "jose";

const { publicKey, privateKey } = await generateKeyPair("EdDSA");
const publicJwk = { ...(await exportJWK(publicKey)), kid: "2026-10", alg: "EdDSA", use: "sig" };
```

Store the private key in a secret manager (PEM/PKCS8 or JWK), never in the repo.

### Sign

```js
import { SignJWT } from "jose";

const jwt = await new SignJWT({ sid, role })
  .setProtectedHeader({ alg: "EdDSA", kid: "2026-10" })
  .setSubject(userId)
  .setIssuer("https://auth.example.com")
  .setAudience("your-api")
  .setJti(crypto.randomUUID())
  .setIssuedAt()
  .setExpirationTime("10m")
  .sign(privateKey);
```

### Publish the public keys (JWKS)

A JWKS is a JSON document of public keys, conventionally served at `/.well-known/jwks.json`:

```js
app.get("/.well-known/jwks.json", (req, res) => {
  res.set("Cache-Control", "public, max-age=300");
  res.json({ keys: [publicJwk /* , previousPublicJwk during rotation */] });
});
```

### Verify elsewhere

```js
import { createRemoteJWKSet, jwtVerify } from "jose";

const JWKS = createRemoteJWKSet(new URL("https://auth.example.com/.well-known/jwks.json"));

const { payload } = await jwtVerify(token, JWKS, {
  issuer: "https://auth.example.com",
  audience: "your-api",
  algorithms: ["EdDSA"],
});
```

`createRemoteJWKSet` fetches and caches the keys and picks the right one by `kid`, refetching when it sees an unknown `kid`. For HS256 in `jose`, pass a `Uint8Array` secret (`new TextEncoder().encode(secret)`).

---

## Asymmetric key rotation

```text
1. Generate new key pair (kid: B). Publish BOTH A and B in the JWKS.
2. Wait for caches to pick up B (cache TTL).
3. Start signing with B.
4. After max token lifetime, remove A from the JWKS.
```

Publishing before signing is the important ordering: a verifier must already know the key when the first token signed with it arrives.

Emergency rotation (key compromised): sign with a new key immediately, **remove** the old key from the JWKS at once, accept that all outstanding access tokens fail, and let clients refresh. Revoke sessions too if you suspect tokens were forged. See [incident-response](../10-production/03-incident-response.md).

---

## Which claims/headers to validate

| Check | How |
|---|---|
| Signature | Library, with explicit key |
| Algorithm | `algorithms` allowlist |
| Expiry (`exp`, `nbf`) | Library, plus small `clockTolerance` |
| Issuer / audience | Options to verify |
| `typ` (optional) | Reject tokens not meant as access tokens (e.g. an ID token) |

---

## Common mistakes

- No `algorithms` allowlist (algorithm confusion).
- Short or guessable HS256 secret.
- Sharing one HS256 secret with other services (each becomes able to forge).
- Using the same key for different environments (staging tokens valid in prod).
- Removing the old key too early (valid tokens suddenly fail) or too late.
- Serving the **private** key by mistake in the JWKS; only public keys belong there.
- Trusting `jku`/`x5u`/`jwk` headers that let a token point to its own key URL. Ignore those; use only your configured key source.

## Debugging

| Symptom | Cause |
|---|---|
| `invalid signature` after deploy | Different secret/key per instance, or rotated without overlap |
| `no applicable key found in the JSON Web Key Set` | `kid` not in JWKS (not yet published, or already removed) |
| `"alg" (Algorithm) Header Parameter value not allowed` | Token's alg isn't in your allowlist (check signer config) |
| Verification fails only on some servers | Env var missing or whitespace/newline in the key |

---

## Quick summary

- HS256 for a single signer/verifier; asymmetric (EdDSA/ES256/RS256) when others verify.
- Always allowlist algorithms; never accept `none`.
- Use `kid` and overlapping keys to rotate without logging everyone out.
- Publish public keys via JWKS; publish before signing, remove after the max token lifetime.

**Next:** [08-email-verification-tokens.md](./08-email-verification-tokens.md)
