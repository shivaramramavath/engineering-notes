# Artifacts, Caching, and Dependencies

Because every job starts on a fresh machine, anything you want to **keep** or **reuse** must be saved deliberately. Actions gives you three mechanisms, each for a different purpose, plus `needs` to control which jobs depend on which.

```
artifacts  →  keep or hand over the RESULTS of a run (builds, reports, logs)
cache      →  speed up FUTURE runs by reusing downloaded or computed files
outputs    →  pass small values between jobs
needs      →  order jobs and decide what waits for what
```

## The simplest artifact

```yaml
jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - run: mkdir dist && echo "hello" > dist/app.txt
      - uses: actions/upload-artifact@v4
        with:
          name: build-output
          path: dist/
```

After the run, the file is downloadable from the run's summary page under **Artifacts**.

## Artifacts

An **artifact** is a file or folder that you upload during a run, so it can be downloaded by people or by later jobs in the same run.

### Upload

```yaml
- uses: actions/upload-artifact@v4
  with:
    name: test-report                  # unique within the run
    path: |
      reports/
      !reports/**/*.tmp                # exclude with "!"
    retention-days: 14                 # optional, default comes from repo settings
    if-no-files-found: error           # warn (default), error, or ignore
```

| Input | Meaning |
|-------|---------|
| `name` | Artifact name; must be unique within a run in v4 |
| `path` | File, folder, or glob list |
| `retention-days` | How long to keep it (up to the maximum allowed by your repository or organization settings) |
| `if-no-files-found` | `warn`, `error`, or `ignore` |
| `compression-level` | `0` to `9`; use `0` for already-compressed files |
| `overwrite` | Replace an existing artifact with the same name |

### Download in a later job

```yaml
jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - run: npm ci && npm run build
      - uses: actions/upload-artifact@v4
        with:
          name: dist
          path: dist/

  deploy:
    needs: build                       # wait so the artifact exists
    runs-on: ubuntu-latest
    steps:
      - uses: actions/download-artifact@v4
        with:
          name: dist
          path: dist/                  # where to place the files
      - run: ls dist/
```

```
build job ──upload──► [artifact store] ──download──► deploy job
```

### Download several artifacts

```yaml
- uses: actions/download-artifact@v4
  with:
    pattern: results-*
    merge-multiple: true               # put all matches into the same folder
    path: all-results/
```

Used with matrix jobs, each uploading `results-${{ matrix.os }}`.

### Artifact facts

| Fact | Detail |
|------|--------|
| Scope | Visible to the whole run, downloadable from the UI or API |
| Persistence | Deleted after the retention period |
| Immutability (v4) | Once uploaded, an artifact cannot be modified; upload a new name instead |
| Size | Subject to storage quotas on your plan; check current limits |
| Security | Anyone with read access to the repository can download artifacts, so never upload secrets |
| Version | Use the same major version for upload and download actions so the formats match |

### Typical artifact contents

| Artifact | Why |
|----------|-----|
| Compiled output (`dist/`, `.jar`, binaries) | Hand to a deploy job or attach to a release |
| Test and coverage reports | Inspect after failures |
| Screenshots and traces from UI tests | Debug flaky tests |
| Logs | Post-mortem of failed runs |
| Packaged installers | Download for manual testing |

## Cache

A **cache** stores files from one run so later runs can restore them instead of recomputing or re-downloading. The classic use is package dependencies.

### Using `actions/cache`

```yaml
- uses: actions/cache@v4
  with:
    path: ~/.npm
    key: ${{ runner.os }}-npm-${{ hashFiles('**/package-lock.json') }}
    restore-keys: |
      ${{ runner.os }}-npm-
```

How it works:

1. At the **start** of the step the action looks for a cache whose key matches `key` exactly
2. If none matches, it tries each `restore-keys` prefix and uses the most recent match (a *partial* hit)
3. If it finds nothing, the step continues without a cache
4. After the job succeeds, a **post step** saves the cache, but only if there was no exact hit

```
key changes when package-lock.json changes
→ new key = miss → install runs → cache saved under the new key
```

### Checking for a hit

```yaml
- id: npm-cache
  uses: actions/cache@v4
  with:
    path: ~/.npm
    key: npm-${{ hashFiles('**/package-lock.json') }}

- if: steps.npm-cache.outputs.cache-hit != 'true'
  run: echo "No exact match; downloading dependencies"
```

### The shortcut: built-in caching in setup actions

```yaml
- uses: actions/setup-node@v4
  with:
    node-version: 20
    cache: npm                          # or yarn, pnpm

- uses: actions/setup-python@v5
  with:
    python-version: "3.12"
    cache: pip

- uses: actions/setup-java@v4
  with:
    distribution: temurin
    java-version: 21
    cache: maven                        # or gradle
```

This handles the path and key for you, and is the best starting point.

### What to cache

| Ecosystem | Cache this | Not this |
|-----------|------------|----------|
| Node (npm) | `~/.npm` | `node_modules`, since `npm ci` deletes and recreates it |
| Python | `~/.cache/pip` | Virtual environments across Python versions |
| Java | `~/.m2/repository`, `~/.gradle/caches` | Build output |
| Go | `~/go/pkg/mod`, build cache | |
| Rust | `~/.cargo/registry`, `target/` (carefully) | |
| Docker layers | Buildx cache (chapter 16) | |

### Cache keys

| Part | Purpose |
|------|---------|
| `runner.os` | Do not mix files from different operating systems |
| A tool or version name | Separate caches for different toolchains (`node20`) |
| `hashFiles('lockfile')` | New cache whenever dependencies change |

```yaml
key: ${{ runner.os }}-node${{ matrix.node }}-${{ hashFiles('**/package-lock.json') }}
```

### Cache rules and limits

| Rule | Detail |
|------|--------|
| Immutable | A cache with a given key can never be overwritten; change the key to save new contents |
| Scope | A run can restore caches from its own branch, its base branch, and the default branch, but **not** from sibling branches |
| Size | Total cache storage per repository is limited (10 GB at the time of writing) and the oldest are evicted when it is exceeded |
| Expiry | Caches not accessed for 7 days are removed |
| Fork PRs | Run in the context of the base, with their own isolated caches |
| Save only on success | By default the post step is skipped if the job failed |

Check GitHub's documentation for current numbers; limits have changed over time.

### Restore and save separately

```yaml
- uses: actions/cache/restore@v4
  with:
    path: build-cache/
    key: build-${{ github.sha }}
    restore-keys: build-

# ... build ...

- uses: actions/cache/save@v4
  if: always()                          # save even when the job fails
  with:
    path: build-cache/
    key: build-${{ github.sha }}
```

Use `restore` and `save` separately when you need to save on failure or save only from certain branches.

## Artifacts vs cache vs outputs

| | Artifacts | Cache | Job outputs |
|---|-----------|-------|-------------|
| Purpose | Keep and hand over results | Speed up future runs | Pass small values |
| Typical content | Builds, reports, logs | Dependencies, build caches | Version strings, flags, paths |
| Who uses them | Later jobs **and** people | Later runs | Later jobs |
| Lifetime | Retention period (days) | Until evicted or 7 days unused | This run only |
| Mutable | No (v4) | No (per key) | N/A |
| Size | Large files fine | Moderate | Tiny strings |
| Safe if missing? | No, deploy needs it | **Yes**, the job just runs slower | No |

Rule of thumb: if the workflow **breaks** without it, it is an artifact or output. If it only gets **slower**, it is a cache.

## Dependencies between jobs: `needs`

```yaml
jobs:
  build:
    runs-on: ubuntu-latest
    steps: [...]

  test:
    needs: build
    runs-on: ubuntu-latest
    steps: [...]

  deploy:
    needs: [build, test]
    runs-on: ubuntu-latest
    steps: [...]
```

```
build ──► test ──► deploy
  └───────────────────┘
```

A common pairing is `needs` plus artifacts: the `needs` guarantees the artifact exists before the next job tries to download it.

| Need | Combine |
|------|---------|
| Build once, deploy that exact build | `needs` + upload/download artifact |
| Share a computed version string | `needs` + job `outputs` |
| Skip deploy if tests fail | `needs` (default behavior) |
| Run notifications even after failure | `needs` + `if: always()` |

## Dependencies in the other sense: package management

Reproducible builds depend on **locked** dependency versions.

| Practice | Why |
|----------|-----|
| Commit lockfiles (`package-lock.json`, `poetry.lock`, ...) | Same versions everywhere |
| Use `npm ci`, not `npm install` | Fast, exact, fails if lock and manifest disagree |
| Hash the lockfile in the cache key | Cache refreshes exactly when dependencies change |
| Enable **Dependabot** for packages and for Actions | Automatic update pull requests |
| Review new dependencies | Supply chain attacks arrive through dependencies |
| Use the dependency review action on PRs | Flags newly introduced vulnerable packages |

```yaml
# .github/dependabot.yml
version: 2
updates:
  - package-ecosystem: "npm"
    directory: "/"
    schedule:
      interval: "weekly"
  - package-ecosystem: "github-actions"
    directory: "/"
    schedule:
      interval: "weekly"
```

## Security notes

| Risk | Mitigation |
|------|------------|
| Secrets inside artifacts | Never upload `.env` files, credentials, or key files; artifacts are readable by anyone with repo read access |
| Cache poisoning | A malicious workflow on a less-trusted branch can save a bad cache that a trusted run might restore; keep untrusted PR workflows away from caches used by release builds |
| Stale caches hiding broken installs | Occasionally run with the cache cleared to prove a clean install works |
| Trusting downloaded artifacts blindly | Treat artifacts from untrusted workflows as untrusted input |

## Mental model checklist

- Fresh machine every job means **save on purpose**
- Needed by the workflow? Artifact or output. Merely faster? Cache
- Cache keys describe **what the contents depend on**; hash the lockfile
- Caches and artifacts are immutable; new content needs a new key or name
- `needs` orders jobs; artifacts and outputs carry the data

## Common questions

| Question | Answer |
|----------|--------|
| How do I download an artifact locally? | From the run's summary page, or `gh run download <run-id>` |
| Can two matrix jobs upload the same artifact name? | Not in v4; give each a unique name (include the matrix value) |
| Why was my cache not restored? | Key changed, cache expired or evicted, or it was created on a branch your run cannot read |
| Should I cache `node_modules`? | No, cache `~/.npm` and let `npm ci` rebuild |
| How do I clear a cache? | Delete it in **Actions → Caches** or via `gh cache delete` |
| Do artifacts count toward storage? | Yes, within plan limits; set a short `retention-days` for bulky or temporary ones |
| Is the cache shared between repositories? | No, caches are per repository |
| Do I need both `cache:` in setup-node and `actions/cache`? | No, pick one |

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Expecting files to appear in the next job | Each job is a new runner | Upload and download an artifact |
| Same artifact name from every matrix job | Upload fails (v4) | Name with `${{ matrix.* }}` |
| Using a constant cache key | Never refreshes | Include `hashFiles(lockfile)` |
| Caching `node_modules` with `npm ci` | `npm ci` wipes it | Cache the npm download cache |
| Missing `needs` before downloading | Artifact does not exist yet | Add `needs: <build-job>` |
| Mixing artifact action major versions | Cannot read each other's artifacts | Use matching `@v4` for both |
| Uploading secrets or `.env` files | Anyone with repo read access can download them | Exclude with `!` patterns |
| Keeping everything for 90 days | Storage costs and clutter | Short `retention-days` for temporary artifacts |

## Try it

1. Build a two-job workflow: `build` creates `dist/app.txt` and uploads it; `deploy` downloads it and prints the file
2. Add `cache: npm` to `setup-node`, run twice, and compare install time between the first and second run
3. Replace the shortcut with a manual `actions/cache` step using `hashFiles`, then change the lockfile and observe the miss

## Key takeaways

- Fresh runners mean nothing persists unless you save it
- Artifacts keep results, caches speed up reruns, outputs carry small values
- Cache keys should include the OS and a hash of the lockfile
- Caches and artifacts are immutable; new content requires a new key or name
- Use `needs` for order, lockfiles and Dependabot for reproducible, up-to-date dependencies

**Next:** [Matrix and Concurrency](./10_matrix-and-concurrency.md)
