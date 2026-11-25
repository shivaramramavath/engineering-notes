<!--
================================================================================
Metadata:
  Title: SSH Keys & GPG / SSH Commit Signing
  Track: Git & GitHub / DevOps
  Difficulty: Intermediate
  Estimated Time: 50 minutes
  Prerequisites: 02-install-and-configuration.md
  Key Terminology: SSH, Ed25519, ssh-agent, GPG Signing, SSH Commit Signing, gpg.format, Verified Badge, Vigilant Mode
================================================================================
-->

# SSH Keys & GPG / SSH Commit Signing

[<- 02-install-and-configuration.md](./02-install-and-configuration.md) | [Back to Module Overview](./README.md) | [04-gitconfig-in-depth.md ->](./04-gitconfig-in-depth.md)

---

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Usage](#4-syntax--usage)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices](#6-best-practices)
- [7. Common Mistakes & Troubleshooting](#7-common-mistakes--troubleshooting)
- [8. Performance, Security & Cost Implications](#8-performance-security--cost-implications)
- [9. Comparison Table](#9-comparison-table)
- [10. Real-World Interview Questions](#10-real-world-interview-questions)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary & Quick Reference Cheat Sheet](#12-summary--quick-reference-cheat-sheet)
- [13. Further Reading & Official Documentation](#13-further-reading--official-documentation)

---

## 1. What & Why

In Git, authorship metadata is fundamentally unauthenticated by default. Anyone can run:
```bash
git -c user.name="Linus Torvalds" -c user.email="torvalds@linux-foundation.org" commit -m "Malicious backdoor"
```
The resulting commit object will proudly display Linus Torvalds as the author. Git repositories and platforms like GitHub will attribute the commit to that email address without question unless cryptographic verification is enforced.

To establish trust and prove that code originates from an authorized engineer:
1. **SSH Keys** authenticate the *transport layer* (identifying who is pushing to or pulling from the remote repository).
2. **Cryptographic Signatures (SSH or GPG)** authenticate the *data layer* (mathematically proving that a specific commit or tag was authored by the holder of a private key and has not been altered).

Modern Git workflows have evolved: while GPG was historically the only option for commit verification, Git 2.34+ natively supports **SSH commit signing**, allowing developers to use their existing SSH keys for both transport authentication and cryptographic commit verification without the complexity of GPG keyrings.

---

## 2. Core Concepts

### Transport Authentication vs. Commit Signing
- **Transport Authentication (SSH / HTTPS)**: Governs *access control*. It answers: "Does this workstation have write permission to push to `github.com/org/repo`?"
- **Commit Signing**: Governs *provenance and non-repudiation*. It answers: "Did the claimed author actually create this exact commit snapshot?"

```
+-----------------------------------------------------------------------------------+
|                               Security Layers in Git                              |
|                                                                                   |
|  [Developer Workstation]                                  [GitHub Remote Server]  |
|                                                                                   |
|  1. COMMIT LEVEL: Cryptographic Signing                                           |
|     git commit -S -m "Feat"                                                       |
|     Signs commit object with Private Key (SSH or GPG)                             |
|     +-------------------------+                                                   |
|     | Commit Data + Signature |                                                   |
|     +-------------------------+                                                   |
|                  |                                                                |
|                  v                                                                |
|  2. TRANSPORT LEVEL: SSH / HTTPS Push                                             |
|     git push origin main                                                          |
|     Authenticates via SSH Key (Port 22) or HTTPS PAT (Port 443)                   |
|     =========================================================> [GitHub Platform]  |
|                                                                - Checks Push ACL  |
|                                                                - Validates Sig    |
|                                                                - Awards "VERIFIED"|
+-----------------------------------------------------------------------------------+
```

### Key Types: Ed25519 vs. RSA
- **RSA**: Legacy public-key cryptosystem based on integer factorization. Requires long keys (3072 or 4096 bits) for acceptable modern security, resulting in slower key generation and larger signatures.
- **Ed25519**: Modern Edwards-curve Digital Signature Algorithm (EdDSA) using Curve25519. It provides 128-bit security equivalent to ~3000-bit RSA using only a 256-bit key length. It offers superior performance, immune to timing and side-channel attacks, and generates compact signatures. **Ed25519 is the industry standard for Git and SSH today.**

---

## 3. How It Works Internally

### Anatomy of an SSH-Signed Commit Object
When you commit with signing enabled (`git commit -S`), Git serializes the commit object metadata (tree hash, parent hash, author timestamp, committer timestamp, commit message). It then signs this payload using the private key specified by `user.signingkey`.

The resulting ASCII-armored signature is embedded directly into the header of the raw commit object in `.git/objects/`:

```
tree 4b825dc642cb6eb9a060e54bf8d69288fbee4904
parent c8f01b38f8702f37c4f42f36f6d0f666fcf9615e
author Alex Morgan <alex@example.com> 1700000000 +0000
committer Alex Morgan <alex@example.com> 1700000000 +0000
gpgsig -----BEGIN SSH SIGNATURE-----
 U1NIU0lHAAAAAQAAADMAAAALc3NoLWVkMjU1MTkAAAAg...
 ...
 -----END SSH SIGNATURE-----

Implement user authentication controller
```

When GitHub or any engineer runs `git verify-commit <hash>`, Git extracts the signature, reconstructs the commit metadata, and verifies it against the corresponding public key registered in GitHub or an `allowed_signers` file.

---

## 4. Syntax & Usage

### Step 1: Generate an Ed25519 SSH Key

```bash
# Generate key with standard email comment and strong passphrase
ssh-keygen -t ed25519 -C "alex.morgan@company.com" -f ~/.ssh/id_ed25519
```
*Note: Do not leave the passphrase empty for developer workstations.*

### Step 2: Configure ssh-agent

The `ssh-agent` stores decrypted private keys in memory so you only enter your passphrase once per session.

#### On Linux / macOS:
```bash
# Start the agent in the background
eval "$(ssh-agent -s)"

# Add the key to agent (macOS supports storing passphrase in Keychain)
# On macOS:
ssh-add --apple-use-keychain ~/.ssh/id_ed25519

# On Linux:
ssh-add ~/.ssh/id_ed25519
```

#### On Windows (PowerShell - run as Administrator once to enable OpenSSH Agent service):
```powershell
Get-Service ssh-agent | Set-Service -StartupType Automatic
Start-Service ssh-agent
ssh-add $HOME\.ssh\id_ed25519
```

### Step 3: Add SSH Public Key to GitHub
1. Copy the public key to clipboard:
   ```bash
   # macOS:
   pbcopy < ~/.ssh/id_ed25519.pub
   # Linux (xclip):
   xclip -selection clipboard < ~/.ssh/id_ed25519.pub
   # Windows (PowerShell):
   Get-Content $HOME\.ssh\id_ed25519.pub | Set-Clipboard
   ```
2. Navigate to **GitHub Settings -> SSH and GPG keys -> New SSH Key**.
3. Set **Key type** to **Authentication Key**.
4. Test the connection:
   ```bash
   ssh -T git@github.com
   # Expected output:
   # Hi username! You've successfully authenticated, but GitHub does not provide shell access.
   ```

---

### Step 4: Configure Native SSH Commit Signing (Git 2.34+)

Add the same public key as a **Signing Key** on GitHub (Key Type: *Signing Key*), then configure Git locally:

```bash
# 1. Instruct Git to use SSH instead of GPG for signing
git config --global gpg.format ssh

# 2. Point Git to your public key (or path to private key)
git config --global user.signingkey ~/.ssh/id_ed25519.pub

# 3. Automatically sign all commits and tags
git config --global commit.gpgsign true
git config --global tag.gpgsign true
```

---

## 5. Practical Examples

### Example 1: Verifying Commits Locally with `allowed_signers`
To verify SSH signatures locally without communicating with GitHub, create an `allowed_signers` file mapping email addresses to public keys.

```bash
# Create signers file
mkdir -p ~/.config/git
echo "alex.morgan@company.com $(cat ~/.ssh/id_ed25519.pub)" >> ~/.config/git/allowed_signers

# Tell Git where to find allowed signers
git config --global gpg.ssh.allowedSignersFile ~/.config/git/allowed_signers

# Verify a signed commit
git log --show-signature -1
```
Output:
```
commit a1b2c3d4e5f6...
Good "git" signature for alex.morgan@company.com with ED25519 key SHA256:abcd...
Author: Alex Morgan <alex.morgan@company.com>
Date:   Mon Nov 25 10:00:00 2024
```

### Example 2: Managing Multiple SSH Keys via `~/.ssh/config`
Developers managing personal and enterprise GitHub accounts on a single machine use SSH host aliasing:

In `~/.ssh/config`:
```text
# Personal GitHub
Host github.com-personal
    HostName github.com
    User git
    IdentityFile ~/.ssh/id_ed25519_personal
    IdentitiesOnly yes

# Enterprise Work GitHub
Host github.com-work
    HostName github.com
    User git
    IdentityFile ~/.ssh/id_ed25519_work
    IdentitiesOnly yes
```

Cloning with host aliases:
```bash
# Clones using personal key
git clone git@github.com-personal:myuser/personal-repo.git

# Clones using corporate key
git clone git@github.com-work:corporate-org/enterprise-app.git
```

### Example 3: GPG Key Generation and Configuration (Legacy / Compliance)
In regulated environments mandating FIPS or hardware security tokens (YubiKey via GPG):

```bash
# Generate full GPG key (RSA 4096 or ECC)
gpg --full-generate-key
# Select option 1 (RSA and RSA), 4096 bits, 0 (no expiry or 1y)

# List key ID
gpg --list-secret-keys --keyid-format=long

# Export public key for GitHub
gpg --armor --export 3AA5C34371567BD2

# Configure Git for GPG
git config --global gpg.format openpgp
git config --global user.signingkey 3AA5C34371567BD2
git config --global commit.gpgsign true
```

### Example 4: Signing Tags for Releases
Release artifacts must be signed to guarantee source authenticity in distribution pipelines:

```bash
# Create an annotated signed tag
git tag -s v1.4.0 -m "Release version 1.4.0: security fixes"

# Verify tag signature
git tag -v v1.4.0
```

### Example 5: Troubleshooting `error: gpg failed to sign the data`
When GPG fails in terminal environments without a GUI pinentry agent:

```bash
# Ensure GPG has access to the current TTY
export GPG_TTY=$(tty)

# Restart gpg-agent
gpgconf --kill gpg-agent
gpgconf --launch gpg-agent

# Test manual encryption/signing
echo "test" | gpg --clearsign
```

---

## 6. Best Practices

1. **Use Ed25519 Over RSA**: Always generate Ed25519 keys (`ssh-keygen -t ed25519`). Only fallback to 4096-bit RSA if legacy internal servers lack OpenSSH 6.5+ support.
2. **Standardize on SSH Commit Signing**: Unless corporate compliance explicitly mandates GPG/X.509 S/MIME, use `gpg.format = ssh`. It drastically simplifies key management across engineering teams.
3. **Always Set a Passphrase**: Never generate passwordless SSH private keys on developer laptops. Let `ssh-agent` handle credential caching securely in memory.
4. **Enable GitHub "Vigilant Mode"**: Under GitHub Account Settings -> SSH and GPG keys, enable **Flag unsigned commits as unverified**. If someone attempts to forge commits under your email, GitHub displays an "Unverified" warning tag.
5. **Enforce Signed Commits in Branch Protection**: In production repositories, enable branch rulesets: **Require signed commits**. Pushes containing unsigned commits are rejected at the server boundary.
6. **Rotate and Expire Keys**: Schedule regular SSH key rotation (annually or semi-annually), and immediately revoke keys when hardware is retired or lost.

---

## 7. Common Mistakes & Troubleshooting

| Error / Symptom | Root Cause | Solution |
| :--- | :--- | :--- |
| `Permission denied (publickey)` during push/clone | Private key not added to `ssh-agent`, wrong key offered, or public key missing on GitHub. | Run `ssh-add -l` to check loaded keys; test with `ssh -Tv git@github.com` for verbose handshake logs. |
| `error: Load key "~/.ssh/id_ed25519": bad permissions` | SSH private key file permissions are too open. | Run `chmod 600 ~/.ssh/id_ed25519` and `chmod 700 ~/.ssh`. |
| GitHub displays "Unverified" badge on signed commit | Commit author email does not match verified email in GitHub, or public signing key was not registered as a **Signing Key**. | Ensure `git config user.email` matches an email verified in your GitHub account, and upload the key under "Signing Keys". |
| `error: Couldn't load public signing key` | Path in `user.signingkey` is invalid or points to a missing file. | Run `git config user.signingkey ~/.ssh/id_ed25519.pub` and verify the file exists on disk. |
| `gpg: signing failed: Inappropriate ioctl for device` | GPG cannot open interactive passphrase prompt in headless/terminal session. | Add `export GPG_TTY=$(tty)` to your `~/.bashrc` or `~/.zshrc`. |
| Windows OpenSSH agent fails: `Error connecting to agent: No such file or directory` | Windows `ssh-agent` service is disabled. | In Administrator PowerShell, run `Set-Service ssh-agent -StartupType Automatic; Start-Service ssh-agent`. |

---

## 8. Performance, Security & Cost Implications

### Performance
- **Signature Creation Overhead**: Signing a commit with Ed25519 takes approximately **0.1 to 0.5 milliseconds** locally. In automated CI/CD pipelines, signing commits or tags adds virtually unmeasurable overhead.
- **Verification at Scale**: Verifying signatures during `git log --show-signature` requires parsing crypto signatures for each commit. For daily development, omit `--show-signature` unless auditing.

### Security
- **Defense Against Impersonation**: Commit signing prevents compromised credentials on third-party services from creating fraudulent git history in your name.
- **Hardware Keys (FIDO2 / U2F)**: OpenSSH 8.2+ supports hardware security keys directly:
  ```bash
  ssh-keygen -t ed25519-sk -C "alex@company.com"
  ```
  This requires a physical touch on a YubiKey for every SSH connection or commit signature, providing hardware-enforced protection against credential theft.

---

## 9. Comparison Table

| Attribute | SSH Transport Keys | GPG Commit Signing | Modern SSH Commit Signing |
| :--- | :--- | :--- | :--- |
| **Primary Purpose** | Repository access & network auth | Commit & tag provenance | Commit & tag provenance |
| **Supported In Git** | All Git versions | Git 1.7.9+ | Git 2.34+ |
| **Underlying Crypto** | Ed25519, RSA, ECDSA | OpenPGP (RSA, Ed25519) | OpenSSH (Ed25519, RSA) |
| **Configuration Complexity** | Very Low | High (requires GPG suite) | Very Low (reuses SSH key) |
| **GitHub Verification** | Not applicable | "Verified" badge | "Verified" badge |
| **Hardware Token Support** | Native (`-sk` keys) | Via `gpg-agent` / scdaemon | Native (`ed25519-sk`) |

---

## 10. Real-World Interview Questions

<details>
<summary><strong>1. Why is Git commit signing necessary if developers already authenticate via SSH or HTTPS to push code?</strong></summary>

Push authentication (SSH/HTTPS) only validates that the person pushing has write permissions to the repository. It does not validate who authored the commits within the push. Because the `Author` and `Committer` headers in a Git commit object are arbitrary plaintext fields, any developer with push access can commit code attributed to any other person's name and email address. Commit signing cryptographically binds the commit contents and metadata to the author's private key, guaranteeing authenticity and non-repudiation.
</details>

<details>
<summary><strong>2. What is the fundamental difference between GPG commit signing and SSH commit signing in Git 2.34+?</strong></summary>

Historically, Git supported signing only via OpenPGP (GPG) and S/MIME. GPG requires maintaining an independent GPG keyring, configuring `gpg-agent`, managing key servers, and dealing with complex trust webs. Git 2.34 introduced SSH commit signing (`gpg.format = ssh`), allowing Git to sign and verify commits using standard OpenSSH keys and the `ssh-keygen` signing subsystem. This eliminates the need to install or configure GPG while maintaining the same level of cryptographic verification.
</details>

<details>
<summary><strong>3. Why is Ed25519 strongly favored over RSA for modern SSH keys?</strong></summary>

Ed25519 is an elliptic curve signature scheme (Curve25519). Key benefits:
1. **Security & Cryptanalysis**: Equivalent to ~3000-bit RSA security using only 256-bit keys.
2. **Performance**: Significantly faster key generation and signature operations.
3. **Resilience**: Designed specifically to prevent side-channel and timing attacks (constant-time execution).
4. **Signature Size**: Generates compact 64-byte signatures, keeping commit objects lean.
</details>

<details>
<summary><strong>4. What conditions must be satisfied for GitHub to display the green "Verified" badge on a commit?</strong></summary>

GitHub requires three conditions:
1. The commit must be signed using a valid private key whose corresponding public key is registered in the committer's GitHub account as a **Signing Key** (GPG or SSH).
2. The committer email address in the Git commit metadata must match a verified email address associated with that same GitHub account.
3. The cryptographic signature must be mathematically valid and not expired or revoked.
</details>

<details>
<summary><strong>5. Explain how GitHub's "Vigilant Mode" enhances developer security.</strong></summary>

By default, GitHub only labels signed commits with a "Verified" badge, leaving unsigned commits with no badge at all. This allows an attacker to impersonate you with unsigned commits without raising obvious visual suspicion. Enabling "Vigilant Mode" tells GitHub that *all* commits attributed to your email address must be signed. If a commit appears with your email but lacks a valid signature, GitHub explicitly flags it with an amber **Unverified** warning badge.
</details>

<details>
<summary><strong>6. How does a team verify SSH-signed commits in a local environment or CI without contacting GitHub?</strong></summary>

By using an `allowed_signers` file (configured via `git config gpg.ssh.allowedSignersFile <path>`). The file contains entries formatted as:
`<email> <key-type> <public-key>`
When running `git log --show-signature` or `git verify-commit <hash>`, Git consults this file to verify that the public key that signed the commit is authorized for that committer email.
</details>

<details>
<summary><strong>7. How do you configure `~/.ssh/config` to use different SSH keys for different GitHub organizations?</strong></summary>

Define host aliases in `~/.ssh/config`:
```text
Host github-work
    HostName github.com
    User git
    IdentityFile ~/.ssh/id_work
    IdentitiesOnly yes

Host github-personal
    HostName github.com
    User git
    IdentityFile ~/.ssh/id_personal
    IdentitiesOnly yes
```
Then set the repository remote URL using the alias: `git remote set-url origin git@github-work:org/repo.git`.
</details>

<details>
<summary><strong>8. What happens when an engineer pushes to a branch protected by a "Require signed commits" ruleset without signing?</strong></summary>

The remote Git server (GitHub pre-receive hook) validates the commit object headers. If any commit in the push payload lacks a valid, recognized cryptographic signature, the push is rejected immediately with an error message: `remote: error: GH007: Your push would publish a commit that is not signed by a verified signature`.
</details>

---

## 11. Practice Exercises

<details>
<summary><strong>Exercise 1: Generating an Ed25519 Key and Inspecting Public/Private Files</strong></summary>

**Task**: Generate a fresh Ed25519 SSH key in a temporary directory, set a passphrase, inspect the file permissions, and examine the public key comment.

**Solution**:
```bash
mkdir -p /tmp/ssh-lab && cd /tmp/ssh-lab
ssh-keygen -t ed25519 -C "lab-engineer@test.org" -f ./id_test -N "StrongPassphrase123!"

# Inspect permissions (private key must be read/write by owner only):
ls -l ./id_test*
# -rw------- 1 alex alex 411 ./id_test
# -rw-r--r-- 1 alex alex  98 ./id_test.pub

# View public key structure:
cat ./id_test.pub
```
</details>

<details>
<summary><strong>Exercise 2: Setting up `ssh-agent` and Adding a Key</strong></summary>

**Task**: Launch the SSH agent daemon in your terminal session, verify that no keys are currently loaded, add your test key, and verify that it is listed.

**Solution**:
```bash
eval "$(ssh-agent -s)"
ssh-add -l
# Output: The agent has no identities.

ssh-add ./id_test
# Enter passphrase

ssh-add -l
# Displays 256 SHA256:... lab-engineer@test.org (ED25519)
```
</details>

<details>
<summary><strong>Exercise 3: Configuring Local SSH Commit Signing</strong></summary>

**Task**: Configure a local repository to sign commits using your generated SSH key, create a signed commit, and inspect the raw commit object.

**Solution**:
```bash
mkdir /tmp/repo-sign && cd /tmp/repo-sign
git init
git config user.name "Lab Engineer"
git config user.email "lab-engineer@test.org"
git config gpg.format ssh
git config user.signingkey /tmp/ssh-lab/id_test.pub
git config commit.gpgsign true

echo "Hello Security" > file.txt
git add file.txt
git commit -m "First signed commit"

# Inspect raw commit object including gpgsig header:
git cat-file -p HEAD
```
</details>

<details>
<summary><strong>Exercise 4: Verifying the Signed Commit with `allowed_signers`</strong></summary>

**Task**: Configure `gpg.ssh.allowedSignersFile` for the repository and execute `git log --show-signature` to verify that the signature is marked as good.

**Solution**:
```bash
echo "lab-engineer@test.org $(cat /tmp/ssh-lab/id_test.pub)" > /tmp/repo-sign/allowed_signers
git config gpg.ssh.allowedSignersFile /tmp/repo-sign/allowed_signers

git log --show-signature -1
# Output contains: Good "git" signature for lab-engineer@test.org with ED25519 key...
```
</details>

<details>
<summary><strong>Exercise 5: Testing Signature Rejection on Tampering</strong></summary>

**Task**: Understand how commit verification detects tampering by simulating an untrusted signer. Change the allowed signers email and observe the verification failure.

**Solution**:
```bash
# Alter the allowed signers entry to a different email
echo "imposter@evil.org $(cat /tmp/ssh-lab/id_test.pub)" > /tmp/repo-sign/allowed_signers

git log --show-signature -1
# Output reports untrusted or unrecognized signer for the commit author email.
```
</details>

<details>
<summary><strong>Exercise 6: Creating and Verifying a Cryptographically Signed Tag</strong></summary>

**Task**: Create a signed release tag `v1.0.0` on your signed commit, and verify it using `git tag -v`.

**Solution**:
```bash
git tag -s v1.0.0 -m "Production Release v1.0.0"

# Verify tag signature
git tag -v v1.0.0
# Returns signature details and commit target
```
</details>

---

## 12. Summary & Quick Reference Cheat Sheet

| Task | Command |
| :--- | :--- |
| Generate Ed25519 Key | `ssh-keygen -t ed25519 -C "email@domain.com"` |
| Start SSH Agent | `eval "$(ssh-agent -s)"` |
| Add Key to Agent | `ssh-add ~/.ssh/id_ed25519` |
| List Agent Keys | `ssh-add -l` |
| Test GitHub SSH Conn | `ssh -T git@github.com` |
| Configure SSH Signing | `git config --global gpg.format ssh` |
| Set Signing Key | `git config --global user.signingkey ~/.ssh/id_ed25519.pub` |
| Auto-Sign All Commits | `git config --global commit.gpgsign true` |
| Auto-Sign All Tags | `git config --global tag.gpgsign true` |
| Inspect Commit Object | `git cat-file -p HEAD` |
| Verify Log Signatures | `git log --show-signature -n 5` |
| Verify Signed Tag | `git tag -v <tagname>` |

---

## 13. Further Reading & Official Documentation

- [GitHub Docs - Generating a new SSH key and adding it to the ssh-agent](https://docs.github.com/en/authentication/connecting-to-github-with-ssh/generating-a-new-ssh-key-and-adding-it-to-the-ssh-agent)
- [GitHub Docs - Telling Git about your signing key](https://docs.github.com/en/authentication/managing-commit-signature-verification/telling-git-about-your-signing-key)
- [GitHub Docs - About commit signature verification and Vigilant Mode](https://docs.github.com/en/authentication/managing-commit-signature-verification/about-commit-signature-verification)
- [Git Documentation - git-tag signing options](https://git-scm.com/docs/git-tag#_options)

---

[<- 02-install-and-configuration.md](./02-install-and-configuration.md) | [Back to Module Overview](./README.md) | [04-gitconfig-in-depth.md ->](./04-gitconfig-in-depth.md)
