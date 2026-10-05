# Cognito

Amazon Cognito **user pools** are a managed user directory and login service: sign-up, email/phone verification, password reset, MFA, social/SAML logins and **JWTs** your API can trust. The practical question for a Node backend is rarely "how do I build login screens" but **"how do I verify the token this request carries"**. That's the core of this note, with the sign-up/sign-in calls covered first so the tokens make sense.

Prerequisites: [SDK v3 and credentials](../01-setup/01-sdk-v3-and-credentials.md).

```bash
npm install @aws-sdk/client-cognito-identity-provider aws-jwt-verify
```

---

## Pieces

- **User pool**: the directory. Has a `userPoolId` (like `ap-south-1_AbC123`) and lives in a region.
- **App client**: an application registered with the pool, with a `clientId` (and optionally a secret). Your web/mobile/server app authenticates *through* an app client, and each client chooses which auth flows it allows.
- **Tokens**: three JWTs come back from a successful login.

| Token | Purpose | Send it to your API? |
|---|---|---|
| **ID token** | Who the user is (email, name, custom attributes) | For profile data in your frontend; normally **not** as API credential |
| **Access token** | What the user may access (scopes, groups); proves authentication | **Yes**: the bearer token for your API |
| **Refresh token** | Gets new ID/access tokens | Only to Cognito, never to your API |

Access and ID tokens are short-lived (1 hour by default, configurable); refresh tokens last longer (30 days by default, configurable).

---

## Two ways to run login

**1. Hosted UI / managed login + OAuth (recommended for web apps).** The browser redirects to Cognito's login page, users sign in (including social/SAML providers), and you get tokens via the OAuth authorization-code flow with PKCE. Your backend does *no* credential handling; it only verifies tokens.

**2. Your own screens + SDK calls.** Your frontend or backend calls Cognito's API (below). More control, more responsibility. Typical when you need a fully custom UX or a backend-for-frontend.

Either way, your API's job is the same: **verify the access token**.

---

## The sign-up and sign-in flow (SDK calls)

```text
SignUp ──▶ (Cognito emails/SMS a code) ──▶ ConfirmSignUp ──▶ InitiateAuth ──▶ tokens
```

```ts
import {
  CognitoIdentityProviderClient, SignUpCommand, ConfirmSignUpCommand,
  InitiateAuthCommand, GlobalSignOutCommand,
} from "@aws-sdk/client-cognito-identity-provider";

const cognito = new CognitoIdentityProviderClient({ region: process.env.AWS_REGION });
const ClientId = process.env.COGNITO_CLIENT_ID!;

// 1. register
await cognito.send(new SignUpCommand({
  ClientId,
  Username: "asha@example.com",
  Password: "S0me-long-passphrase!",
  UserAttributes: [{ Name: "email", Value: "asha@example.com" }],
}));

// 2. confirm with the code the user received
await cognito.send(new ConfirmSignUpCommand({
  ClientId, Username: "asha@example.com", ConfirmationCode: "123456",
}));

// 3. sign in
const out = await cognito.send(new InitiateAuthCommand({
  ClientId,
  AuthFlow: "USER_PASSWORD_AUTH",
  AuthParameters: { USERNAME: "asha@example.com", PASSWORD: "S0me-long-passphrase!" },
}));

if (out.AuthenticationResult) {
  const { AccessToken, IdToken, RefreshToken, ExpiresIn } = out.AuthenticationResult;
} else if (out.ChallengeName) {
  // e.g. "NEW_PASSWORD_REQUIRED", "SOFTWARE_TOKEN_MFA": respond with
  // RespondToAuthChallengeCommand({ ClientId, ChallengeName, Session: out.Session, ChallengeResponses: {...} })
}
```

Rules and gotchas:

- `USER_PASSWORD_AUTH` must be **enabled on the app client** (the "ALLOW_USER_PASSWORD_AUTH" explicit flow). The more secure SRP flow (`USER_SRP_AUTH`) never sends the password; it's complex enough that you use a library (Amplify Auth or `amazon-cognito-identity-js`) in the client rather than hand-rolling it.
- A login result is **either** `AuthenticationResult` (done) **or** a `ChallengeName` (more steps, such as MFA or a forced password change). Handle both.
- **App client with a secret**: every call needs `SECRET_HASH`, the base64 of an HMAC-SHA256 of `username + clientId` keyed by the client secret. Browser/mobile clients should use clients *without* secrets; secrets belong only on a trusted server.

```ts
import { createHmac } from "node:crypto";
const secretHash = (username: string) =>
  createHmac("sha256", process.env.COGNITO_CLIENT_SECRET!).update(username + ClientId).digest("base64");
// SignUp: SecretHash: secretHash(u);  InitiateAuth: AuthParameters: { USERNAME, PASSWORD, SECRET_HASH: secretHash(u) }
```

- **Refresh**: `AuthFlow: "REFRESH_TOKEN_AUTH"` with `AuthParameters: { REFRESH_TOKEN }` returns new ID and access tokens (not a new refresh token by default).
- **Sign out everywhere**: `GlobalSignOutCommand({ AccessToken })` revokes the user's refresh tokens. Already-issued access tokens stay valid until they expire. JWTs can't be recalled, which is why they're short-lived.
- `SignUp`, `ConfirmSignUp` and `InitiateAuth` are public operations authorized by the app client ID, not by AWS IAM credentials. The `Admin*` operations (`AdminCreateUser`, `AdminGetUser`, `AdminAddUserToGroup`, …) **are** IAM-authorized and meant for trusted backends.

---

## Verifying the JWT in your API

Verifying means checking, in order: **signature**, **issuer**, **token type**, **client/audience**, **expiry**. Decoding a JWT without verifying it proves nothing: anyone can craft one.

Use AWS's own library:

```ts
import { CognitoJwtVerifier } from "aws-jwt-verify";

const verifier = CognitoJwtVerifier.create({
  userPoolId: process.env.COGNITO_USER_POOL_ID!,
  tokenUse: "access",                       // "access" or "id"
  clientId: process.env.COGNITO_CLIENT_ID!, // string or string[]
});

export async function authenticate(authorizationHeader?: string) {
  const token = authorizationHeader?.replace(/^Bearer /i, "");
  if (!token) throw new Error("missing token");
  return verifier.verify(token); // throws if signature, issuer, client, type or expiry is wrong
}
```

As Express middleware:

```ts
app.use(async (req, res, next) => {
  try {
    const claims = await authenticate(req.headers.authorization);
    req.user = { id: claims.sub, groups: claims["cognito:groups"] ?? [] };
    next();
  } catch {
    res.status(401).json({ error: "unauthorized" });
  }
});
```

What the library does for you: downloads and **caches the pool's public keys** (JWKS) from `https://cognito-idp.<region>.amazonaws.com/<userPoolId>/.well-known/jwks.json`, picks the key matching the token's `kid`, verifies the RS256 signature, and checks `iss`, `token_use`, `exp` and the client ID. Call `verifier.hydrate()` at startup if you want the keys loaded before the first request (useful for Lambda cold starts).

Useful claims:

| Claim | Meaning |
|---|---|
| `sub` | Stable unique user ID. **Use this as your user key**, not email or username |
| `cognito:groups` | Groups the user belongs to (for role checks) |
| `scope` | OAuth scopes on an access token |
| `client_id` (access) / `aud` (ID) | App client the token was issued to |
| `token_use` | `"access"` or `"id"` |
| `exp` | Expiry (epoch seconds) |
| `email`, `custom:*` | Profile and custom attributes (ID token) |

**Don't accept an ID token where you expect an access token.** `tokenUse` in the verifier prevents mix-ups.

If you're on API Gateway, you may not need this code: HTTP APIs have a built-in **JWT authorizer** that validates Cognito tokens before your Lambda runs ([API Gateway APIs](../03-lambda/02-api-gateway-apis.md)). Verify in your own code when you run on ECS/Express or need finer checks.

---

## Authorization: groups and triggers

- Put users in **groups** (`admin`, `editor`) and read `cognito:groups` from the token for coarse roles. For fine-grained permissions, store them in your own database keyed by `sub`.
- **Lambda triggers** let you customize the pool: *pre sign-up* (auto-confirm, block domains), *post confirmation* (create the user's row in your database), *pre token generation* (add custom claims), custom messages, and more. "Create the app-side profile after confirmation" is the most common one.

---

## Cognito vs building your own auth

| | Cognito | Own auth (e.g. Passport + DB) |
|---|---|---|
| Password storage, hashing, breach handling | Managed | Yours to get right |
| Email/SMS verification, MFA, reset flows | Built in | You build and maintain them |
| Social / SAML / OIDC federation | Built in | Integrations per provider |
| UX and flexibility | Constrained by Cognito's model | Total control |
| Portability | Users are locked in; exporting password hashes isn't possible | Yours |
| Cost | Per monthly active user (check current pricing tiers) | Your infrastructure and time |
| Ops | None | Security patching, rate limiting, abuse handling |

Reasonable rule: use a managed provider unless you have a strong reason not to; auth is expensive to get wrong. Be aware of Cognito's rough edges, below.

---

## Rough edges worth knowing

- **Some pool settings can't be changed after creation**: whether users sign in with email/username/phone, and the schema of standard/custom attributes (you can add custom attributes but not remove or change them). Decide up front.
- **Default email sending is limited** (a low daily cap). For real traffic, configure Cognito to send through [SES](./05-ses-email.md).
- **User enumeration**: enable `PreventUserExistenceErrors` on the app client so login/reset errors don't reveal whether an account exists.
- Tokens are **not revocable** individually before expiry. Keep access tokens short-lived.
- Rate limits apply to the auth APIs (`TooManyRequestsException`); don't call `InitiateAuth` per request. Verify JWTs locally instead.

---

## Common mistakes and errors

| Symptom | Cause | Fix |
|---|---|---|
| `NotAuthorizedException: USER_PASSWORD_AUTH flow not enabled for this client` | App client doesn't allow that flow | Enable the explicit auth flow on the client |
| `Unable to verify secret hash for client` | Client has a secret, call lacks `SECRET_HASH` | Compute and send it (server side only) |
| `UserNotConfirmedException` | User never entered the confirmation code | Prompt confirmation; `ResendConfirmationCodeCommand` |
| `UsernameExistsException` | Account already registered | Handle gracefully (mind enumeration) |
| `CodeMismatchException` / `ExpiredCodeException` | Wrong or stale code | Resend code |
| `InvalidPasswordException` | Violates pool password policy | Show policy; validate client-side |
| Verifier throws "Token use not allowed" / "Client ID not allowed" | ID token sent instead of access token, or wrong client | Send the access token; check `clientId` config |
| Valid-looking token rejected | Wrong `userPoolId` or region in verifier | Match the pool that issued it |
| Using `email` as the user key and breaking when it changes | Emails are mutable attributes | Key on `sub` |
| Decoding with `jwt.decode()` and trusting it | No verification | Verify with the verifier |

---

## Quick summary

- A user pool issues three JWTs: **access** (authorize your API), **ID** (profile), **refresh** (renew).
- Web apps: use hosted UI / OAuth + PKCE and let the backend only verify. Custom flows: `SignUp → ConfirmSignUp → InitiateAuth`, handling challenges.
- Verify with `aws-jwt-verify` (`CognitoJwtVerifier`, `tokenUse: "access"`), key your users on `sub`, read roles from `cognito:groups`.
- `SECRET_HASH` is needed only for clients with a secret, and only on trusted servers.
- Plan for the immutable bits (sign-in attributes, schema), SES for email, short token lifetimes, and `PreventUserExistenceErrors`.

## Next

[Bedrock](./07-bedrock.md): calling foundation models from the SDK and LangChain.js.
