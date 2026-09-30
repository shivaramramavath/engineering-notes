# Publishing a Package

Putting your own code on the npm registry so it can be installed with `npm install your-package-name`, the same way you install any other dependency.

## Prerequisites

```bash
npm adduser        # or: npm login
```

Creates or logs into an npm account — required before you can publish anything.

---

## Preparing `package.json`

```json
{
  "name": "my-awesome-package",
  "version": "1.0.0",
  "description": "A short, clear description of what this does",
  "main": "dist/index.js",
  "type": "module",
  "files": ["dist"],
  "keywords": ["utility", "helper"],
  "author": "Your Name <you@example.com>",
  "license": "MIT",
  "repository": {
    "type": "git",
    "url": "https://github.com/you/my-awesome-package.git"
  }
}
```

Fields worth getting right before publishing:

- **`name`** — must be globally unique across all of npm; check `npm view <name>` first to see if it's taken
- **`main`** — the entry point file loaded when someone `require()`s/`import`s your package
- **`files`** — an allowlist of what actually gets published; without it, npm publishes nearly everything not excluded by `.npmignore`/`.gitignore`, which often includes things you don't want shipped (tests, source before compilation, config files)
- **`license`** — required for most organizations to even consider using a package at all

### Scoped packages

```json
{
  "name": "@yourusername/my-package"
}
```

A **scope** (`@yourusername/`) namespaces your package under your account/organization — useful when the unscoped name you wanted is already taken, or when publishing several related packages under one umbrella.

---

## `.npmignore` — excluding files from the published package

```
# .npmignore
src/
tests/
.env
*.test.js
```

If `.npmignore` doesn't exist, npm falls back to `.gitignore`. If `files` is set in `package.json` (recommended, shown above), it takes precedence as an explicit allowlist — generally the more predictable approach, since you're stating what _should_ be included rather than trying to exclude everything that shouldn't.

---

## Dry-run before actually publishing

```bash
npm pack --dry-run
```

Shows exactly which files would be included in the published package, without actually publishing anything — always worth checking before your first real publish, since an overlooked `files`/`.npmignore` mistake can ship secrets or bloat the package unnecessarily.

```bash
npm pack
```

Creates an actual `.tgz` file locally, identical to what would be uploaded — useful for inspecting the contents directly, or for testing an install from the local file before publishing for real:

```bash
npm install ./my-awesome-package-1.0.0.tgz
```

---

## Publishing

```bash
npm publish
```

```bash
npm publish --access public    # required the FIRST time for a scoped (@username/x) package —
                                 # scoped packages default to private, which requires a paid plan
```

Once published, that exact version number can **never be reused or overwritten** — even `npm unpublish`ing it doesn't free the version number back up for reuse. This is a deliberate npm policy to prevent a supply-chain-style bait-and-switch where a previously-trusted version number suddenly points at different code.

---

## Versioning a new release

```bash
npm version patch    # 1.0.0 → 1.0.1
npm version minor      # 1.0.0 → 1.1.0
npm version major        # 1.0.0 → 2.0.0
```

`npm version` bumps the version in `package.json`, creates a Git commit for that change, and tags it — following semantic versioning (`01-semantic-versioning.md`) is expected practice so consumers' `^`/`~` ranges behave correctly against your releases.

```bash
npm version patch
npm publish
git push --follow-tags
```

A typical release flow: bump the version, publish to npm, then push the commit and tag `npm version` created back to your Git remote.

---

## Publishing with a build step

Most real packages are written in TypeScript or use modern syntax that needs compiling before publishing:

```json
{
  "scripts": {
    "build": "tsc",
    "prepublishOnly": "npm run build"
  },
  "files": ["dist"],
  "main": "dist/index.js"
}
```

`prepublishOnly` runs automatically right before `npm publish` — a safeguard ensuring the compiled `dist/` output is always fresh and can't be accidentally skipped before a release goes out.

---

## Deprecating a version (instead of unpublishing)

```bash
npm deprecate my-awesome-package@1.0.0 "Critical bug, please upgrade to 1.0.1"
```

Shows a warning to anyone installing that specific version, without removing it entirely — generally the better option over `npm unpublish`, which npm restricts heavily (only allowed within 72 hours of publishing, specifically to prevent the kind of ecosystem disruption a widely-depended-upon package suddenly vanishing would cause.

## Common mistakes

- **Forgetting `files` in `package.json`** — can accidentally publish source maps, tests, `.env` files, or anything else not explicitly excluded.
- **Publishing a scoped package without `--access public`** on the first publish — fails, or worse, silently attempts to create a private package requiring a paid plan.
- **Not running `npm pack --dry-run` first** — the cheapest possible check before something becomes permanently, unchangeably part of the registry.
- **Manually editing the version number in `package.json`** instead of using `npm version` — skips the Git tagging that makes it easy to find exactly what code a given published version corresponds to later.
- **Publishing uncompiled/untranspiled source** without a `prepublishOnly` build step — ships code consumers' environments may not be able to run.

## Quick summary

- `npm login`, a unique `name`, and an explicit `files` allowlist are the essentials before a first publish
- `npm pack --dry-run` shows exactly what would be published — always check before you actually run `npm publish`
- Published versions are permanent and can't be overwritten — use `npm version patch/minor/major` to bump correctly per semver
- `prepublishOnly` ensures a build step runs automatically before publishing
- Prefer `npm deprecate` over `npm unpublish` when something needs to be flagged rather than erased

## Section complete

That covers the npm ecosystem beyond the basics — versioning, dependency management, scripts, and publishing. **`05-http-web`** moves into the HTTP protocol itself: methods, status codes, headers, and more.
