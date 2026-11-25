---
title: "Signed Commits, Cryptographic Verification & Vigilant Mode"
category: "04-devops"
subcategory: "git-github / 07-github-security"
tags: ["git", "github", "gpg", "ssh-signing", "vigilant-mode", "commit-signing", "security", "identity"]
---

# Signed Commits, Cryptographic Verification & Vigilant Mode

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Usage](#4-syntax--usage)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices](#6-best-practices)
- [7. Common Mistakes](#7-common-mistakes)
- [8. Performance, Security & Cost Considerations](#8-performance-security--cost-considerations)
- [9. Comparison Table](#9-comparison-table)
- [10. Real-World Interview Questions](#10-real-world-interview-questions)
- [11. Hands-On Exercises](#11-hands-on-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading](#13-further-reading)

---

## 1. What & Why

In Git's fundamental architecture, the author name and email attached to a commit are purely cosmetic metadata strings specified in `.gitconfig`. Anyone with access to a terminal can configure:
```bash
git config user.name "Linus Torvalds"
git config user.email "torvalds@linux-foundation.org"
```
When pushed to a remote repository, GitHub will link the commit to Linus Torvalds' profile picture and username, giving the illusion of authenticity. This impersonation vector—known as **commit spoofing**—allows malicious actors or insider threats to introduce backdoors disguised as trusted maintainers.

**Cryptographic Commit Signing** guarantees non-repudiation and tamper resistance. By signing commits using GPG, SSH keys, or S/MIME X.509 certificates, developers append an asymmetric cryptographic signature to the commit object. GitHub validates this signature against public keys registered to the author's account, displaying a green **Verified** badge.

Furthermore, GitHub's **Vigilant Mode** enforces zero-trust identity verification: any unsigned commit or commit signed with an unknown key authored under the user's email address is explicitly branded with a prominent yellow **Unverified** warning badge, alerting reviewers that the author identity cannot be authenticated.

---

## 2. Core Concepts

### GPG vs SSH Signing
- **GPG (GNU Privacy Guard)**: The historical standard for Git signing. Highly flexible with subkey management and web-of-trust, but notorious for complex setup, external daemons (`gpg-agent`), and key expiration maintenance.
- **SSH Key Signing (Git 2.34+)**: Allows developers to sign commits using their existing SSH authentication keys (e.g., `ed25519`). Requires zero additional software, drastically simplifying team onboarding while maintaining identical cryptographic security.
- **S/MIME (X.509)**: Uses certificates issued by corporate Certificate Authorities (CAs) tied to enterprise identity infrastructure.

### Commit Object Signatures (`gpgsig`)
A signed commit embeds the signature text directly inside the commit object header under the `gpgsig` field. When Git or GitHub verifies a commit, it reconstructs the commit payload (tree SHA, parent SHA, author, committer, and commit message), hashes it, and verifies the cryptographic signature against the public key.

### Verification States on GitHub
- **Verified**: The commit was signed with a valid key associated with the author's verified GitHub email.
- **Unverified**: The signature is missing, expired, revoked, or the email in the commit does not match the GPG/SSH key email identity.
- **Partially Verified**: The commit was signed by a committer on behalf of an unverified co-author.

### Vigilant Mode
By default, GitHub only flags signed commits with "Verified" and leaves unsigned commits unbadged. Enabling **Vigilant Mode** reverses this assumption: all unsigned commits authored under any of the user's registered email addresses are stamped with an **Unverified** badge, immediately exposing spoofed commits.

---

## 3. How It Works Internally

```
Developer Workstation                               GitHub Remote Verification
+-----------------------------------+             +-----------------------------------+
| 1. Create Commit Payload:         |             | 4. Parse Commit Object Header:    |
|    - tree 8a3f2b...               |             |    - Extract author email         |
|    - parent 4c9d1a...             |             |    - Extract gpgsig block         |
|    - author Alice <alice@corp.com>|             +-----------------------------------+
|    - message: "fix: auth flaw"    |                               |
+-----------------------------------+                               v
                  |                               +-----------------------------------+
                  v                               | 5. Query GitHub Public Key Store: |
+-----------------------------------+             |    - Look up public keys          |
| 2. Sign Payload with Private Key  |             |      associated with alice@corp.com|
|    (ed25519 / RSA)                |             +-----------------------------------+
+-----------------------------------+                               |
                  |                                                 v
                  v                               +-----------------------------------+
| 3. Append gpgsig Header to Commit |             | 6. Verify Signature Hash against  |
|    Object & Compute Object SHA    |             |    Commit Payload                 |
+-----------------------------------+             +-----------------------------------+
                  |                                                 |
                  | git push                                        v
                  +==============================>+-----------------------------------+
                                                  | Result:                           |
                                                  | - MATCH -> Green "Verified" Badge |
                                                  | - FAIL  -> Yellow "Unverified"    |
                                                  +-----------------------------------+
```

---

## 4. Syntax & Usage

### Method A: Modern SSH Key Signing (Recommended)
Git 2.34+ natively supports signing with SSH keys:

```bash
# 1. Generate dedicated Ed25519 signing key
ssh-keygen -t ed25519 -C "shiva@engineering.corp" -f ~/.ssh/id_ed25519_signing

# 2. Configure Git to use SSH for signing
git config --global gpg.format ssh
git config --global user.signingkey ~/.ssh/id_ed25519_signing.pub
git config --global commit.gpgsign true
git config --global tag.gpgsign true

# 3. Configure allowed signers for local verification
git config --global gpg.ssh.allowedSignersFile ~/.ssh/allowed_signers
echo "shiva@engineering.corp $(cat ~/.ssh/id_ed25519_signing.pub)" >> ~/.ssh/allowed_signers
```

### Method B: GPG Key Signing
```bash
# 1. List GPG secret keys
gpg --list-secret-keys --keyid-format=long

# 2. Configure Git with GPG key ID
git config --global user.signingkey 3AA5C34371567BD2
git config --global commit.gpgsign true
git config --global gpg.program gpg
```

---

## 5. Practical Examples

### Example 1: Creating and Verifying a Signed Commit Locally
```bash
# Make a commit (auto-signed due to commit.gpgsign=true)
git commit -m "feat(billing): implement invoice pdf export"

# Verify signature on the latest commit
git verify-commit HEAD

# Inspect raw commit object showing the gpgsig block
git cat-file -p HEAD
```
Expected output of `git cat-file -p HEAD`:
```text
tree 6b0a1d4b68e4...
parent 12f9e4a83...
author Shiva <shiva@engineering.corp> 1700000000 +0000
committer Shiva <shiva@engineering.corp> 1700000000 +0000
gpgsig -----BEGIN SSH SIGNATURE-----
 U1NIU0lHAAAAAQAAADMAAAALc3NoLWVkMjU1MTkAAAAg...
 -----END SSH SIGNATURE-----

feat(billing): implement invoice pdf export
```

### Example 2: Enabling Vigilant Mode via GitHub Web UI
1. Navigate to **GitHub Settings** -> **SSH and GPG keys**.
2. Scroll to the **Vigilant mode** section.
3. Check the box **"Flag unsigned commits as unverified"**.
4. Any future unsigned commit using your verified email address will display the yellow "Unverified" pill in GitHub PRs and commit lists.

### Example 3: Enforcing Signed Commits via Enterprise Rulesets
Enforce signature compliance across all repository branches:
```bash
# Define Ruleset JSON payload requiring signed commits
cat << 'EOF' > require-signed-commits.json
{
  "name": "Enforce Signed Commits",
  "target": "branch",
  "enforcement": "active",
  "conditions": {
    "ref_name": {
      "include": ["refs/heads/*"],
      "exclude": []
    }
  },
  "rules": [
    {
      "type": "required_signatures"
    }
  ]
}
EOF

# Create Ruleset via GitHub CLI
gh api --method POST -H "Accept: application/vnd.github+json" \
  /repos/my-org/core-platform/rulesets \
  --input require-signed-commits.json
```

### Example 4: Signing with 1Password CLI / Biometrics
Modern developers can sign Git commits with biometric Touch ID / Windows Hello via 1Password:
```bash
# Configure 1Password as the SSH signing agent
git config --global gpg.format ssh
git config --global user.signingkey "ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAI..."
git config --global gpg.ssh.program "C:\Users\<user>\AppData\Local\1Password\app\8\op-ssh-sign.exe" # Windows
# On macOS: "/Applications/1Password.app/Contents/MacOS/op-ssh-sign"
git config --global commit.gpgsign true
```

### Example 5: Signing Tags for Immutable Release Artifacts
```bash
# Create an annotated, cryptographically signed release tag
git tag -s v2.4.0 -m "Release v2.4.0 - verified binary release"

# Verify tag signature locally
git tag -v v2.4.0
```

---

## 6. Best Practices

- **Adopt SSH Signing Over GPG**: Unless required by legacy PKI infrastructure, standardize team workstations on SSH signing (`gpg.format ssh`). It eliminates GPG agent bugs and reuses existing developer SSH knowledge.
- **Enable Vigilant Mode Individually**: Encourage all staff to enable Vigilant Mode in their GitHub personal settings to prevent impersonation attacks.
- **Enforce Rulesets on Protected Branches**: Require signed commits in repository rulesets to reject unsigned commits at the remote push boundary.
- **Store Signing Keys in Hardware Security Modules (HSMs)**: Use YubiKeys or 1Password biometrics so private signing keys can never be extracted or stolen by workstation malware.
- **Sign Release Tags in CI**: Generate and sign deployment release tags using GitHub Actions with dedicated, ephemeral signing identities (e.g., Sigstore or bot keys).

---

## 7. Common Mistakes

| Mistake | Impact | Correct Approach |
|:---|:---|:---|
| Uploading an SSH key as Authentication only | GitHub rejects commit signature as unverified | Upload key specifically under "Signing Key" type in GitHub settings |
| Email in `.gitconfig` doesn't match public key identity | Commit displays yellow "Unverified" badge | Align `git config user.email` with the verified email linked to the key |
| Forgetting to configure `gpg.format ssh` | Git invokes `gpg` binary for SSH keys, throwing format errors | Set `git config --global gpg.format ssh` |
| Rebasing signed commits without auto-signing | Rebase rewrites commit objects, stripping previous signatures | Ensure `commit.gpgsign=true` is set globally so rebased commits are re-signed |
| Checking in private signing keys to dotfiles | Private keys are compromised across the organization | Never commit `id_ed25519_signing` or export private GPG armors to git |
| Believing unsigned commits are blocked by default | GitHub permits spoofed unsigned commits unless Rulesets block them | Explicitly enable the `required_signatures` rule in repository rulesets |

---

## 8. Performance, Security & Cost Considerations

- **Performance**: Signing adds negligible local CPU overhead (<15ms per commit using Ed25519). Remote verification in GitHub is cached based on commit object SHA.
- **Security**: Commit signing proves *origin* and *integrity* (who authored the commit and that it was not modified). It does *not* prove that the code is free of bugs or malware; combine with CodeQL and CI review gates.
- **Cost**: Commit signing and Vigilant Mode are completely free features available to all GitHub users and organizations.

---

## 9. Comparison Table

| Attribute | SSH Key Signing | GPG Signing | S/MIME Signing |
|:---|:---|:---|:---|
| **Ease of Setup** | Extremely simple (uses `ssh-keygen`) | Complex (`gpg`, agents, pinentry) | Enterprise setup (CA certs) |
| **Algorithm** | Ed25519 (Fast, short) | RSA, ECC | RSA / ECC |
| **Hardware Token** | Native YubiKey support (`sk-ssh`) | YubiKey OpenPGP applet | PIV / SmartCard applet |
| **Key Expiration**| Handled via allowed_signers | Built-in expiration dates | Built-in X.509 validity dates |
| **Git Version** | Git 2.34+ required | Any Git version | Any Git version |

---

## 10. Real-World Interview Questions

<details>
<summary>1. How does commit spoofing work in Git, and why does Git allow it by default?</summary>
Git was designed as a distributed version control system for the Linux kernel community, relying on email patch exchanges. Git treats `user.name` and `user.email` as purely informative headers provided by the client. It performs no cryptographic authentication on these strings. Anyone can set any name and email in their configuration. Commit signing solves this by binding the commit payload to an asymmetric cryptographic key pair.
</details>

<details>
<summary>2. What happens to commit signatures during an interactive rebase (`git rebase -i`)?</summary>
An interactive rebase creates entirely new commit objects with new timestamps, parents, and tree hashes. Because the commit payload changes, the original cryptographic signature becomes invalid. If `commit.gpgsign=true` is enabled, Git will automatically generate new signatures using the rebaser's private key. If not enabled, the rebased commits become unsigned.
</details>

<details>
<summary>3. Why would a commit with a valid GPG signature display an 'Unverified' badge on GitHub?</summary>
Common reasons include:
- The email in the commit (`user.email`) does not match any verified email on the GitHub account owning the GPG key.
- The GPG public key has expired or was revoked.
- The GPG key has not been uploaded to the GitHub account.
- The subkey used for signing was not properly exported and registered.
</details>

<details>
<summary>4. What is GitHub Vigilant Mode and what problem does it solve?</summary>
By default, GitHub only flags signed commits with a green "Verified" badge and leaves unsigned commits unbadged. An attacker could spoof an engineer's email without a signature, and reviewers might not notice the absence of the verified badge. Vigilant Mode marks all unsigned commits authored under that engineer's email with a yellow "Unverified" warning badge, making impersonation attempts immediately obvious.
</details>

<details>
<summary>5. How does SSH key signing differ fundamentally from SSH authentication in GitHub?</summary>
SSH authentication uses an authentication key to establish an encrypted SSH transport session to authenticate your push/pull commands (`git@github.com`). SSH signing uses an SSH key as an asymmetric signing tool to generate a digital signature embedded directly into the Git commit object (`gpgsig`). On GitHub, an SSH key must be explicitly designated as a "Signing Key" to verify commits.
</details>

<details>
<summary>6. How do you configure a repository to strictly block any commit that is not cryptographically signed?</summary>
In GitHub Enterprise or Team repositories, navigate to **Settings** -> **Rules** -> **Rulesets**. Create a new branch ruleset targeting default branches, and enable the rule **Require signed commits**. Any developer attempting to push an unsigned commit will receive a rejection directly in their Git terminal.
</details>

<details>
<summary>7. What is the `allowed_signers` file in SSH commit signing?</summary>
The `allowed_signers` file is Git's local trust store for SSH keys. It maps email addresses to trusted public SSH keys. Git uses this file when running `git verify-commit` or `git log --show-signature` locally to verify that the signature was generated by the trusted owner of that email address.
</details>

<details>
<summary>8. Can GitHub Actions sign commits created during automated CI pipelines?</summary>
Yes. GitHub Actions can sign commits using an organization bot GPG/SSH key stored in Action secrets. Alternatively, modern pipelines can leverage **Sigstore / Fulcio / Rekor** keyless signing using OpenID Connect (OIDC) tokens issued by GitHub Actions to produce cryptographically verifiable artifact and commit attestations.
</details>

---

## 11. Hands-On Exercises

<details>
<summary>Exercise 1: Generate an SSH signing key and configure Git</summary>

**Goal:** Configure local Git to sign commits with Ed25519 SSH keys.

```bash
ssh-keygen -t ed25519 -C "dev@example.com" -f ~/.ssh/id_signing
git config --global gpg.format ssh
git config --global user.signingkey ~/.ssh/id_signing.pub
git config --global commit.gpgsign true
```
</details>

<details>
<summary>Exercise 2: Create and verify a signed commit</summary>

**Goal:** Make a commit and verify its cryptographic signature.

```bash
git commit --allow-empty -m "test: verify signed commit"
git log --show-signature -1
```
</details>

<details>
<summary>Exercise 3: Inspect the raw commit object</summary>

**Goal:** View the `gpgsig` header using `git cat-file`.

```bash
git cat-file -p HEAD
```
</details>

<details>
<summary>Exercise 4: Set up local allowed_signers for verification</summary>

**Goal:** Enable local verification of SSH-signed commits.

```bash
git config --global gpg.ssh.allowedSignersFile ~/.ssh/allowed_signers
echo "$(git config user.email) $(cat ~/.ssh/id_signing.pub)" >> ~/.ssh/allowed_signers
git verify-commit HEAD
```
</details>

<details>
<summary>Exercise 5: Create a signed release tag</summary>

**Goal:** Tag a release with cryptographic signature.

```bash
git tag -s v1.0.0 -m "Release v1.0.0"
git tag -v v1.0.0
```
</details>

<details>
<summary>Exercise 6: Query repository ruleset signatures requirement</summary>

**Goal:** Check via GitHub CLI if signed commits are mandated on the repository.

```bash
gh api /repos/:owner/:repo/rulesets \
  --jq '.[] | select(.rules[]?.type == "required_signatures") | .name'
```
</details>

---

## 12. Summary / Cheat Sheet

```text
================================================================================
                    SIGNED COMMITS & VIGILANT MODE
================================================================================
Impersonation Flaw : user.name & user.email are unauthenticated text strings
SSH Signing Setup  : git config --global gpg.format ssh
                     git config --global user.signingkey ~/.ssh/id_ed25519.pub
                     git config --global commit.gpgsign true
Verification Badges: Green "Verified" | Yellow "Unverified"
Vigilant Mode      : Explicitly brands ALL unsigned commits as "Unverified"
Server Enforcement : GitHub Rulesets -> Rule: "Require signed commits"
Inspection Command : git cat-file -p HEAD (observe gpgsig block)
Verification Cmd   : git log --show-signature -1  OR  git verify-commit HEAD
================================================================================
```

---

## 13. Further Reading
- [GitHub Docs: About commit signature verification](https://docs.github.com/en/authentication/managing-commit-signature-verification/about-commit-signature-verification)
- [GitHub Docs: Signing commits with SSH keys](https://docs.github.com/en/authentication/managing-commit-signature-verification/telling-git-about-your-signing-key#telling-git-about-your-ssh-key)
- [GitHub Docs: Displaying verification statuses with vigilant mode](https://docs.github.com/en/authentication/managing-commit-signature-verification/displaying-verification-statuses-for-all-of-your-commits)
- [Sigstore: Keyless signing and verification](https://www.sigstore.dev/)

---

[<- Previous: CodeQL & Static Analysis](./03-code-scanning-codeql.md) | [Module 07 Overview](./README.md) | [Next: Security Policies & Advisories ->](./05-security-policies-and-advisories.md)
