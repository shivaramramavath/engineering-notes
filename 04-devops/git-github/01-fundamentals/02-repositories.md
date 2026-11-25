# Repositories

## What is a repository?

A **repository** (or "repo") is a project folder that Git is tracking. The moment you run `git init` inside a folder, that folder becomes a repository — Git starts watching it for changes and can save snapshots of its history.

```bash
mkdir my-project
cd my-project
git init
```

That's it. `my-project` is now a Git repository.

## What actually makes it a repository: the `.git` folder

`git init` creates one thing: a hidden folder called `.git` inside your project.

```
my-project/
├── .git/          ← this is the actual repository
├── src/
├── README.md
└── package.json
```

**The `.git` folder _is_ the repository.** Everything else in the project folder — your source code, README, etc. — is just the working files. All of Git's history, configuration, branches, and commit data live inside `.git`.

If you delete `.git`, you lose the entire history — the project files remain, but they're no longer version controlled.

### A peek inside `.git`

```
.git/
├── HEAD           # pointer to the current branch/commit
├── config         # repo-specific configuration
├── objects/       # every commit, file, and folder snapshot ever made
├── refs/
│   ├── heads/     # local branches
│   └── tags/
└── index          # the staging area
```

You'll rarely edit these files directly, but knowing they exist demystifies what commands like `git commit` or `git branch` are actually doing — they're reading and writing files in here.

## Local vs remote repositories

|            | Local repository                   | Remote repository                                         |
| ---------- | ---------------------------------- | --------------------------------------------------------- |
| Where      | Your machine                       | A server (GitHub, GitLab, etc.)                           |
| Created by | `git init` or `git clone`          | Hosted by a Git service, or `git init --bare` on a server |
| Purpose    | Where you actually work and commit | Where you push to share/back up your work                 |

A repository doesn't need a remote at all — plenty of local-only repos exist for personal projects, notes, or experiments that are never pushed anywhere.

## Creating a repository

There are two ways to end up with a local repository:

### 1. Start fresh with `git init`

```bash
mkdir my-project
cd my-project
git init
```

Use this when you're starting a brand-new project with no existing history to copy.

### 2. Copy an existing one with `git clone`

```bash
git clone https://github.com/user/repo.git
```

This downloads the entire repository — full history included — from a remote source into a new folder on your machine. Use this when the project already exists somewhere (usually on GitHub/GitLab) and you want a working copy.

## Checking repository status

```bash
git status
```

Tells you what branch you're on, what's staged, what's modified, and what's untracked. This is the command you'll run more than any other while working in a repository.

## Bare repositories (a quick mention)

A **bare repository** has no working files — only the `.git` contents, without a checked-out copy of the project. These are used purely as a shared point to push/pull from, typically on a server, since nobody edits files directly in a bare repo:

```bash
git init --bare
```

You won't create one of these often as a beginner, but it's why a GitHub repository doesn't have a "working tree" the way your local clone does.

## Quick summary

- A repository is any folder containing a `.git` subfolder
- The `.git` folder holds the _entire_ history — delete it and version control is gone, even though your files remain
- `git init` creates a new repository; `git clone` copies an existing one
- A repository can exist purely locally, with no remote at all

## Next

**`03_working-tree-staging-area.md`** covers the three states a file moves through as you go from editing to committing.
