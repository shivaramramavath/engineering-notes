# Getting Started with Git

Everything you need to go from "Git isn't installed" to "I made my first commit."

---

## 1. Install Git

### Windows

Download and run the installer from [git-scm.com](https://git-scm.com/download/win). Accepting the defaults is fine for almost everyone. This also installs **Git Bash**, a Unix-style terminal that's a comfortable place to run the commands in this guide.

### macOS

If you have Homebrew:

```bash
brew install git
```

Or install the Xcode Command Line Tools, which include Git:

```bash
xcode-select --install
```

### Linux

```bash
# Debian/Ubuntu
sudo apt update && sudo apt install git

# Fedora
sudo dnf install git

# Arch
sudo pacman -S git
```

### Verify the install

```bash
git --version
```

You should see something like `git version 2.4x.x`. If the command isn't found, restart your terminal (or your machine) and try again — the installer usually needs a fresh shell to update your `PATH`.

---

## 2. Configure your identity

Every commit records who made it. Set this once per machine:

```bash
git config --global user.name "Your Name"
git config --global user.email "you@example.com"
```

Use the same email as your GitHub/GitLab account if you want commits linked to your profile there.

### Check your configuration

```bash
git config --global user.name
git config --global user.email
```

Or see everything at once:

```bash
git config --list
```

### A few optional but useful defaults

```bash
# Use a consistent default branch name for new repos
git config --global init.defaultBranch main

# Pick your editor for commit messages (example: VS Code)
git config --global core.editor "code --wait"

# Nicer colored output
git config --global color.ui auto
```

`--global` applies the setting to every repository on your machine. Drop `--global` (and run the command inside a specific repo) to override it for just that project.

---

## 3. Your first commands

With Git installed and configured, create a repository and make your first commit.

### Initialize a repository

```bash
mkdir my-project
cd my-project
git init
```

This creates a hidden `.git` folder — that's the entire repository. Nothing is tracked yet.

### Check status

```bash
git status
```

This is the command you'll run constantly. Right now it should say you're on your initial branch with no commits yet.

### Create and stage a file

```bash
echo "# My Project" > README.md
git add README.md
```

`git add` moves a file into the **staging area** — the set of changes you're about to commit.

### Commit it

```bash
git commit -m "Initial commit"
```

This saves a permanent snapshot of the staged changes, along with the message, your name/email, and a timestamp.

### Confirm it worked

```bash
git log
```

You should see your one commit, with its author, date, and message.

---

## You're set up when you can:

- [ ] Run `git --version` and see a version number
- [ ] Run `git config --global user.name` and `user.email` and see your values
- [ ] Run `git init`, `git add`, and `git commit` in a folder without errors
- [ ] See your commit with `git log`

## Next

Head to **`01_git_fundamentals`** to understand what a repository, the staging area, and a commit actually _are_ under the hood before learning more commands.
