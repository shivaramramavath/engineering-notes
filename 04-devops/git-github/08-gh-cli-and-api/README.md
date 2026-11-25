# Module 08: GitHub CLI & Developer APIs

## Overview
This module explores programmatic interaction, developer automation, and platform integration on GitHub. It covers developer productivity via the official GitHub CLI (`gh`), data extraction and scripting using GitHub REST (v3) and GraphQL (v4) APIs, event-driven architectures using Webhooks and first-class GitHub Apps, and the strategic integration of `gh` directly within GitHub Actions CI/CD workflows to eliminate third-party action supply chain dependencies.

---

## Learning Objectives
By completing this module, you will:
- Master the GitHub CLI (`gh`) for daily operations: PR workflows, issue triage, workflow run inspection, and terminal output formatting (`--json`, `--jq`, `--template`).
- Architect queries against GitHub REST (v3) and GraphQL (v4) APIs, managing rate limits, secondary throttling, and cursor pagination.
- Build event-driven systems using GitHub Webhooks with constant-time HMAC-SHA256 signature verification.
- Author, configure, and authenticate GitHub Apps using asymmetric RSA key pairs and ephemeral 1-hour Installation Access Tokens.
- Replace fragile third-party GitHub Marketplace actions with pre-installed, secure `gh` CLI commands in GitHub Actions workflows.

---

## Curriculum Roadmap

| # | Topic | File | Summary |
|---|---|---|---|
| 01 | GitHub CLI (`gh`) Essentials | [`01-gh-cli-essentials.md`](./01-gh-cli-essentials.md) | Terminal PR reviews, issue management, `gh run watch`, custom aliases, and `--jq` output. |
| 02 | REST & GraphQL APIs | [`02-github-rest-and-graphql-api.md`](./02-github-rest-and-graphql-api.md) | Endpoint design, cursor pagination, query cost scoring, rate limits, and Octokit SDKs. |
| 03 | Webhooks & GitHub Apps | [`03-webhooks-and-apps.md`](./03-webhooks-and-apps.md) | HMAC validation, JWT signing, installation tokens, Smee local testing, and App lifecycle. |
| 04 | Automating with `gh` in Actions | [`04-automating-with-gh-and-actions.md`](./04-automating-with-gh-and-actions.md) | Replacing third-party actions, cross-repo dispatch, auto-merge, releases, and minimal permissions. |

---

## Developer Automation Spectrum
- **Interactive Terminal**: GitHub CLI (`gh`) for developer speed and zero context switching.
- **Backend Services**: GitHub Apps with HMAC webhooks for enterprise bots and automated compliance.
- **CI/CD Pipelines**: `gh` running inside GitHub Actions workflows for secure, supply-chain-safe automation.
