# WebAuthn and Passkeys

WebAuthn is the browser standard for authenticating with **public-key cryptography** instead of shared secrets. The user's device holds a private key that never leaves it; the server stores only the matching public key. A **passkey** is a WebAuthn credential that is typically synced across a user's devices (iCloud Keychain, Google Password Manager, a password manager) and unlocked with a fingerprint, face, or PIN.

It is the strongest option in this chapter because it is **phishing-resistant**.

**Prerequisite:** [00-mfa-overview.md](./00-mfa-overview.md)

---

## Why it's different

```text
Password / TOTP:   the user types a secret → can be typed into a fake site
WebAuthn:          the browser signs a challenge + the site's real origin
                   → a fake site gets a signature that is useless on the real one
```

| Property | Result |
|---|---|
| Server stores a public key | A database leak exposes nothing usable |
| Private key never leaves the authenticator | Nothing to steal in transit or phish |
| Credential bound to the relying party ID (your domain) | Doesn't work on `evil.com` |
| Per-site, per-account keys | No cross-site tracking or reuse |
| Local user verification (biometric/PIN) | Possession + inherence/knowledge in one gesture |

---

## Vocabulary

| Term | Meaning |
|---|---|
| **Relying Party (RP)** | Your site. `rpID` is its domain (e.g. `example.com`) |
| **Authenticator** | Platform (Touch ID, Windows Hello, phone) or roaming (YubiKey) |
| **Credential** | A key pair for one account at one RP, with an id |
| **Registration** (create) | Make a new credential and send the public key |
| **Authentication** (get) | Sign a server challenge with the private key |
| **Discoverable credential / resident key** | Lets the authenticator find the account without a username → enables passwordless "sign in with a passkey" |
| **Challenge** | Random server value that must be signed, preventing replay |
| **User verification** | The authenticator checking the user locally |

---

## Setup

```bash
npm i @simplewebauthn/server       # backend
npm i @simplewebauthn/browser      # frontend
```

`@simplewebauthn` changed some parameter and field names across major versions (for example how the user id and credential fields are passed). The code below follows the recent API; check the docs for your installed version if a call or property doesn't match.

Config:

```js
const rpName = "YourApp";
const rpID = process.env.WEBAUTHN_RP_ID;          // "example.com" in prod, "localhost" in dev
const origin = process.env.FRONTEND_URL;          // exact: "https://app.example.com"
```

- `rpID` must be the site's domain (or a registrable parent of it), **without** scheme or port.
- `origin` must match the page's origin **exactly**, including port.
- WebAuthn requires HTTPS, except on `localhost`.

---

## Data model

One user, many credentials:

```js
const passkeySchema = new mongoose.Schema({
  userId:       { type: mongoose.Schema.Types.ObjectId, ref: "User", index: true },
  credentialId: { type: String, unique: true },       // base64url
  publicKey:    { type: Buffer, required: true },
  counter:      { type: Number, default: 0 },
  transports:   [String],                             // "internal", "usb", "ble", "nfc", "hybrid"
  deviceType:   String,                               // "singleDevice" | "multiDevice"
  backedUp:     Boolean,                              // synced passkey?
  name:         String,                               // "MacBook Touch ID" (user-editable)
  createdAt:    { type: Date, default: Date.now },
  lastUsedAt:   Date,
});
```

---

## Registration

```text
Browser                        Server
   │  POST /webauthn/register/options    │
   │ ──────────────────────────────────► │  generate options + challenge
   │ ◄────────────────────────────────── │  store challenge (Redis, TTL 5m, single-use)
   │                                      │
   │  navigator.credentials.create(...)   │
   │  (user touches key / biometric)      │
   │                                      │
   │  POST /webauthn/register/verify      │
   │ ──────────────────────────────────► │  verify signature + origin + rpID + challenge
   │ ◄────────────────────────────────── │  save public key
```

```js
import { generateRegistrationOptions, verifyRegistrationResponse } from "@simplewebauthn/server";

export async function registrationOptions(req, res) {
  const user = await User.findById(req.user.sub);
  const existing = await Passkey.find({ userId: user.id });

  const options = await generateRegistrationOptions({
    rpName,
    rpID,
    userName: user.email,
    userID: new TextEncoder().encode(user.id),          // stable, opaque id; not the email
    attestationType: "none",                            // we don't need device attestation
    excludeCredentials: existing.map((p) => ({ id: p.credentialId, transports: p.transports })),
    authenticatorSelection: { residentKey: "preferred", userVerification: "preferred" },
  });

  await redis.set(`webauthn:reg:${user.id}`, options.challenge, { EX: 300 });
  res.json(options);
}

export async function registrationVerify(req, res) {
  const expectedChallenge = await redis.getDel(`webauthn:reg:${req.user.sub}`);   // single-use
  if (!expectedChallenge) return res.status(400).json({ message: "Expired" });

  const { verified, registrationInfo } = await verifyRegistrationResponse({
    response: req.body,
    expectedChallenge,
    expectedOrigin: origin,
    expectedRPID: rpID,
  });
  if (!verified || !registrationInfo) return res.status(400).json({ message: "Verification failed" });

  const { credential, credentialDeviceType, credentialBackedUp } = registrationInfo;
  await Passkey.create({
    userId: req.user.sub,
    credentialId: credential.id,
    publicKey: Buffer.from(credential.publicKey),
    counter: credential.counter,
    transports: credential.transports,
    deviceType: credentialDeviceType,
    backedUp: credentialBackedUp,
  });

  res.json({ ok: true });
}
```

Frontend:

```js
import { startRegistration } from "@simplewebauthn/browser";

const optionsJSON = await api.post("/webauthn/register/options").then((r) => r.data);
const attestation = await startRegistration({ optionsJSON });    // older versions pass the options directly
await api.post("/webauthn/register/verify", attestation);
```

`excludeCredentials` prevents registering the same authenticator twice. `attestationType: "none"` is right for almost every consumer app; attestation is for enterprises that must restrict allowed hardware models.

---

## Authentication

```js
import { generateAuthenticationOptions, verifyAuthenticationResponse } from "@simplewebauthn/server";

export async function authOptions(req, res) {
  // As a second factor, you know the user (from the MFA challenge) → list their credentials.
  const userId = await beginAttempt(req.body.challengeToken);            // from 00-mfa-overview.md
  const creds = await Passkey.find({ userId });

  const options = await generateAuthenticationOptions({
    rpID,
    allowCredentials: creds.map((c) => ({ id: c.credentialId, transports: c.transports })),
    userVerification: "preferred",
  });

  await redis.set(`webauthn:auth:${hashToken(req.body.challengeToken)}`, options.challenge, { EX: 300 });
  res.json(options);
}

export async function authVerify(req, res) {
  const { challengeToken, response } = req.body;
  const userId = await beginAttempt(challengeToken);
  const expectedChallenge = await redis.getDel(`webauthn:auth:${hashToken(challengeToken)}`);
  if (!expectedChallenge) return res.status(400).json({ message: "Expired" });

  const passkey = await Passkey.findOne({ credentialId: response.id, userId });  // must belong to THIS user
  if (!passkey) return res.status(400).json({ message: "Verification failed" });

  const { verified, authenticationInfo } = await verifyAuthenticationResponse({
    response,
    expectedChallenge,
    expectedOrigin: origin,
    expectedRPID: rpID,
    credential: {
      id: passkey.credentialId,
      publicKey: new Uint8Array(passkey.publicKey),
      counter: passkey.counter,
      transports: passkey.transports,
    },
  });
  if (!verified) return res.status(400).json({ message: "Verification failed" });

  passkey.counter = authenticationInfo.newCounter;
  passkey.lastUsedAt = new Date();
  await passkey.save();

  await finishChallenge(challengeToken);
  // → create session with amr: ["hwk"/"swk", "user"]
}
```

Frontend: `startAuthentication({ optionsJSON })`.

Key checks to never skip: the challenge is **single-use and server-stored**, the credential **belongs to the claimed user**, and `expectedOrigin`/`expectedRPID` are your exact values (the library enforces them in verification).

### Passwordless (passkey as the first factor)

Call `generateAuthenticationOptions` **without** `allowCredentials` (discoverable credentials). The browser shows the user's passkeys for your site; the response includes a `userHandle` identifying the account. Look up the credential by `response.id`, verify as above, and create the session. Because the passkey already includes user verification, you can treat it as MFA-strength on its own.

Browser autofill ("conditional UI") for passkeys lets them appear in the username field's autocomplete; see the library docs.

---

## The signature counter

Authenticators may increment a counter on each use so the server can detect a **cloned** authenticator (counter goes backwards). Caveats:

- Synced passkeys (multi-device) often keep the counter at `0` forever.
- If both stored and received counters are `0`, that's expected, not an attack.
- If a nonzero counter fails to increase, treat it as suspicious (log, alert, possibly revoke that credential); the library flags this during verification.

---

## Recovery and fallbacks

Passkeys change *where* the recovery problem sits:

- **Synced passkeys** survive a lost phone if the platform account (Apple/Google/password manager) is recoverable.
- **Hardware keys** are lost with the key: require at least two registered authenticators or recovery codes.
- Let users **name, list and remove** passkeys. Removing the last strong factor requires step-up and a notification.
- Keep a fallback factor (TOTP, recovery codes) unless you are deliberately passkey-only.

---

## Common mistakes

| Mistake | Consequence |
|---|---|
| `rpID` or `origin` wrong (port, subdomain, `http`) | Registration/verification fails, or credentials bind to the wrong domain |
| Reusing or not storing challenges | Replay of old responses |
| Not tying the credential id to the user | One user authenticates as another |
| Treating `counter` of 0 as an attack | False alarms with synced passkeys |
| Storing public keys as strings with lossy encoding | Verification mysteriously fails; keep bytes (Buffer) |
| Requiring a passkey with no fallback | Lockouts |
| Changing your domain later | **Passkeys are bound to `rpID` and stop working**; choose it carefully |
| Mixing up `userID` (opaque id) with the display name | Leaks or collisions; use a stable random-ish id, not the email |

## Debugging

| Symptom | Cause |
|---|---|
| `NotAllowedError` in the browser | User cancelled, timed out, or the page isn't focused/secure |
| `SecurityError: The relying party ID is not a registrable domain suffix` | `rpID` doesn't match the page's domain |
| "Unexpected registration response origin" | `expectedOrigin` differs from `location.origin` (scheme/port) |
| `Authenticator not registered for this account` | Credential not found for that user/credentialId |
| Works on desktop, fails in an embedded webview or HTTP page | WebAuthn needs a secure context and browser support |
| `InvalidStateError` on register | Credential already exists on that authenticator (`excludeCredentials` working as intended) |

Test locally on `http://localhost` with `rpID = "localhost"` and `origin = "http://localhost:5173"`; Chrome DevTools has a **WebAuthn** panel that simulates authenticators.

---

## Quick summary

- WebAuthn stores only public keys; private keys stay on the device and responses are bound to your origin, so it is phishing-resistant.
- Registration and authentication are challenge-response flows; challenges must be random, server-stored, short-lived and single-use.
- Set `rpID` and `origin` exactly; the domain choice is hard to change later.
- Verify the credential belongs to the user; treat the counter carefully.
- Plan recovery: multiple authenticators, fallback factor, recovery codes.

**Next:** [05-step-up-auth.md](./05-step-up-auth.md)
