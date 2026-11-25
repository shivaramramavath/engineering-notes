---
title: "GitHub Developer APIs: REST (v3) & GraphQL (v4)"
category: "04-devops"
subcategory: "git-github / 08-gh-cli-and-api"
tags: ["git", "github", "api", "rest", "graphql", "gh-api", "octokit", "automation", "devops"]
---

# GitHub Developer APIs: REST (v3) & GraphQL (v4)

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

Automating enterprise engineering workflows—auditing permissions, batch-updating repository settings, triaging security alerts, or building custom release pipelines—requires programmatic access to GitHub's backend. GitHub provides two primary API architectures:
1. **GitHub REST API (v3)**: A resource-oriented, standard HTTP API returning predictable JSON payloads. Ideal for simple CRUD operations, downloading binary assets, and straightforward scripts.
2. **GitHub GraphQL API (v4)**: A declarative, type-safe API that allows developers to request precisely the data fields needed in a single HTTP request. Essential for complex multi-resource relationships (e.g., retrieving a PR along with its reviews, issue comments, timeline events, and commit statuses) without suffering from REST over-fetching or N+1 request waterfalls.

Using the `gh api` subcommand, developers can execute raw authenticated REST and GraphQL queries directly from the terminal without handling raw HTTP headers, curl authentication, or manual OAuth exchanges.

---

## 2. Core Concepts

### REST API Architecture (v3)
- **Endpoint Structure**: Organized around resource paths (e.g., `GET /repos/{owner}/{repo}/issues`).
- **HTTP Verbs**: Maps directly to standard semantics (`GET`, `POST`, `PATCH`, `PUT`, `DELETE`).
- **Headers**: Mandates `Accept: application/vnd.github+json` and `X-GitHub-Api-Version: 2022-11-28`.
- **Pagination**: Delivered via HTTP response `Link` headers containing `rel="next"`, `rel="last"`, `rel="prev"`.

### GraphQL API Architecture (v4)
- **Single Endpoint**: All operations POST to `https://api.github.com/graphql`.
- **Schema & Types**: Strongly typed schema defining Queries (reads) and Mutations (writes).
- **Cursor-Based Pagination**: Employs Relay-style connections (`edges`, `node`, `pageInfo { endCursor, hasNextPage }`).
- **Precise Data Fetching**: Completely prevents over-fetching and under-fetching.

### Rate Limiting & Quotas
- **Primary Rate Limits**:
  - Personal Tokens / GitHub Apps: 5,000 requests per hour (15,000 per hour for Enterprise Cloud).
  - Unauthenticated: 60 requests per hour.
  - GraphQL Cost Scoring: Rate limits are calculated based on query complexity points (up to 500,000 points per hour) rather than flat request counts.
- **Secondary (Abuse) Rate Limits**: Triggered by concurrency spikes (making > 100 concurrent requests or > 1 mutation per second).

---

## 3. How It Works Internally

```
REST API Workflow (v3)                            GraphQL API Workflow (v4)
--------------------------------------            --------------------------------------
Client: GET /repos/org/api/pulls/1                Client: POST /graphql { query: ... }
           |                                                 |
           v                                                 v
Server: Returns full PR payload (15KB)            Server: Executes Type Schema Resolver
           |                                                 |
           | (Requires 2nd call for reviews)                 v
           v                                      Returns EXACT requested fields (1KB):
Client: GET /repos/org/api/pulls/1/reviews         {
           |                                         "data": {
           v                                           "repository": {
Server: Returns review array (10KB)                      "pullRequest": {
                                                           "title": "fix: auth",
                                                           "reviews": { "totalCount": 2 }
                                                         }
                                                       }
                                                     }
                                                   }
```

---

## 4. Syntax & Usage

### Executing REST Queries via `gh api`
```bash
# Basic GET request
gh api repos/my-org/core-api

# POST request with JSON fields
gh api --method POST repos/my-org/core-api/issues \
  -f title="Bug: Memory leak" \
  -f body="Repro details..." \
  -F labels[]="bug"

# Paginated GET request
gh api --paginate repos/my-org/core-api/commits
```

### Executing GraphQL Queries via `gh api graphql`
```bash
gh api graphql -f query='
  query($owner: String!, $name: String!) {
    repository(owner: $owner, name: $name) {
      name
      stargazerCount
      issues(states: OPEN) {
        totalCount
      }
    }
  }
' -F owner="my-org" -F name="core-api"
```

---

## 5. Practical Examples

### Example 1: Querying PR Reviews and Mergeable Status via GraphQL
Fetch PR title, mergeability, and reviewer comments in a single query:
```bash
gh api graphql -f query='
  query($repo: String!, $owner: String!, $pr: Int!) {
    repository(owner: $owner, name: $repo) {
      pullRequest(number: $pr) {
        title
        mergeable
        reviewDecision
        reviews(first: 5) {
          nodes {
            author { login }
            state
            submittedAt
          }
        }
      }
    }
  }
' -F owner="my-org" -F repo="payment-service" -F pr=42
```

### Example 2: Automatic REST Pagination with `--paginate` and `--jq`
Retrieve every collaborator across an entire organization without writing pagination loops:
```bash
gh api --paginate /orgs/my-org/members \
  --jq '.[].login' | sort -u
```

### Example 3: Enabling Vulnerability Alerts via REST API
```bash
gh api --method PUT \
  -H "Accept: application/vnd.github+json" \
  /repos/my-org/payment-service/vulnerability-alerts
```

### Example 4: Querying Rate Limit Status
Check remaining primary and secondary API points:
```bash
gh api rate_limit --jq '{
  core_remaining: .resources.core.remaining,
  core_reset: (.resources.core.reset | todate),
  graphql_remaining: .resources.graphql.remaining
}'
```

### Example 5: GraphQL Mutation to Add Comment to an Issue
```bash
gh api graphql -f query='
  mutation($subjectId: ID!, $body: String!) {
    addComment(input: {subjectId: $subjectId, body: $body}) {
      commentEdge {
        node {
          id
          url
        }
      }
    }
  }
' -F subjectId="I_kwDOAbcdef12345" -F body="Approved by automated release bot."
```

---

## 6. Best Practices

- **Target GraphQL for Complex Read Models**: Avoid multiple sequential REST queries (the N+1 anti-pattern). Use GraphQL to fetch nested relationships (PR -> commits -> reviews -> checks) in a single request.
- **Always Pass the API Version Header in REST**: Include `-H "X-GitHub-Api-Version: 2022-11-28"` to ensure long-term backward compatibility against breaking schema changes.
- **Implement Exponential Backoff**: When writing automated integrations, monitor the `x-ratelimit-remaining` and `retry-after` response headers to handle secondary rate limit spikes gracefully.
- **Use Octokit SDKs in Code**: In TypeScript, Python, or Go applications, use official Octokit libraries (`@octokit/rest`, `@octokit/graphql`) rather than raw HTTP clients to gain automatic retries, pagination, and authentication handling.
- **Prefer Node IDs over Database IDs**: Use GraphQL Global Node IDs (`node_id`, e.g., `MDQ6VXNlcjE=`) for referencing resources across API versions.

---

## 7. Common Mistakes

| Mistake | Impact | Correct Approach |
|:---|:---|:---|
| Scraping web pages instead of using the API | Extremely fragile; breaks on UI redesigns | Use stable REST or GraphQL API endpoints |
| Forgetting `--paginate` on REST lists | Returns only the first 30 items by default | Append `--paginate` to collect complete datasets |
| Rapidly polling endpoints in an un-throttled loop | Triggers secondary rate limit (HTTP 403 / 429) | Use Webhooks for event-driven updates or throttle polling |
| Passing string types for integers in `gh api` | API rejects schema validation | Use `-F <key>=<int>` instead of `-f <key>=<str>` |
| Requesting unbounded lists in GraphQL (`first: 1000`) | GraphQL rejects query with max node limit error | Keep `first:` $\le 100$ and paginate using cursors |
| Hardcoding personal tokens in automation scripts | High security risk and token attribution failure | Use GitHub App installation tokens |

---

## 8. Performance, Security & Cost Considerations

- **Performance**: GraphQL drastically reduces payload sizes (often >85% smaller than REST payloads) and reduces round-trip network latency.
- **Security**: Scope tokens strictly. Do not use an admin token when only reading issue comments. Ensure all webhook callbacks validate HMAC signatures.
- **Cost**: APIs are free to use within the standard rate limits. GitHub Enterprise accounts receive 15,000 requests per hour per user/app.

---

## 9. Comparison Table

| Feature | GitHub REST API (v3) | GitHub GraphQL API (v4) |
|:---|:---|:---|
| **Paradigm** | Resource / URI-based | Schema / Query-based |
| **Payload Size** | Fixed JSON schema (Over-fetching) | Custom requested fields only |
| **Network Calls** | Multiple calls for nested data (N+1) | Single composite query |
| **Pagination** | HTTP `Link` header (`?page=2`) | Cursor-based (`after: $cursor`) |
| **Rate Limit Model**| 5,000 requests per hour | 500,000 points per hour (calculated)|
| **File Blobs** | Native binary download support | Encoded strings only |

---

## 10. Real-World Interview Questions

<details>
<summary>1. When would you choose the GitHub GraphQL API over the REST API?</summary>
Choose GraphQL when fetching nested relational data (such as a pull request with its commits, comments, reviewers, and check runs) in a single round-trip, or when bandwidth and payload size must be minimized. Choose REST when performing simple single-resource operations, uploading/downloading binary release assets, or when using tooling that does not support GraphQL.
</details>

<details>
<summary>2. How does GitHub's GraphQL rate limiting model differ from REST rate limiting?</summary>
The REST API enforces a flat limit of 5,000 requests per hour regardless of how simple or heavy the request is. The GraphQL API assigns a point cost to each query based on its complexity (e.g., number of nodes returned and nesting depth), allowing up to 500,000 points per hour. Clients can inspect query costs by requesting the `rateLimit { cost, remaining }` fields.
</details>

<details>
<summary>3. What is the difference between primary rate limits and secondary rate limits on GitHub?</summary>
Primary rate limits are hourly quotas based on account tier (e.g., 5,000 requests/hour). Secondary rate limits are dynamic concurrency and volume throttles designed to protect infrastructure stability. They trigger when a client makes more than 100 concurrent requests, submits more than 1 mutation per second, or consumes excessive CPU within a short window.
</details>

<details>
<summary>4. What HTTP headers are critical when interacting with the GitHub REST API?</summary>
- `Authorization: Bearer <TOKEN>` (or `token <TOKEN>`)
- `Accept: application/vnd.github+json` (Specifies official response format)
- `X-GitHub-Api-Version: 2022-11-28` (Locks the API schema version)
- `User-Agent: <AppName>` (Mandatory; GitHub rejects requests without a User-Agent)
</details>

<details>
<summary>5. How does cursor-based pagination work in GitHub's GraphQL API?</summary>
Each item in a collection has an opaque cursor string. The query specifies `first: N, after: "CURSOR"`. The response includes `pageInfo { hasNextPage, endCursor }`. To fetch the next page, the client executes the query passing `after: endCursor` until `hasNextPage` evaluates to false.
</details>

<details>
<summary>6. How can you distinguish between string and non-string parameters using `gh api`?</summary>
In `gh api`:
- `-f key=val` passes `val` as a JSON **string**.
- `-F key=val` passes `val` with **type inference** (integers, booleans, and arrays are converted to their native JSON types).
</details>

<details>
<summary>7. What is a Global Node ID in GitHub APIs?</summary>
A Global Node ID is a unique, base64-encoded identifier assigned to every object across the entire GitHub platform (users, repos, issues, PRs). It allows any resource to be looked up directly in GraphQL using the root `node(id: "...")` field, regardless of repository context.
</details>

<details>
<summary>8. How do you handle an HTTP 403 or 429 response indicating rate limit exhaustion?</summary>
Inspect the `x-ratelimit-reset` (UTC epoch timestamp) and `retry-after` (seconds) response headers. The client script must pause execution and sleep until the indicated reset time before retrying with an exponential backoff algorithm.
</details>

---

## 11. Hands-On Exercises

<details>
<summary>Exercise 1: Query current user profile via REST</summary>

**Goal:** Fetch authenticated user details with `gh api`.

```bash
gh api user --jq '{login: .login, id: .id, name: .name}'
```
</details>

<details>
<summary>Exercise 2: Query repo stars and open issues via GraphQL</summary>

**Goal:** Execute a basic GraphQL query.

```bash
gh api graphql -f query='
  query {
    viewer {
      login
      repositories(first: 3) {
        nodes {
          name
          stargazerCount
        }
      }
    }
  }
'
```
</details>

<details>
<summary>Exercise 3: Inspect GraphQL query cost</summary>

**Goal:** Include rateLimit node to monitor point consumption.

```bash
gh api graphql -f query='
  query {
    rateLimit {
      cost
      remaining
      resetAt
    }
    viewer { login }
  }
'
```
</details>

<details>
<summary>Exercise 4: Paginate open issues via REST</summary>

**Goal:** Retrieve all issue numbers using `--paginate`.

```bash
gh api --paginate /repos/:owner/:repo/issues --jq '.[].number'
```
</details>

<details>
<summary>Exercise 5: Add a label to an issue using REST</summary>

**Goal:** POST an array of labels to an issue.

```bash
gh api --method POST /repos/:owner/:repo/issues/1/labels \
  -F labels[]="triage"
```
</details>

<details>
<summary>Exercise 6: Check secondary rate limit response headers</summary>

**Goal:** Inspect HTTP response headers using `gh api --include`.

```bash
gh api --include /rate_limit | grep -i "x-ratelimit"
```
</details>

---

## 12. Summary / Cheat Sheet

```text
================================================================================
                    GITHUB DEVELOPER APIS (REST & GRAPHQL)
================================================================================
REST API Base      : https://api.github.com (GET/POST/PATCH/DELETE)
GraphQL Endpoint   : https://api.github.com/graphql (POST only)
CLI Execution      : gh api <endpoint> | gh api graphql -f query='...'
REST Pagination    : gh api --paginate <endpoint>
Type Inference     : -f key=str (String) | -F key=val (Native JSON / Int / Bool)
Headers Required   : Accept: application/vnd.github+json, X-GitHub-Api-Version: 2022-11-28
Rate Limits        : REST = 5,000 req/hr | GraphQL = 500,000 points/hr
Rate Limit Check   : gh api rate_limit
================================================================================
```

---

## 13. Further Reading
- [GitHub REST API Documentation](https://docs.github.com/en/rest)
- [GitHub GraphQL API Explorer & Docs](https://docs.github.com/en/graphql)
- [Octokit JavaScript SDK](https://github.com/octokit/octokit.js)
- [GraphQL Official Specification](https://spec.graphql.org/)

---

[<- Previous: GitHub CLI Essentials](./01-gh-cli-essentials.md) | [Module 08 Overview](./README.md) | [Next: Webhooks & GitHub Apps ->](./03-webhooks-and-apps.md)
