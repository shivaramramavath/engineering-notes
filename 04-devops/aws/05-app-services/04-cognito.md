# Cognito: Authentication for Your App's Users

Cognito handles **sign-up, sign-in, tokens and (optionally) AWS access for the users of your application**: the customers who log in to your web or mobile app, not the engineers who manage your AWS account. (That second group is what [IAM and IAM Identity Center](../01-foundations/03-iam.md) are for. Mixing the two up is the most common Cognito confusion.)

It saves you from building password storage, email verification, password reset, MFA and social login yourself. In return you accept Cognito's model, its quirks, and some inflexibility.

Prerequisites: [IAM](../01-foundations/03-iam.md); [API Gateway](../04-networking/03-load-balancing-and-api-gateway.md) helps for the "protect an API" section.

---

## Two services under one name

| | **User pools** | **Identity pools** (federated identities) |
|---|---|---|
| What it is | A **user directory + identity provider (IdP)** | A broker that exchanges identity for **temporary AWS credentials** |
| Gives you | Sign-up/in, **JWT tokens** (ID, access, refresh), MFA, hosted login, social/SAML/OIDC federation | AWS credentials (via STS) scoped by an IAM role |
| Used for | **Authenticating users to your app and APIs**: by far the common case | Letting a client call AWS services *directly* (e.g. upload to S3 from the browser) |

Most apps only need a **user pool**. Add an identity pool only when the client itself must call AWS APIs. Often a backend plus presigned S3 URLs is simpler and safer.

```
Browser ──► Cognito user pool (managed login / SDK) ──► tokens (JWT)
   │
   └── Authorization: Bearer <access or ID token> ──► API Gateway / ALB / your backend
                                                          verifies the JWT signature + claims
```

---

## User pool concepts

| Concept | Meaning |
|---|---|
| **User pool** | The directory of users and its settings. |
| **App client** | A registered application (SPA, mobile, backend) with its own allowed flows, callback URLs and tokens. Create **one per app**. |
| **Managed login / hosted UI** | Cognito-hosted pages for sign-up, sign-in, MFA and password reset, on your domain or an AWS prefix domain. |
| **Groups** | Collections of users, surfaced in the token as `cognito:groups`. A simple way to do roles (`admin`, `member`). |
| **Attributes** | Standard (`email`, `phone_number`, …) and custom (`custom:tenantId`). |
| **Lambda triggers** | Hooks into the lifecycle: pre sign-up, post confirmation, pre token generation, custom auth challenges, and more. |
| **Federation** | Sign in with Google/Apple/Facebook, or enterprise **SAML/OIDC** IdPs, mapped into the same user pool. |

### Feature plans

User pools run on one of three **feature plans**: **Lite**, **Essentials** (the default for new pools), and **Plus**. Roughly: Lite covers basic sign-in and social login; Essentials adds **managed login** (the newer hosted pages), passwordless options such as **passkeys**, email MFA and custom access tokens; Plus adds threat protection (adaptive authentication, compromised-credential detection). Billing is per **monthly active user (MAU)**, with a free allowance on Lite and Essentials. Check the [Cognito pricing page](https://aws.amazon.com/cognito/pricing/) for current rates, and note that pools created before the plans were introduced may sit on Lite.

---

## Tokens: what you get back

After sign-in, a user pool returns three JWTs:

| Token | Purpose | Typical lifetime (configurable) |
|---|---|---|
| **ID token** | Who the user is (claims: `sub`, `email`, `cognito:groups`, custom attributes) | Short (default 1 hour) |
| **Access token** | Authorises API calls (scopes, groups); also used against Cognito's own user APIs | Short (default 1 hour) |
| **Refresh token** | Gets new ID/access tokens without re-login | Long (default 30 days) |

Rules of thumb:

- Send the **access token** (or ID token, if your backend wants user claims) to your API in `Authorization: Bearer …`. Be consistent. API Gateway's JWT authorizer, for instance, validates `aud`/`client_id` and issuer, and expects the token type you configured.
- **Never trust a token without verifying it**: signature (against the pool's public JWKS), issuer (`iss`), expiry (`exp`), audience/client, and `token_use`.
- The stable user identifier is **`sub`**, so use it as your database key rather than email or username (which can change).
- Keep refresh tokens out of places JavaScript can leak them (prefer httpOnly cookies handled by your backend where you can).

Verifying in a Node backend:

```ts
import { CognitoJwtVerifier } from "aws-jwt-verify";

const verifier = CognitoJwtVerifier.create({
  userPoolId: "ap-south-1_AbCdEfGhI",
  tokenUse: "access",             // or "id"
  clientId: "1h57kf5cpq17m0eml12EXAMPLE",
});

export async function authenticate(authHeader?: string) {
  const token = authHeader?.replace(/^Bearer /, "");
  if (!token) throw new Error("missing token");
  return verifier.verify(token);   // throws if invalid/expired; returns the claims
}
```

(`aws-jwt-verify` is AWS's own library; it fetches and caches the JWKS for you.)

---

## Sign-in flows

- **Managed login + OAuth 2.0 authorization code flow with PKCE**: the recommended approach for web and mobile apps. Users are redirected to Cognito's login pages, then back to your app with a code you exchange for tokens. You never handle passwords. Don't use the older implicit grant.
- **Direct API flows** (e.g. SRP via the SDK or Amplify libraries) when you build your own login UI. SRP means the password is never sent in plain text.
- **Passwordless / passkeys** on supported plans.
- **Client credentials** (machine-to-machine) using **resource servers and custom scopes**, so backend services can get tokens without a user.
- **Federation**: add Google, Apple, SAML or OIDC providers and map their attributes to pool attributes.

**App client secrets:** public clients (SPAs, mobile apps) **cannot keep a secret**, so create those app clients **without** a secret. Only server-side (confidential) clients should use one.

---

## Protecting an API

| Backend | How |
|---|---|
| **API Gateway HTTP API** | **JWT authorizer**: point it at the pool's issuer URL and audience/client ID; it validates tokens for you ([API Gateway](../04-networking/03-load-balancing-and-api-gateway.md)) |
| **API Gateway REST API** | **Cognito user pool authorizer** |
| **ALB** | Built-in **authenticate-cognito** listener action (OIDC) for browser-based apps |
| **Your own service** | Verify the JWT in middleware (as above) |

Authorisation (what a user may *do*) is still your job: use **groups**, **scopes**, or custom claims, and enforce them in the API/business logic. Cognito authenticates; it doesn't model fine-grained permissions.

---

## Customising with Lambda triggers

```ts
// Pre Token Generation trigger: add a custom claim (e.g. tenant) to tokens
export const handler = async (event) => {
  const tenantId = await lookupTenant(event.request.userAttributes.sub);
  event.response = {
    claimsOverrideDetails: {
      claimsToAddOrOverride: { tenantId },
    },
  };
  return event;     // you MUST return the event
};
```

Other common triggers: **pre sign-up** (auto-confirm, block disposable emails, link federated accounts), **post confirmation** (create the user's record in your database), **custom message** (customise emails/SMS). Triggers run **synchronously** in the auth path: keep them fast, make them reliable, and remember a thrown error blocks the user's sign-in. Give the user pool permission to invoke the function (resource-based policy on the Lambda).

---

## Things you must decide up front

These are painful to change after the pool has users:

- **Sign-in identifier** ("username attributes"): email, phone, or username. Set when creating the pool.
- **Required standard attributes** and their **mutability**: you **can't remove or change the type** of an attribute later, and can only *add* custom attributes. Plan your schema.
- **Self-registration or admin-only** sign-up.
- **Region**: a user pool lives in one region. Cross-region replication is a limited, newer, plan-dependent feature, so check the docs before relying on it for DR. Users can't simply be moved between pools (migrating means a user-migration Lambda trigger or a bulk import, and password hashes can't be exported).

Cognito's **default email sender** has very low daily sending limits and sends from an AWS address. For production, configure Cognito to send through **[SES](./03-ses-email.md)** from your own domain. SMS (for MFA/verification) goes through SNS with its own registration, regional rules and costs, so many teams prefer TOTP authenticator apps or email/passkeys instead.

---

## Identity pools (when you need AWS credentials in the client)

An identity pool takes a login (from a user pool, Google, etc.), and returns **temporary AWS credentials** for an **IAM role** you configure (separate roles for authenticated and unauthenticated "guest" users, or rules mapping groups/claims to roles). Typical use: a mobile app uploading directly to S3, scoped to the user's own prefix using the `${cognito-identity.amazonaws.com:sub}` policy variable. Keep these roles **extremely narrow**: anything in the role can be exercised by any user (or attacker) who can obtain the credentials. If a backend can mediate the call, presigned URLs are usually a safer design ([S3](../03-storage-and-databases/01-s3.md)).

---

## Common mistakes

- Confusing **user pools** (authentication) with **identity pools** (AWS credentials), and both with **IAM users**.
- **Not verifying JWTs** server-side, or verifying only the signature and ignoring `iss`/`aud`/`exp`/`token_use`.
- Using **email as the primary key** instead of `sub`.
- Using an **ID token where an access token is expected** (or vice versa) with an authorizer.
- Creating a **SPA/mobile app client with a secret**, which isn't secret.
- Using the **implicit grant** rather than authorization code + PKCE.
- Locking in a poor **attribute schema** or sign-in alias, which is hard to change.
- Relying on the **default Cognito email** in production, then hitting limits.
- Long, flaky **Lambda triggers** that make sign-in slow or fail.
- Treating **groups in the token** as instantly revocable (tokens live until expiry, so keep them short and design for it).
- Over-permissive **identity-pool roles**.
- Not planning **user migration** or a backup/export strategy.

---

## Debugging

| Symptom | Check |
|---|---|
| `redirect_mismatch` / invalid redirect | Callback URL in the app client must **exactly** match what the app sends (scheme, host, path, trailing slash) |
| `invalid_grant` on code exchange | Code reused/expired; PKCE `code_verifier` mismatch; wrong redirect URI |
| `NotAuthorizedException` | Wrong password, user disabled/unconfirmed, flow not enabled on the app client, or a secret hash missing/incorrect for a client with a secret |
| `UserNotConfirmedException` | User hasn't verified email/phone (or no auto-confirm) |
| API Gateway returns **401** with a token | Wrong token type (ID vs access), wrong audience/client, wrong issuer URL, or an expired token |
| Custom claim missing | Pre-token-generation trigger not attached / not returning the event / wrong trigger version for access-token customisation |
| Emails not arriving | Default sender limits; SES sandbox/identity if using SES; spam folder ([SES](./03-ses-email.md)) |
| Federation login loops or fails | IdP attribute mapping, SAML/OIDC metadata and clock, callback URLs |
| Sign-in slow/failing after adding a trigger | Trigger errors/timeouts, so check its CloudWatch logs |

Decode a token's claims (never trust it for security, just to inspect) at jwt.io or with `cut -d. -f2 | base64 -d`, and check **CloudWatch Logs** for triggers and **CloudTrail** for user-pool API calls.

---

## Quick Summary

- Cognito authenticates **your app's users**. IAM/Identity Center are for **your engineers**.
- **User pool** = directory + login + JWTs (ID / access / refresh). **Identity pool** = swap a login for temporary AWS credentials, which you rarely need.
- Use **managed login + authorization code with PKCE**; public clients have **no secret**.
- **Verify JWTs** (signature, `iss`, `aud`/client, `exp`, `token_use`); use **`sub`** as the user key.
- Protect APIs with **API Gateway JWT/Cognito authorizers**, an **ALB** Cognito action, or your own verifier. Authorisation logic is still yours (groups/scopes).
- **Plan the schema and sign-in method up front** (they're hard to change); use **SES** for email in production; keep **Lambda triggers** fast.
- Feature plans (**Lite / Essentials / Plus**) gate passkeys, managed login and threat protection. Check pricing.

**Next:** [Bedrock](./05-bedrock.md)
