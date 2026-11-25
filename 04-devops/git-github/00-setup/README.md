# Module 00: Setup & Configuration

[Back to Git & GitHub Track Overview](../README.md)

---

## Overview

Welcome to **Module 00: Setup & Configuration** of the Git & GitHub engineering curriculum. Establishing a rock-solid, secure, and standardized local Git environment is the essential prerequisite for all collaborative software engineering.

This module walks through everything required to prepare a professional workstation: from understanding Git's distributed architecture and installing modern Git across all platforms, to configuring global developer identities, generating Ed25519 SSH keys, configuring cryptographic commit signing, crafting production-grade `.gitconfig` profiles, and managing `.gitignore` and `.gitattributes` policies.

---

## Module Roadmap & Notes

| Note | Title | Difficulty | Estimated Time | Key Topics |
| :--- | :--- | :--- | :--- | :--- |
| **[01-getting-started.md](./01-getting-started.md)** | Git Getting Started & Verification | Beginner | 45 min | DVCS vs CVCS, Three Tree States, git init/clone, SHA verification |
| **[02-install-and-configuration.md](./02-install-and-configuration.md)** | Git Installation & Configuration | Beginner | 45 min | Cross-platform install, config scopes, `core.autocrlf`, `credential.helper` |
| **[03-ssh-keys-and-gpg-or-ssh-signing.md](./03-ssh-keys-and-gpg-or-ssh-signing.md)** | SSH Keys & GPG / SSH Commit Signing | Intermediate | 50 min | Ed25519 keys, ssh-agent, `gpg.format=ssh`, commit verification, Vigilant Mode |
| **[04-gitconfig-in-depth.md](./04-gitconfig-in-depth.md)** | Git Configuration In-Depth (.gitconfig) | Advanced | 55 min | `zdiff3`, `histogram`, `rerere`, `includeIf`, `push.autoSetupRemote` |
| **[05-gitignore-and-gitattributes.md](./05-gitignore-and-gitattributes.md)** | .gitignore & .gitattributes Deep Dive | Intermediate | 50 min | Precedence, `git check-ignore`, line normalization, `linguist-generated` |

---

## Learning Objectives

By completing this module, you will be able to:
1. Explain the architectural distinctions between Centralized and Distributed Version Control Systems.
2. Install and configure Git 2.45+ on Linux, macOS, and Windows with consistent line ending handling.
3. Configure multi-scope Git settings using System, Global, Local, and Conditional (`includeIf`) directives.
4. Generate secure Ed25519 SSH keys and configure native SSH commit signing without GPG overhead.
5. Engineer a modern `.gitconfig` that activates `zdiff3` conflict views, `histogram` diffing, and `rerere` conflict memory.
6. Write robust `.gitignore` and `.gitattributes` files to normalize repository line endings and collapse PR diffs.

---

## Verification Checklist

Before advancing to [Module 01: Core Fundamentals](../01-fundamentals/README.md), verify your local environment:

- [ ] `git --version` reports Git 2.40 or higher.
- [ ] `git config --get user.name` and `git config --get user.email` return your verified identity.
- [ ] `ssh -T git@github.com` outputs successful authentication.
- [ ] `git config --get gpg.format` returns `ssh`.
- [ ] `git config --get merge.conflictStyle` returns `zdiff3`.
- [ ] Global `.gitignore` is configured via `core.excludesFile`.

---

[Proceed to Module 01: Core Fundamentals ->](../01-fundamentals/README.md)
