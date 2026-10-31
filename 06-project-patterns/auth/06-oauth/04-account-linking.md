# Account Linking

One person, several ways to sign in: password, Google, GitHub, a passkey. **Account linking** is connecting those identities to the same local user. It is also the single riskiest part of social login: a careless rule turns "Sign in with Google" into an account-takeover tool.

**Prerequisites:** [03-social-login.md](./03-social-login.md), [08-email-verification-tokens.md](../03-tokens/08-email-verification-tokens.md)

---

## The model

```text
User (one)                     OAuthAccount (many)
 ├─ email, role, ...            ├─ google : sub=1101…
 ├─ passwordHash (optional)     └─ github : id=4821…
 └─ passkeys (optional)
```

Identity = `(provider, providerAccountId)`, **not** email. Email is only a hint used when deciding whether two identities belong to the same person.

---

## The core question

A user arrives via Google with `alice@example.com`, and your database already has a local account `alice@example.com`. Same person?

```text
Maybe. Or:
- Someone registered alice@example.com with a password, never verified it
  (the "pre-hijacking" setup).
- The Google account is a Workspace/other-domain account whose email was reassigned.
- A provider returned an unverified email.
```

Linking on "emails match" without more checks is the classic vulnerability.

---

## Pre-hijacking (why verified email on both sides matters)

```text
1. Attacker registers alice@example.com with password "attacker-pw"
   (email never verified, because Alice doesn't know)
2. Later, Alice signs in with Google using alice@example.com
3. A naive app: "email matches → link Google to this account"
4. Attacker logs in with "attacker-pw" and shares Alice's account,
   with her data, and any sessions the attacker opened earlier remain valid
```

Variants exist for the reverse order and for providers that don't verify emails.

---

## Decision table

| Provider email verified? | Local account exists with that email? | Local email verified? | Action |
|---|---|---|---|
| Yes | No | n/a | **Create** user (email verified), create `OAuthAccount` |
| Yes | Yes | **Yes** | Safest choice: require the user to prove ownership of the local account (see below); auto-link only if you accept the risk and the provider is trustworthy |
| Yes | Yes | **No** | **Do not link naively.** Invalidate the unverified local credentials (password, sessions) and take over the account as the verified one, or refuse and ask to verify |
| No / missing | any | any | Never link by email. Create a separate account (or require entering and verifying an email yourself) |
| n/a | n/a | n/a | Identity already linked → just log in |

### Auto-link vs explicit link

| Strategy | UX | Security |
|---|---|---|
| **Explicit linking** (user signs in the existing way, then connects the provider in settings) | Extra step | **Strongest**: ownership proven by an existing credential |
| **Link after proof at the moment of conflict** ("An account with this email exists. Sign in to link Google.") | Moderate | Strong |
| **Auto-link on verified email from a trusted provider** | Smoothest | Acceptable only with a trusted provider and a verified local email; weak if either side is lax |

Recommended default: **explicit or proof-at-conflict linking**, with auto-link reserved for cases where the provider is authoritative for the address (for example Google for `@gmail.com`).

Mind that "email verified by the provider" means *the provider says the user controlled that address when they verified it*, not that they still do. Domain-based accounts (a former employee at `@company.com`) are a known edge case; `sub` stays tied to the old account while email could be reassigned.

---

## Implementation

```js
export async function linkOrCreate(provider, profile) {
  // Providers that don't assert a verified email can't be used to find or claim an account
  if (!profile.email || !profile.emailVerified) {
    throw new AuthError("A verified email is required to sign in with this provider");
  }

  const email = profile.email.toLowerCase();
  const local = await User.findOne({ email }).select("+passwordHash");

  if (!local) {
    const user = await User.create({ email, isEmailVerified: true, name: profile.name });
    await OAuthAccount.create({ userId: user._id, provider, providerAccountId: profile.providerAccountId, email, emailVerified: true });
    return user;
  }

  if (!local.isEmailVerified) {
    // Pre-hijack defense: the existing account was never proven to belong to anyone.
    // Neutralize everything the registrant set up, then adopt it as verified.
    await User.updateOne({ _id: local._id }, { $unset: { passwordHash: 1 }, isEmailVerified: true });
    await revokeAllSessions(local._id.toString());
    await redis.del(`email-verification:user:${local._id}`);   // pending verification links
  } else {
    // Verified local account: require proof of ownership before linking
    throw new LinkRequiredError({ provider, userId: local._id });
  }

  await OAuthAccount.create({ userId: local._id, provider, providerAccountId: profile.providerAccountId, email, emailVerified: true });
  return local;
}
```

Handle `LinkRequiredError` by showing: "You already have an account. Sign in with your password to connect Google." After a successful normal sign-in, finish the link using a **pending link** stored server-side (not in the URL):

```js
// during callback, on LinkRequiredError
await redis.set(`oauth:pending-link:${hashToken(token)}`, JSON.stringify({ userId, provider, profile }), { EX: 600 });
res.cookie("pending_link", token, { httpOnly: true, secure: true, sameSite: "lax", maxAge: 600000 });
res.redirect(`${FRONTEND_URL}/link-account`);

// after the user authenticates normally (password + MFA), consume the pending link
const pending = await redis.getDel(`oauth:pending-link:${hashToken(req.cookies.pending_link)}`);
// verify pending.userId === the authenticated user's id before creating the OAuthAccount
```

---

## Linking while signed in (explicit)

```text
Settings → "Connect GitHub"
   → require step-up (recent auth)             (05-step-up-auth.md)
   → run the normal OAuth flow with a flag in the state record: { linkToUserId }
   → in the callback: ensure the SESSION user == linkToUserId, then create OAuthAccount
```

Important checks:

- The provider identity must not already be linked to a **different** user. The unique index returns `E11000`; handle it with a clear message ("This GitHub account is already connected to another user"), never by silently moving it.
- Bind `linkToUserId` to the browser via the same state record + cookie as the login flow.
- Notify the user by email: "GitHub was connected to your account".

---

## Unlinking

```js
const methods = (user.passwordHash ? 1 : 0) + passkeyCount + linkedAccountCount;
if (methods <= 1) throw new AuthError("Add another sign-in method before removing this one");
```

Rules: require step-up; never remove the **last** way to sign in; after removal, consider revoking sessions that came from that provider; send a notification. A social-only user can add a password through a "set password" flow that requires step-up or goes through the verified-email reset flow ([03-password-change-and-reset.md](../02-passwords/03-password-change-and-reset.md)).

---

## Email changes

| Event | Rule |
|---|---|
| Provider email changes | Don't update the local email automatically; identity is `sub`, email is informational |
| User changes local email | Verify the new address, bind the token to it ([08-email-verification-tokens.md](../03-tokens/08-email-verification-tokens.md)), require step-up |
| Two local users, same provider identity | Impossible by the unique index; if it arises through merging, resolve explicitly |

Account **merging** (two existing local accounts discovered to be one person) is a rare, support-driven, audited operation with proof from both accounts; don't automate it.

---

## Common mistakes

| Mistake | Consequence |
|---|---|
| Linking on email alone | Account takeover |
| Trusting an unverified provider email | Same |
| Leaving an unverified local account's password/sessions alive after taking it over | Pre-hijacker keeps access |
| Linking in the callback without checking who is signed in | Attacker links their provider identity to the victim (or vice versa) via CSRF |
| Allowing unlinking the last method | Locked-out user |
| Automatically changing the email from the provider | Recovery-channel hijack |
| Putting linking state in URLs or client storage | Tamperable |
| No notification on link/unlink | User can't detect abuse |

## Debugging

| Symptom | Cause |
|---|---|
| `E11000` on `OAuthAccount` | Identity already linked (possibly to someone else) |
| Duplicate accounts for one person | Different emails across providers; offer explicit linking from settings |
| "Link required" loop | Pending-link cookie not sent (`SameSite`, path), or it expired |
| Social user can't set a password | Schema requires `passwordHash`, or no set-password flow exists |

---

## Quick summary

- Identity is `(provider, sub)`; email is a hint, not a key.
- Never link on an unverified email, on either side.
- Prefer explicit linking, or proof-at-conflict; auto-link only with trusted providers and verified local emails.
- Neutralize unverified local accounts to defeat pre-hijacking.
- Protect link/unlink with step-up, notifications, and "keep at least one method".

**Next:** [05-oauth-security.md](./05-oauth-security.md)
