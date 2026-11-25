# `.dockerignore`

A file that tells Docker which files and folders to exclude from the **build context** — the set of files sent to the daemon before a build even starts. It works just like `.gitignore`, but for `docker build` instead of `git`.

## Why it matters

Recall from `02_docker_commands/04_build.md`: the trailing `.` in `docker build -t myapp .` means the entire contents of that folder are sent to the Docker daemon before the build begins — not just the files your Dockerfile actually `COPY`s.

```bash
docker build -t myapp .
```

```
Sending build context to Docker daemon  1.2GB
```

Without a `.dockerignore`, that includes things like `node_modules`, `.git`, log files, and local `.env` files — none of which the build usually needs, and some of which (`.env`, `.git`) you actively don't want inside an image.

---

## A typical `.dockerignore`

```gitignore
node_modules
npm-debug.log
.git
.gitignore
.env
.env.*
dist
build
coverage
*.md
.vscode
.DS_Store
Dockerfile
.dockerignore
```

Placed at the root of the build context, next to your Dockerfile.

---

## The three big wins

### 1. Faster builds

A smaller build context means less to send to the daemon before the build even starts — the "Sending build context..." step shrinks dramatically.

### 2. Smaller, cleaner images

If your Dockerfile ever does something broad like `COPY . .`, excluding `node_modules` in `.dockerignore` prevents your local (possibly platform-specific, possibly stale) `node_modules` from being copied in and clashing with the `RUN npm install` your Dockerfile actually does.

### 3. Avoiding leaked secrets

This is the security-relevant one: without `.dockerignore` excluding `.env` (and similarly sensitive files), a broad `COPY . .` bakes your local secrets directly into the image layer — and once in a layer, they're recoverable by anyone who can pull or inspect that image, even if a _later_ layer deletes the file. Excluding them from the build context in the first place means they're never copied in at all.

```gitignore
.env
.env.*
*.pem
*.key
```

---

## Pattern syntax

Mostly the same conventions as `.gitignore`:

```gitignore
node_modules        # exact name, anywhere in the tree
*.log                # wildcard
.env*                # matches .env, .env.local, .env.production, etc.
temp/                 # a specific folder
!temp/keep-this.txt   # negation — re-include something inside an excluded folder
```

Negation (`!`) can only re-include a file whose parent directory wasn't itself excluded — you can't un-ignore something inside a folder that's fully excluded above it.

---

## Checking what's actually being sent

If you're unsure whether `.dockerignore` is working as expected, watch the reported context size:

```bash
docker build -t myapp .
```

```
Sending build context to Docker daemon  4.2MB
```

A sudden jump in that number after adding a new large folder to your project is usually a sign `.dockerignore` needs an update.

---

## `.dockerignore` vs `.gitignore`

They serve similar purposes but are **not the same file** and aren't automatically synced — Docker doesn't read `.gitignore` at all. If a folder should be excluded from both Git and Docker builds (which is common, e.g. `node_modules`), it needs an entry in both files.

## Quick summary

- `.dockerignore` excludes files from the build context sent to the Docker daemon, same syntax family as `.gitignore`
- Always exclude `node_modules`, `.git`, and any `.env`/secret files — the last one matters for security, not just size
- A smaller context means faster builds and a smaller risk of accidentally copying something (stale dependencies, secrets) into the final image
- It's a separate file from `.gitignore` — maintain both if you need the same exclusions in each

## Section complete

That covers writing a good Dockerfile — instructions, caching, multi-stage builds, and a clean build context. **`04_volumes_networking`** covers what a Dockerfile alone can't: persisting data across container restarts, and how containers talk to each other and the outside world.
