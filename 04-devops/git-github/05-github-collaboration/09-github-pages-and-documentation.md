---
title: "GitHub Pages and Technical Documentation Hosting"
category: "04-devops"
subcategory: "git-github / 05-github-collaboration"
tags: ["github", "github-pages", "documentation", "static-site-generator", "jekyll", "docusaurus", "custom-domain"]
difficulty: "Beginner-Intermediate"
estimated_read_time: "22 min"
prerequisites:
  - "04-github-actions/01-core-concepts"
  - "04-github-actions/15-cd-and-deployment"
last_updated: "2026-11-25"
---

# GitHub Pages and Technical Documentation Hosting

> **Navigation**: [← Previous: 08-forking-and-open-source-contributions.md](file:///e:/Learning/engineering-notes/04-devops/git-github/05-github-collaboration/08-forking-and-open-source-contributions.md) | [Module README](file:///e:/Learning/engineering-notes/04-devops/git-github/05-github-collaboration/README.md) | [Next: 10-github-releases-and-packages.md →](file:///e:/Learning/engineering-notes/04-devops/git-github/05-github-collaboration/10-github-releases-and-packages.md)

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Site Types: User/Org Sites vs Project Sites](#2-site-types-userorg-sites-vs-project-sites)
3. [Publishing Source: Branch Deploy vs GitHub Actions](#3-publishing-source-branch-deploy-vs-github-actions)
4. [Modern Deployment via GitHub Actions](#4-modern-deployment-via-github-actions)
5. [Custom Domains & Automatic HTTPS](#5-custom-domains--automatic-https)
6. [Static Site Generators (Docusaurus, VitePress, Hugo)](#6-static-site-generators-docusaurus-vitepress-hugo)
7. [Practical Examples](#7-practical-examples)
8. [Best Practices](#8-best-practices)
9. [Common Mistakes](#9-common-mistakes)
10. [Comparison: GitHub Pages vs Vercel vs Cloudflare Pages](#10-comparison-github-pages-vs-vercel-vs-cloudflare-pages)
11. [Interview Questions & Answers](#11-interview-questions--answers)
12. [Practice Exercises](#12-practice-exercises)
13. [Summary & Cheat Sheet](#13-summary--cheat-sheet)
14. [Further Reading](#14-further-reading)

---

## 1. What & Why

### What is GitHub Pages?
GitHub Pages is a static web hosting service provided directly by GitHub that serves HTML, CSS, JavaScript, and static assets straight from a Git repository to the public internet or private organization members.

### Why Do We Use It?
1. **Zero Infrastructure Web Hosting**: Host open-source project documentation, portfolios, architectural decision records (ADRs), and internal API catalogs without provisioning web servers, Nginx containers, or cloud storage buckets.
2. **Automated CI/CD Deployment**: Update docs automatically whenever markdown files are pushed to `main` using GitHub Actions.
3. **Free Global CDN & SSL**: Backed by Fastly's global CDN with automated Let's Encrypt TLS/SSL certificates and custom domain support.
4. **Git-Native Versioning**: Documentation is version-controlled in the exact same repository and commit history as the source code it describes.

---

## 2. Site Types: User/Org Sites vs Project Sites

```
┌─────────────────────────────────────────────────────────────────────────┐
│ Two Distinct Types of GitHub Pages Sites                                │
├──────────────────────────┬──────────────────────┬───────────────────────┤
│ Dimension                │ User / Org Site      │ Project Site          │
├──────────────────────────┼──────────────────────┼───────────────────────┤
│ Repository Name          │ `<user>.github.io`   │ `<any-repo-name>`     │
│ Published URL            │ `https://<user>.github.io` │ `https://<user>.github.io/<repo>` │
│ Limit                    │ Exactly 1 per user/org │ Unlimited (1 per repo)│
│ Root Path (`base`)       │ `/` (Root domain)    │ `/<repo>/` (Subpath)  │
└──────────────────────────┴──────────────────────┴───────────────────────┘
```

> **Critical Gotcha**: On a **Project Site**, all asset links (`<img src="/logo.png">`) will fail with 404 because the site lives under `/<repo>/`. Asset paths must be relative or prefix the repository name!

---

## 3. Publishing Source: Branch Deploy vs GitHub Actions

In repository settings (`Settings -> Pages -> Build and deployment`):
- **Deploy from a branch (Legacy)**: GitHub builds files from `/ (root)` or `/docs` of a branch (e.g. `gh-pages` or `main`) using legacy Jekyll.
- **GitHub Actions (Modern & Recommended)**: An automated GitHub Actions workflow compiles your documentation with any framework (Node, Vite, Python, Hugo) and deploys the output directly via GitHub Pages APIs.

---

## 4. Modern Deployment via GitHub Actions

Modern Pages deployments use two official actions:
1. `actions/upload-pages-artifact@v3`: Packages the static directory into a signed tarball.
2. `actions/deploy-pages@v4`: Deploys the artifact directly to GitHub's CDN.

### Complete Production Workflow:
```yaml
# .github/workflows/deploy-pages.yml
name: Deploy Documentation to GitHub Pages

on:
  push:
    branches: [main]
  workflow_dispatch:

permissions:
  contents: read
  pages: write        # Required to deploy to GitHub Pages
  id-token: write     # Required for OIDC CDN verification

concurrency:
  group: "pages"
  cancel-in-progress: false # Never cancel an in-flight deployment

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - name: Checkout repository
        uses: actions/checkout@v4

      - name: Setup Node.js 20
        uses: actions/setup-node@v4
        with:
          node-version: 20
          cache: npm

      - name: Install dependencies & Build static docs
        run: |
          npm ci
          npm run build # Emits static files to dist/

      - name: Upload Pages artifact
        uses: actions/upload-pages-artifact@v3
        with:
          path: dist/ # Folder containing index.html

  deploy:
    environment:
      name: github-pages
      url: ${{ steps.deployment.outputs.page_url }}
    needs: build
    runs-on: ubuntu-latest
    steps:
      - name: Deploy to GitHub Pages
        id: deployment
        uses: actions/deploy-pages@v4
```

---

## 5. Custom Domains & Automatic HTTPS

### 1. The `CNAME` File
When configuring a custom domain (e.g. `docs.example.com`), GitHub writes a file named `CNAME` at the root of the published artifact containing the domain string:
```text
docs.example.com
```

### 2. DNS Configuration
- **For Subdomains** (`docs.example.com`):
  Add a `CNAME` DNS record pointing to your GitHub username:
  ```text
  Type: CNAME | Host: docs | Value: <username>.github.io
  ```
- **For Apex Domains** (`example.com`):
  Add `A` records pointing to GitHub's 4 IP addresses:
  ```text
  185.199.108.153
  185.199.109.153
  185.199.110.153
  185.199.111.153
  ```

### 3. Automatic HTTPS
GitHub automatically issues a TLS certificate via Let's Encrypt. Check **"Enforce HTTPS"** in repository settings once DNS propagation completes.

---

## 6. Static Site Generators (Docusaurus, VitePress, Hugo)

While GitHub historically used Jekyll, modern engineering teams build documentation using modern static site generators:
- **Docusaurus (React)**: High-performance documentation engine with versioning, search, and MDX support.
- **VitePress (Vue)**: Ultra-fast documentation site generator powered by Vite.
- **Hugo (Go)**: Compiles thousands of markdown pages into HTML in milliseconds.
- **Astro**: Zero-JS by default content sites.

---

## 7. Practical Examples

### Example 1: Deploying VitePress Docs via Actions
```yaml
name: VitePress Docs
on:
  push:
    branches: [main]

permissions:
  contents: read
  pages: write
  id-token: write

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
        with:
          node-version: 20
          cache: npm
      - run: npm ci && npm run docs:build
      - uses: actions/upload-pages-artifact@v3
        with:
          path: docs/.vitepress/dist

  deploy:
    needs: build
    runs-on: ubuntu-latest
    steps:
      - uses: actions/deploy-pages@v4
```

### Example 2: Adding a Custom Domain via GitHub CLI
```bash
# Set custom domain on repository
gh api --method PUT /repos/:owner/:repo/pages \
  -f cname="docs.my-org.com"
```

### Example 3: Verifying DNS Propagation with `dig`
```bash
dig +noall +answer CNAME docs.my-org.com
# docs.my-org.com. 300 IN CNAME my-org.github.io.
```

---

## 8. Best Practices

1. **Use GitHub Actions as Publishing Source**: Avoid maintaining separate `gh-pages` Git branches. Let GitHub Actions compile and deploy artifacts directly from `main`.
2. **Always Enable "Enforce HTTPS"**: Ensure all web traffic is encrypted with Let's Encrypt TLS.
3. **Configure Correct Base URL Path**: In project sites, configure your generator's base path to `/<repo-name>/` (e.g. `base: '/my-project/'` in `vite.config.ts`).
4. **Set Up a Custom 404 Page**: Provide a `404.html` file in your root output to handle broken documentation links gracefully.
5. **Add `.nojekyll` File**: If your build output contains folders starting with an underscore (e.g. `_next/` or `_assets/`), include an empty `.nojekyll` file at the root so GitHub does not ignore them.

---

## 9. Common Mistakes

| Mistake | Consequence | Root Cause | Fix / Prevention |
|:---|:---|:---|:---|
| Asset 404s on Project Site | CSS/JS fails to load on project site | Hardcoding `/styles.css` instead of `/<repo>/styles.css` | Configure `base: '/<repo>/'` in build config |
| Assets starting with `_` ignored | Missing scripts and CSS files | GitHub assumes Jekyll and ignores `_` directories | Add an empty `.nojekyll` file |
| Build wipes `CNAME` file | Custom domain disappears on every deploy | Build script overwrites output without preserving `CNAME` | Place `CNAME` in `public/` or `static/` directory |
| Missing `pages: write` permission | Deploy step fails with HTTP 403 | Missing Actions permission | Add `permissions: pages: write` |
| `cancel-in-progress: true` on Pages | Partial or broken CDN deployment | Concurrency cancellation | Set `cancel-in-progress: false` |

---

## 10. Comparison: GitHub Pages vs Vercel vs Cloudflare Pages

| Feature | GitHub Pages | Vercel | Cloudflare Pages |
|:---|:---|:---|:---|
| **Primary Use** | Documentation & static sites | Full-stack Next.js / Serverless | Edge-rendered static sites & Workers |
| **Server-Side Rendering (SSR)**| **No** (Static only) | Yes (Serverless functions) | Yes (Edge functions) |
| **Custom Domains & SSL** | Free | Free | Free |
| **Bandwidth Limits** | 100 GB / month | 100 GB / month (Hobby) | **Unlimited** |
| **Build Limits** | Standard Actions quotas | 6,000 build minutes | Unlimited |

---

## 11. Interview Questions & Answers

<details>
<summary><strong>Q1: What is the primary difference between a GitHub User Site and a GitHub Project Site?</strong></summary>

- **User / Org Site**: Tied to a repository named exactly `<username>.github.io`. Served at the root of the domain (`https://<username>.github.io/`). You can only have one user/org site per account.
- **Project Site**: Created from any standard repository. Served at a subpath under the user domain (`https://<username>.github.io/<repository-name>/`). You can have an unlimited number of project sites.
</details>

<details>
<summary><strong>Q2: Why must modern static site generators include a `.nojekyll` file when deploying to GitHub Pages?</strong></summary>

By default, GitHub Pages processes all deployments through Jekyll. In Jekyll, any folder or file that begins with an underscore (`_assets`, `_next`, `_nuxt`) is treated as internal metadata and omitted from the published website. Modern frontend frameworks (Next.js, Nuxt, Docusaurus) generate assets in `_next/` or `_assets/`. Placing an empty `.nojekyll` file at the root tells GitHub to disable Jekyll processing completely and serve all files directly.
</details>

<details>
<summary><strong>Q3: What minimal GitHub Actions permissions are required to deploy a site to GitHub Pages?</strong></summary>

The workflow requires:
```yaml
permissions:
  contents: read
  pages: write
  id-token: write
```
`pages: write` allows uploading and publishing to the Pages CDN, and `id-token: write` is required for OIDC authentication with GitHub's internal Pages deployment service.
</details>

<details>
<summary><strong>Q4: Why do relative links and asset paths frequently break when publishing a Project Site? How is this resolved?</strong></summary>

Because Project Sites are served at `https://<user>.github.io/<repo>/`, any absolute reference in HTML like `<script src="/main.js">` will resolve to `https://<user>.github.io/main.js` (a 404 error) rather than `https://<user>.github.io/<repo>/main.js`.
**Resolution**: Configure the static site generator's base URL parameter (e.g. `base: '/my-repo/'` in Vite, or `baseUrl: '/my-repo/'` in Docusaurus) so that all compiled asset paths automatically prepend the repository prefix.
</details>

<details>
<summary><strong>Q5: Can GitHub Pages sites be kept private to organization members only?</strong></summary>

Yes, in **GitHub Enterprise Cloud**. Under repository settings -> **Pages**, maintainers can toggle visibility between **Public** and **Private**. When set to Private, visitors must authenticate via GitHub SAML/SSO and have read permissions on the repository to view the website.
</details>

<details>
<summary><strong>Q6: How does GitHub Pages handle custom domain verification and SSL certificate issuance?</strong></summary>

When a custom domain is configured, GitHub writes a `CNAME` file. GitHub verifies domain ownership via a DNS `TXT` record (`_github-pages-challenge-<username>`). Once verified, GitHub automatically provisions a free TLS certificate via Let's Encrypt and renews it automatically before expiration.
</details>

<details>
<summary><strong>Q7: What are the bandwidth and repository size limits for GitHub Pages?</strong></summary>

GitHub Pages sites are subject to soft usage limits:
- Published site size limit: **1 GB**.
- Monthly bandwidth limit: **100 GB per month**.
- Build timeout: **10 minutes** (when using legacy Jekyll build).
If a site exceeds these limits, GitHub may throttle or suspend the site.
</details>

<details>
<summary><strong>Q8: What is the benefit of using `actions/deploy-pages@v4` over the legacy `peaceiris/actions-gh-pages` action?</strong></summary>

The legacy `peaceiris` action required committing compiled HTML files into a dedicated Git branch (`gh-pages`). Over time, committing gigabytes of compiled assets bloated the Git object store and polluted repository commit history.
`actions/deploy-pages@v4` interacts directly with GitHub's native Pages deployment API without writing any commits to your Git branches, keeping repository history clean.
</details>

---

## 12. Practice Exercises

<details>
<summary><strong>Exercise 1: Create a Simple Static HTML Pages Deployment</strong></summary>

**Scenario**: Deploy a single `index.html` file using modern GitHub Actions.
```yaml
name: Deploy Static HTML
on: push
permissions:
  pages: write
  id-token: write
jobs:
  deploy:
    runs-on: ubuntu-latest
    steps:
      - run: mkdir public && echo "<h1>Hello GitHub Pages</h1>" > public/index.html
      - uses: actions/upload-pages-artifact@v3
        with:
          path: public/
      - uses: actions/deploy-pages@v4
```
</details>

<details>
<summary><strong>Exercise 2: Add `.nojekyll` File</strong></summary>

**Scenario**: Ensure underscore directories are preserved.
```bash
touch dist/.nojekyll
```
</details>

<details>
<summary><strong>Exercise 3: Configure Base URL in Vite Config</strong></summary>

**Scenario**: Configure Vite for project site deployment.
```javascript
// vite.config.js
export default {
  base: '/my-project-repo/',
}
```
</details>

<details>
<summary><strong>Exercise 4: Verify CNAME DNS Record</strong></summary>

**Scenario**: Query DNS resolution for custom domain using `nslookup`.
```bash
nslookup docs.my-org.com
```
</details>

<details>
<summary><strong>Exercise 5: Configure Custom Domain via CLI</strong></summary>

**Scenario**: Attach custom domain to GitHub Pages site.
```bash
gh api --method PUT /repos/:owner/:repo/pages -f cname="docs.my-org.com"
```
</details>

<details>
<summary><strong>Exercise 6: Query Pages Status via GitHub CLI</strong></summary>

**Scenario**: Check build and deployment status.
```bash
gh api /repos/:owner/:repo/pages
```
</details>

---

## 13. Summary & Cheat Sheet

```yaml
# Modern GitHub Pages Action Template
name: Deploy Pages
on:
  push:
    branches: [main]

permissions:
  contents: read
  pages: write
  id-token: write

concurrency:
  group: "pages"
  cancel-in-progress: false

jobs:
  deploy:
    environment:
      name: github-pages
      url: ${{ steps.deployment.outputs.page_url }}
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - run: npm ci && npm run build
      - uses: actions/upload-pages-artifact@v3
        with:
          path: dist/
      - id: deployment
        uses: actions/deploy-pages@v4
```

---

## 14. Further Reading
- [GitHub Docs - About GitHub Pages](https://docs.github.com/en/pages/getting-started-with-github-pages/about-github-pages)
- [Using a Custom Domain with GitHub Pages](https://docs.github.com/en/pages/configuring-a-custom-domain-for-your-github-pages-site)
- [Docusaurus Deployment Guide for GitHub Pages](https://docusaurus.io/docs/deployment#deploying-to-github-pages)
- [05-github-collaboration/10-github-releases-and-packages.md](file:///e:/Learning/engineering-notes/04-devops/git-github/05-github-collaboration/10-github-releases-and-packages.md)
