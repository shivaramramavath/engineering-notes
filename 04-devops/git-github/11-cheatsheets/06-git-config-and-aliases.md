# Quick Reference Cheatsheet: Git Configuration & Supercharged Aliases

[<- Back to Cheatsheets Overview](./README.md) | [Previous Cheatsheet: GitHub CLI <-](./05-gh-cli.md) | [Next Cheatsheet: Common Errors & Fixes ->](./07-common-errors-and-fixes.md)

---

## 1. Production `.gitconfig` Reference

Save as `~/.gitconfig`:

```ini
[user]
    name = Shiva
    email = shiva@engineering.corp
    signingkey = ~/.ssh/id_ed25519_signing.pub

[gpg]
    format = ssh

[commit]
    gpgsign = true

[tag]
    gpgsign = true

[init]
    defaultBranch = main

[core]
    editor = vim
    autocrlf = input
    filemode = true
    fsmonitor = true
    untrackedCache = true
    whitespace = trailing-space,space-before-tab

[pull]
    rebase = true

[push]
    default = current
    autoSetupRemote = true
    followTags = true

[fetch]
    prune = true
    pruneTags = true

[merge]
    conflictStyle = diff3

[rerere]
    enabled = true
    autoUpdate = true

[diff]
    colorMoved = default
    algorithm = histogram

[status]
    short = true
    branch = true
```

---

## 2. Conditional Configurations (`includeIf`)

Separate personal GitHub projects from corporate enterprise work:

```ini
# Inside ~/.gitconfig

# Default: Personal
[include]
    path = ~/.gitconfig-personal

# If repository path matches corporate workspace directory, override identity
[includeIf "gitdir:~/work/"]
    path = ~/.gitconfig-work
```

`~/.gitconfig-work`:
```ini
[user]
    name = Shiva (Work)
    email = shiva@corporate.com
    signingkey = ~/.ssh/id_work_signing.pub
```

---

## 3. Top 20 Productivity Aliases

Add to the `[alias]` section of `~/.gitconfig`:

```ini
[alias]
    # Visual graph logs
    lg = log --graph --pretty=format:'%Cred%h%Creset -%C(yellow)%d%Creset %s %Cgreen(%cr) %C(bold blue)<%an>%Creset' --abbrev-commit --date=relative
    lga = log --graph --pretty=format:'%Cred%h%Creset -%C(yellow)%d%Creset %s %Cgreen(%cr) %C(bold blue)<%an>%Creset' --abbrev-commit --date=relative --all

    # Status & Branches
    s = status -s -b
    b = branch -vv
    ba = branch -a

    # Fast checkout / switch
    co = checkout
    sw = switch
    main = switch main

    # Staging & Unstaging
    unstage = restore --staged
    discard = restore
    amend = commit --amend --no-edit

    # Quick commits
    cm = commit -m
    wip = commit -am "wip: save state"

    # Diffs
    df = diff
    dfs = diff --staged
    last = log -1 HEAD --stat

    # Cleaning & Stashing
    save = stash push -u -m
    pop = stash pop

    # Discarding and Resetting
    undo = reset --soft HEAD~1
    nuke = reset --hard HEAD

    # Upstream sync
    sync = !git fetch --all --prune && git rebase origin/main
    prune-branches = !git branch -vv | grep ': gone]' | awk '{print $1}' | xargs git branch -D
```

---

[<- Back to Cheatsheets Overview](./README.md) | [Previous Cheatsheet: GitHub CLI <-](./05-gh-cli.md) | [Next Cheatsheet: Common Errors & Fixes ->](./07-common-errors-and-fixes.md)
