# Semantic Versioning (SemVer)

A convention for version numbers that communicates, at a glance, how risky it is to update a dependency. Almost the entire npm ecosystem relies on it.

## The format: `MAJOR.MINOR.PATCH`

```
4.18.2
│  │  │
│  │  └── PATCH — bug fixes, no new features, no breaking changes
│  └───── MINOR — new features, backward compatible
└──────── MAJOR — breaking changes, may require code changes to upgrade
```

| Change                                     | Version bump               | Example                                 |
| ------------------------------------------ | -------------------------- | --------------------------------------- |
| Fix a bug, no API change                   | Patch: `4.18.2` → `4.18.3` | Fixing an off-by-one error              |
| Add a new feature, nothing removed/changed | Minor: `4.18.2` → `4.19.0` | Adding a new optional config option     |
| Remove or change existing behavior         | Major: `4.18.2` → `5.0.0`  | Renaming a function, changing a default |

This is a **convention**, not something npm enforces automatically — a package's maintainers choose how to bump the version, and (rarely, but it happens) a "minor" release accidentally contains a breaking change. Semver is a strong signal, not an absolute guarantee.

---

## Version ranges in `package.json`

```json
{
  "dependencies": {
    "express": "^4.18.2",
    "lodash": "~4.17.21",
    "some-lib": "4.0.0",
    "another-lib": "*"
  }
}
```

### Caret (`^`) — the default, and usually what you want

```
^4.18.2  →  allows 4.18.2 up to (but not including) 5.0.0
```

Permits any minor or patch update within the same major version — the idea being that minor/patch updates shouldn't break your code, per semver's own convention, so it's safe to auto-receive them.

### Tilde (`~`) — more conservative

```
~4.17.21  →  allows 4.17.21 up to (but not including) 4.18.0
```

Permits only patch updates — useful when you want bug fixes but don't trust a dependency's minor releases to truly be non-breaking, or when a project specifically pins minor versions for stability.

### Exact version — no updates at all

```
4.0.0  →  only ever exactly 4.0.0
```

No automatic updates whatsoever — you'd need to manually bump this yourself. Rare in practice, since it means missing out on bug fixes too, but occasionally used for a dependency known to have caused problems on update before.

### Wildcard (`*`) — anything goes

```
*  →  any version at all
```

Almost never a good idea for a real project — offers no protection against a breaking major update landing unexpectedly.

---

## Pre-release versions

```
5.0.0-beta.1
5.0.0-rc.2
```

`beta`/`rc` (release candidate) suffixes mark a version as not yet stable — npm's default version ranges (`^`, `~`) intentionally **exclude** pre-release versions unless you request one explicitly, so you don't accidentally get an unstable release through a normal update.

```bash
npm install express@beta       # explicitly opt into a pre-release
```

---

## `0.x.x` versions: a special case

```
0.4.2
```

By convention, a `0.x.x` version means the package is still considered unstable/in-initial-development — semver explicitly allows **any** change, including breaking ones, within `0.x` releases. `^0.4.2` therefore only permits patch updates (`0.4.2` → `0.4.9`, but not `0.5.0`), treating the _minor_ version as if it were the major version for a `0.x` package.

---

## Checking what version range you actually have installed

```bash
npm list express
```

```
myapp@1.0.0
└── express@4.18.2
```

```bash
npm outdated
```

```
Package  Current  Wanted  Latest
express   4.18.2   4.18.2   4.19.0
```

- **Current** — what's actually installed
- **Wanted** — the highest version matching your `package.json` range
- **Latest** — the newest version published at all, even if it's outside your current range

A gap between **Wanted** and **Latest** means a major version bump is available that your current range (correctly) won't auto-install — worth reviewing deliberately rather than jumping to blindly.

---

## Why this matters for `package-lock.json`

As covered in `00-setup/02-npm-and-package-json.md`, `package.json`'s version ranges describe what's _acceptable_; `package-lock.json` pins the _exact_ version actually installed and tested. Semver ranges give you controlled flexibility for routine `npm install`/`npm update` runs, while the lockfile guarantees everyone gets the identical exact tree until someone deliberately updates it.

## Common mistakes

- **Assuming a minor/patch update is always 100% safe** — semver is a strong convention, not a guarantee; a determined maintainer (or an honest mistake) can still ship a breaking change in a "minor" release.
- **Using `*` or no version constraint at all** — offers zero protection from an unexpected breaking major version landing on the next install.
- **Not distinguishing `^` from `~`** — `^` is more permissive (minor + patch); `~` is more conservative (patch only) — pick deliberately based on how much you trust a dependency's minor releases.
- **Manually bumping `package.json`'s version without updating `package-lock.json`** — always let `npm install`/`npm update` update both together, rather than hand-editing version numbers.

## Quick summary

- `MAJOR.MINOR.PATCH` — major for breaking changes, minor for new backward-compatible features, patch for bug fixes
- `^4.18.2` (default) allows minor+patch updates within the same major; `~4.17.21` allows patch updates only
- `0.x.x` versions are special-cased as unstable — semver permits breaking changes even within them
- `npm outdated` shows the gap between what you have, what your range would allow, and what's newest overall
- Semver is a convention maintainers choose to follow, not something npm enforces — a strong signal, not a guarantee

## Next

**`02-dependencies-and-scripts.md`** covers managing dependencies and npm scripts well, day to day.
