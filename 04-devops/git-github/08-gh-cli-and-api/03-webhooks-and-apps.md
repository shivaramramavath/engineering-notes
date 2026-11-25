---
title: "GitHub Webhooks & GitHub Apps Architecture"
category: "04-devops"
subcategory: "git-github / 08-gh-cli-and-api"
tags: ["git", "github", "webhooks", "github-apps", "oauth", "security", "hmac", "automation", "devops"]
---

# GitHub Webhooks & GitHub Apps Architecture

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

Polling the GitHub API continuously to check if a commit was pushed, a PR was approved, or a release was published is wildly inefficient and rapidly exhausts API rate limits. Modern enterprise DevOps pipelines require **event-driven architectures** that react immediately when activities occur.

GitHub provides two primary building blocks for event-driven integrations:
1. **GitHub Webhooks**: HTTP POST callbacks dispatched to an external server whenever events happen in a repository or organization (pushes, issues, workflow runs).
2. **GitHub Apps**: The official, first-class integration framework for building programmatic tools, bots, and CI/CD systems on GitHub. Unlike legacy OAuth apps or personal access tokens, GitHub Apps act as independent actors with granular permissions, receive their own dedicated webhooks, and authenticate using cryptographically signed JSON Web Tokens (JWT) exchanging for short-lived installation access tokens.

By combining Webhooks and GitHub Apps, organizations build secure, auditable, serverless automations that scale cleanly without managing personal user passwords or long-lived static tokens.

---

## 2. Core Concepts

### Webhook Payloads & HMAC Verification
When an event fires, GitHub sends an HTTP POST request containing:
- `X-GitHub-Event`: The name of the event (e.g., `pull_request`, `push`, `workflow_job`).
- `X-GitHub-Delivery`: A unique UUID identifying the webhook delivery attempt.
- `X-Hub-Signature-256`: An HMAC-SHA256 signature calculated over the request body using a shared secret. Receivers **must** verify this signature to prevent request forgery from malicious third parties.

### At-Least-Once Delivery Guarantees
GitHub guarantees *at-least-once* delivery. Webhook receivers must be **idempotent**: network retries or transient server failures may cause GitHub to deliver the same `X-GitHub-Delivery` event more than once.

### GitHub Apps Architecture
A GitHub App consists of:
- **App Settings**: Defines webhook endpoints, requested permissions, and event subscriptions.
- **Private Key**: An asymmetric RSA 2048-bit key used by the backend service to sign JWTs.
- **Installations**: The App can be installed on specific organizations or repositories. Each installation generates its own scoped **Installation Access Token** valid for 1 hour.

### GitHub Apps vs OAuth Apps
- **OAuth Apps**: Act on behalf of an authenticated *user*. If the user leaves the organization or has permissions removed, the integration breaks. Scopes are broad and account-wide.
- **GitHub Apps**: Act as an independent *entity* (e.g., `my-bot[bot]`). Permissions are granular, scoped strictly to specific repositories, and completely unaffected by changes in individual employee employment status.

---

## 3. How It Works Internally

```
GitHub App Authentication & Webhook Interaction Loop
+-------------------------------------------------------------------------+
| GitHub Event Occurs (e.g., PR Opened in my-org/payment-service)         |
+-------------------------------------------------------------------------+
                                    |
                                    v
+-------------------------------------------------------------------------+
| GitHub Webhook Dispatcher                                               |
| 1. Generates JSON Payload                                               |
| 2. Computes HMAC: sha256(payload, webhook_secret)                       |
| 3. Sends HTTP POST to App URL with X-Hub-Signature-256                   |
+-------------------------------------------------------------------------+
                                    |
                                    v
+-------------------------------------------------------------------------+
| Your Webhook Receiver (Node.js, Go, Python Serverless Function)        |
| 1. Verifies HMAC-SHA256 signature (Rejects 401 if invalid)              |
| 2. Extracts installation_id: 12345678                                  |
| 3. Signs a local JWT using App's Private RSA Key (valid 10 mins)        |
| 4. Calls GitHub API: POST /app/installations/12345678/access_tokens     |
+-------------------------------------------------------------------------+
                                    |
                                    v
+-------------------------------------------------------------------------+
| GitHub Token Service                                                    |
| - Validates JWT signature against App Public Key                        |
| - Mints ephemeral Installation Access Token (Valid 1 hour)              |
+-------------------------------------------------------------------------+
                                    |
                                    v
+-------------------------------------------------------------------------+
| Your Webhook Receiver (Actions Executed)                                |
| - Calls GitHub API using Installation Token (e.g., adds comment / label)|
| - Submits HTTP 200 OK to GitHub Webhook Service                         |
+-------------------------------------------------------------------------+
```

---

## 4. Syntax & Usage

### Verifying Webhook Signatures (Node.js Example)
```javascript
const crypto = require('crypto');

function verifyGitHubSignature(req, webhookSecret) {
  const signature = req.headers['x-hub-signature-256'];
  if (!signature) return false;

  const hmac = crypto.createHmac('sha256', webhookSecret);
  const digest = 'sha256=' + hmac.update(req.rawBody).digest('hex');

  // Use timingSafeEqual to prevent timing attacks
  return crypto.timingSafeEqual(Buffer.from(signature), Buffer.from(digest));
}
```

### Generating an App JWT in Bash
```bash
# Generate a JWT valid for 10 minutes using openssl
APP_ID="12345"
PRIVATE_KEY_PATH="private-key.pem"

HEADER=$(echo -n '{"alg":"RS256","typ":"JWT"}' | openssl base64 -e -A | tr '+/' '-_' | tr -d '=')
NOW=$(date +%s)
IAT=$((NOW - 60))
EXP=$((NOW + 600))
PAYLOAD=$(echo -n "{\"iat\":$IAT,\"exp\":$EXP,\"iss\":\"$APP_ID\"}" | openssl base64 -e -A | tr '+/' '-_' | tr -d '=')

SIGNATURE=$(echo -n "$HEADER.$PAYLOAD" | openssl dgst -sha256 -sign "$PRIVATE_KEY_PATH" | openssl base64 -e -A | tr '+/' '-_' | tr -d '=')
JWT="$HEADER.$PAYLOAD.$SIGNATURE"
```

---

## 5. Practical Examples

### Example 1: Exchanging JWT for an Installation Access Token
```bash
# 1. Fetch installations of the GitHub App
INSTALLATION_ID=$(curl -s -H "Authorization: Bearer $JWT" \
  -H "Accept: application/vnd.github+json" \
  https://api.github.com/app/installations | jq '.[0].id')

# 2. Mint 1-hour Installation Token
INSTALLATION_TOKEN=$(curl -s -X POST \
  -H "Authorization: Bearer $JWT" \
  -H "Accept: application/vnd.github+json" \
  https://api.github.com/app/installations/$INSTALLATION_ID/access_tokens | jq -r '.token')

# 3. Perform authenticated operation as the GitHub App
curl -s -H "Authorization: token $INSTALLATION_TOKEN" \
  https://api.github.com/installation/repositories | jq '.repositories[].name'
```

### Example 2: Complete Express.js Webhook Handler
```javascript
const express = require('express');
const crypto = require('crypto');

const app = express();
app.use(express.json({
  verify: (req, res, buf) => { req.rawBody = buf; }
}));

const WEBHOOK_SECRET = process.env.GITHUB_WEBHOOK_SECRET;

app.post('/webhook', (req, res) => {
  const signature = req.headers['x-hub-signature-256'];
  const hmac = crypto.createHmac('sha256', WEBHOOK_SECRET);
  const expectedSignature = 'sha256=' + hmac.update(req.rawBody).digest('hex');

  if (!crypto.timingSafeEqual(Buffer.from(signature || ''), Buffer.from(expectedSignature))) {
    return res.status(401).send('Signatures do not match');
  }

  const event = req.headers['x-github-event'];
  if (event === 'pull_request' && req.body.action === 'opened') {
    console.log(`New PR #${req.body.pull_request.number} opened in ${req.body.repository.full_name}`);
    // Trigger automated review routing or Slack alert
  }

  res.status(200).send('OK');
});

app.listen(3000, () => console.log('Webhook server listening on port 3000'));
```

### Example 3: Forwarding Local Webhooks with `smee-client`
Test webhooks locally during development without exposing public ports:
```bash
# Install Smee client
npm install -g smee-client

# Forward webhooks from public Smee URL to local Express server
smee --url https://smee.io/AbcDeFG12345 --target http://localhost:3000/webhook
```

### Example 4: Creating a Repository Webhook via GitHub CLI
```bash
gh api --method POST \
  -H "Accept: application/vnd.github+json" \
  /repos/my-org/payment-service/hooks \
  -f name="web" \
  -F active=true \
  -F events[]="push" \
  -F events[]="pull_request" \
  -f config[url]="https://api.my-corp.com/github-events" \
  -f config[content_type]="json" \
  -f config[secret]="super_secure_webhook_passphrase"
```

### Example 5: Testing Webhook Deliveries via GitHub CLI
Redeliver a failed webhook event to test server fixes:
```bash
# List recent webhook deliveries
gh api /repos/my-org/payment-service/hooks/123456/deliveries \
  --jq '.[0] | {id: .id, status: .status_code, event: .event, delivered_at: .delivered_at}'

# Redeliver failed delivery
gh api --method POST \
  /repos/my-org/payment-service/hooks/123456/deliveries/987654/attempts
```

---

## 6. Best Practices

- **Always Verify HMAC Signatures**: Never process an incoming webhook payload without verifying the `X-Hub-Signature-256` header using `crypto.timingSafeEqual`.
- **Respond with HTTP 200 Fast**: GitHub webhooks timeout after 10 seconds. Process long-running workloads (e.g., builds or Slack messaging) asynchronously in background queues (AWS SQS, Redis) after returning `200 OK`.
- **Store Webhook Delivery UUIDs**: Record `X-GitHub-Delivery` in a persistent cache (e.g., Redis with a 24-hour TTL) to detect and deduplicate retried webhook deliveries.
- **Rotate App Private Keys Safely**: GitHub Apps allow multiple active private keys. Generate a second key, update your server, verify traffic, and then delete the old key.
- **Use Minimal Permission Sets**: When creating an App, request read-only access for resources you do not intend to modify. Modifying permissions later prompts administrators to re-approve the installation.

---

## 7. Common Mistakes

| Mistake | Impact | Correct Approach |
|:---|:---|:---|
| Using standard string equality (`===`) for HMAC checks | Vulnerable to timing attack side-channel exploits | Use constant-time comparison (`crypto.timingSafeEqual`) |
| Parsing raw body as JSON before hashing | Formatting/whitespace differences invalidate HMAC signature calculation | Capture raw byte buffer before JSON parsing (`req.rawBody`) |
| Performing heavy operations synchronously in webhook handler | GitHub times out after 10 seconds and flags webhook as unhealthy | Enqueue task in worker queue and immediately respond with HTTP 200 |
| Building enterprise bots as OAuth Apps or user PATs | Breaches security when employees depart; broad permissions | Build as a GitHub App with installation access tokens |
| Forgetting to configure SSL/TLS on webhook endpoint | Secrets and payload data transmitted unencrypted over WAN | Always mandate HTTPS endpoints for webhooks |
| Assuming webhook deliveries arrive in strict chronological order | Race conditions in state machines | Inspect event timestamps and commit SHAs rather than arrival order |

---

## 8. Performance, Security & Cost Considerations

- **Performance**: GitHub Apps receive independent, elevated rate limits (up to 15,000 requests per hour for organizations with >1,000 repos).
- **Security**: Private RSA keys must be stored in secure key vaults (AWS Secrets Manager, HashiCorp Vault), never committed to code repositories.
- **Cost**: Creating and installing GitHub Apps and configuring webhooks is completely free.

---

## 9. Comparison Table

| Attribute | Webhook | GitHub App | OAuth App |
|:---|:---|:---|:---|
| **Identity Type** | Passive HTTP Callback | First-class Platform Identity | User-delegated Identity |
| **Permissions** | N/A (Event notifications only) | Fine-grained, repository-scoped | Coarse, account-wide |
| **Token Lifespan** | None | 1 Hour (Installation Tokens) | Indefinite until revoked |
| **Rate Limit** | N/A | Scales with org size (up to 15,000/hr)| 5,000/hr tied to user |
| **Admin Approval**| Set by repo admin | Must be approved by org admin | Approved by individual user |

---

## 10. Real-World Interview Questions

<details>
<summary>1. Why should you always use `crypto.timingSafeEqual` instead of `===` when verifying webhook signatures?</summary>
Standard string equality comparison (`===`) terminates as soon as it encounters the first non-matching byte (short-circuit evaluation). Attackers can measure the microscopic time differences of HTTP responses to deduce characters of the signature one byte at a time (a timing attack). `timingSafeEqual` always compares the entire buffer length in constant time, preventing timing side-channel leaks.
</details>

<details>
<summary>2. How does the authentication lifecycle of a GitHub App work from private key to API call?</summary>
1. The App holds an RSA private key and its App ID.  
2. It generates a short-lived JSON Web Token (JWT) signed with the private key (valid max 10 minutes).  
3. It makes an API call to `POST /app/installations/{installation_id}/access_tokens` using the JWT in the `Authorization: Bearer` header.  
4. GitHub returns an ephemeral Installation Access Token (valid for 1 hour).  
5. The App uses this token to make standard REST/GraphQL requests against repository resources.
</details>

<details>
<summary>3. What is the webhook timeout threshold on GitHub, and how should webhook handlers be architected?</summary>
GitHub expects an HTTP response status (`2xx` or `3xx`) within **10 seconds**. If a handler takes longer, GitHub marks the delivery as failed and may disable the webhook after repeated timeouts. Webhook handlers should validate the HMAC signature, parse the event, push the payload onto an asynchronous message broker (AWS SQS, RabbitMQ, Kafka), and return `HTTP 200 OK` within milliseconds. Background workers then process the business logic.
</details>

<details>
<summary>4. Why are GitHub Apps immune to employee turnover?</summary>
OAuth apps and Personal Access Tokens execute under the context of the individual user who authorized them. When that user leaves the company and their GitHub/SSO account is de-provisioned, all associated tokens immediately become invalid, breaking mission-critical CI/CD pipelines. GitHub Apps are installed at the organization level as dedicated platform actors, completely decoupled from human employee lifecycles.
</details>

<details>
<summary>5. What is the purpose of the `X-GitHub-Delivery` header?</summary>
It contains a unique GUID generated for each webhook dispatch. Because GitHub operates an at-least-once delivery model, transient network retries can send duplicate events. Receivers use this UUID as an idempotency key to prevent executing the same action (such as sending a duplicate notification or re-triggering a build) twice.
</details>

<details>
<summary>6. How can an engineer test GitHub webhooks on a local `localhost:3000` development server?</summary>
Since GitHub cannot route HTTP POST requests to a private `localhost` address, developers use reverse webhook proxies such as **Smee.io** or **ngrok**. Smee provides a public URL that receives the GitHub POST request and forwards it over an outbound SSE (Server-Sent Events) tunnel to a local CLI client (`smee --url ... --target http://localhost:3000/webhook`).
</details>

<details>
<summary>7. What happens when a GitHub App requests updated or expanded permissions?</summary>
When a developer updates the requested permissions of an existing GitHub App, the changes do not take effect immediately on active installations. Organization owners receive an email notification and must review and approve the updated permission grant before the App can access the new APIs.
</details>

<details>
<summary>8. How do you redeliver a failed webhook delivery using GitHub CLI?</summary>
Query `/repos/{owner}/{repo}/hooks/{hook_id}/deliveries` to identify the delivery ID, and execute:
```bash
gh api --method POST /repos/{owner}/{repo}/hooks/{hook_id}/deliveries/{delivery_id}/attempts
```
</details>

---

## 11. Hands-On Exercises

<details>
<summary>Exercise 1: List repository webhooks via GitHub CLI</summary>

**Goal:** Check active webhooks on a repository.

```bash
gh api /repos/:owner/:repo/hooks --jq '.[].config.url'
```
</details>

<details>
<summary>Exercise 2: Set up a local Smee proxy for testing</summary>

**Goal:** Forward webhooks from Smee to localhost.

```bash
npx smee-client --url https://smee.io/new --target http://localhost:3000/webhook
```
</details>

<details>
<summary>Exercise 3: Compute an HMAC-SHA256 signature locally</summary>

**Goal:** Manually calculate signature using OpenSSL.

```bash
PAYLOAD='{"action":"opened"}'
SECRET="mysecret"
echo -n "$PAYLOAD" | openssl dgst -sha256 -hmac "$SECRET"
```
</details>

<details>
<summary>Exercise 4: Query GitHub App details via API</summary>

**Goal:** Look up an App's metadata using its slug.

```bash
gh api /apps/github-actions
```
</details>

<details>
<summary>Exercise 5: Inspect recent webhook deliveries</summary>

**Goal:** View response status codes for the last 5 deliveries.

```bash
gh api /repos/:owner/:repo/hooks/:hook_id/deliveries \
  --jq '.[0:5] | .[] | {id: .id, status: .status_code, event: .event}'
```
</details>

<details>
<summary>Exercise 6: Test timingSafeEqual implementation</summary>

**Goal:** Verify signature comparison in Node.js REPL.

```javascript
const crypto = require('crypto');
const a = Buffer.from('abc');
const b = Buffer.from('abc');
console.log(crypto.timingSafeEqual(a, b)); // true
```
</details>

---

## 12. Summary / Cheat Sheet

```text
================================================================================
                    GITHUB WEBHOOKS & APPS CHEAT SHEET
================================================================================
Webhook Signature  : X-Hub-Signature-256: sha256=<hmac_digest>
Verification Rule  : Always use crypto.timingSafeEqual over raw byte buffers
Idempotency Key    : X-GitHub-Delivery (deduplicate against multiple retries)
Timeout Rule       : Respond with HTTP 200 within 10s; offload work to queues
GitHub App Auth    : Private RSA Key -> Mint JWT (10m) -> Exchange for Installation Token (1h)
App Identity       : Native actor (my-bot[bot]); zero seat cost; immune to employee turnover
Local Debugging    : Use smee.io or ngrok to tunnel webhooks to localhost
Delivery Inspection: gh api /repos/:owner/:repo/hooks/:id/deliveries
================================================================================
```

---

## 13. Further Reading
- [GitHub Docs: About webhooks](https://docs.github.com/en/webhooks/about-webhooks)
- [GitHub Docs: Securing your webhooks](https://docs.github.com/en/webhooks/using-webhooks/validating-webhook-deliveries)
- [GitHub Docs: About creating GitHub Apps](https://docs.github.com/en/apps/creating-github-apps/about-creating-github-apps)
- [Probot: Framework for building GitHub Apps in Node.js](https://probot.github.io/)

---

[<- Previous: GitHub REST & GraphQL APIs](./02-github-rest-and-graphql-api.md) | [Module 08 Overview](./README.md) | [Next: Automating with gh & Actions ->](./04-automating-with-gh-and-actions.md)
