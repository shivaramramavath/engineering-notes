# `git init` & `git clone`

Both commands give you a local repository to work in — the difference is whether you're starting brand new or copying something that already exists.

```
git init   →  start a repository from nothing
git clone  →  copy an existing repository (with full history)
```

---

## `git init`

Turns the current folder into a Git repository by creating a `.git` subfolder.

```bash
mkdir my-project
cd my-project
git init
```

```
Initialized empty Git repository in /path/to/my-project/.git/
```

At this point nothing is tracked yet — `init` just sets up the machinery. You still need `git add` and `git commit` to start actually recording history.

### Initializing in an existing folder

You don't need an empty folder — `git init` works fine in a directory that already has files:

```bash
cd existing-project
git init
```

Existing files become **untracked** — Git sees them but isn't recording their history until you `git add` them.

### Setting the initial branch name

```bash
git init -b main
```

Or set it once, globally, so every new repo uses it by default:

```bash
git config --global init.defaultBranch main
```

### Bare repositories

```bash
git init --bare
```

Creates a repository with no working files — just the `.git` contents. Used as a shared push/pull target, typically on a server; you won't create one of these for everyday project work.

---

## `git clone`

Downloads an existing repository — **including its entire history** — into a new folder on your machine.

```bash
git clone https://github.com/user/repo.git
```

This creates a folder named `repo/` (matching the repository name), with:

- Every commit, branch, and tag from the original
- A remote called `origin` already pointing back to the source
- The default branch already checked out and ready to work in

### Cloning into a specific folder name

```bash
git clone https://github.com/user/repo.git my-folder-name
```

### Cloning a specific branch

```bash
git clone -b develop https://github.com/user/repo.git
```

### Shallow clone (partial history)

For a very large repository where you don't need full history:

```bash
git clone --depth 1 https://github.com/user/repo.git
```

Only the most recent commit is downloaded. Useful for CI pipelines or quick one-off checkouts; not ideal if you plan to browse history or contribute long-term.

### Cloning over SSH vs HTTPS

```bash
# HTTPS — works everywhere, may prompt for credentials each time
git clone https://github.com/user/repo.git

# SSH — requires an SSH key set up with the host, no password prompts after that
git clone git@github.com:user/repo.git
```

---

## `init` vs `clone`: when to use which

| Situation                                                                    | Command                          |
| ---------------------------------------------------------------------------- | -------------------------------- |
| Starting a brand-new project with no existing code                           | `git init`                       |
| The project already exists on GitHub/GitLab/etc. and you want a working copy | `git clone`                      |
| You have an existing folder of files you want to start tracking              | `git init` inside that folder    |
| You want to contribute to someone else's project                             | `git clone` (or clone your fork) |

## Checking what you have

```bash
git status
```

Right after `init`: no commits, everything untracked.
Right after `clone`: fully checked out, matching the remote's default branch, nothing to commit.

## Quick summary

- `git init` — creates a new, empty repository from the current folder
- `git clone <url>` — copies an existing repository, full history included, with `origin` already configured
- Cloning is how you get a working copy of a project that's already hosted somewhere
- `init` is how every repository — including the one you eventually clone from — begins

## Next

**`02_status-diff-log.md`** covers the commands you'll use to inspect what's going on in a repository you've just created or cloned.
