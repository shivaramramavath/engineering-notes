# Quick Reference Cheatsheet: GitHub CLI (`gh`)

[<- Back to Cheatsheets Overview](./README.md) | [Previous Cheatsheet: GitHub Actions Syntax <-](./04-github-actions-syntax.md) | [Next Cheatsheet: Git Config & Aliases ->](./06-git-config-and-aliases.md)

---

## 1. Authentication & Setup

```bash
# Interactive web browser login
gh auth login

# Headless / CI login with token via standard input
echo "$GITHUB_TOKEN" | gh auth login --with-token

# Check login status and granted scopes
gh auth status

# Add additional OAuth scopes to existing session
gh auth refresh -s repo,admin:org,workflow

# Configure default Git protocol (ssh or https)
gh config set git_protocol ssh
```

---

## 2. Pull Request Management

```bash
# Create a PR interactively
gh pr create

# Create PR using commit message for title & body
gh pr create --fill

# Create PR targeting specific base branch and labels
gh pr create --base main --title "feat: billing retry" --body "Closes #42" --label "billing"

# Checkout a coworker's PR locally
gh pr checkout 123

# View PR diff in terminal
gh pr diff 123

# Submit an approving review
gh pr review 123 --approve --body "LGTM! Tested locally."

# Request changes
gh pr review 123 --request-changes --body "Please handle division by zero."

# Merge PR using squash and delete branch
gh pr merge 123 --squash --delete-branch

# Enable auto-merge when CI passes
gh pr merge 123 --auto --squash

# Open PR directly in web browser
gh pr view 123 --web
```

---

## 3. Issue Management

```bash
# List open issues assigned to you
gh issue list --assignee "@me"

# List issues with specific label
gh issue list --label "bug" --state open

# Create an issue
gh issue create --title "Memory leak in worker" --body "Repro steps..." --label "bug"

# View issue details
gh issue view 42

# Add comment to issue
gh issue comment 42 --body "Fixed in commit 4a1b2c."

# Close issue as completed
gh issue close 42 --reason "completed"

# Close issue as not planned
gh issue close 42 --reason "not planned"
```

---

## 4. GitHub Actions Inspection

```bash
# List recent workflow runs
gh run list

# List runs for a specific workflow file
gh run list --workflow=ci.yml

# Live stream running workflow execution in terminal
gh run watch

# View logs of the failed steps only
gh run view --log-failed

# Re-run failed jobs in a workflow run
gh run rerun 123456789 --failed

# Trigger a workflow dispatch with inputs
gh workflow run deploy.yml -f environment=staging -f version=1.2.0
```

---

## 5. Releases & Repository Secrets

```bash
# Create a release and auto-generate release notes from merged PRs
gh release create v1.0.0 --title "v1.0.0" --generate-notes

# Create a release and upload binary assets
gh release create v1.0.0 dist/* --title "Release v1.0.0"

# List repository secrets
gh secret list

# Set an encrypted secret from standard input
echo "super_secret_db_pass" | gh secret set DATABASE_PASSWORD

# Set organization secret with repository access policy
gh secret set API_KEY --org my-org --visibility selected --repos repoA,repoB
```

---

## 6. Power Scripting with `--json` and `--jq`

```bash
# Extract PR numbers and titles
gh pr list --json number,title --jq '.[] | "#" + (.number|tostring) + " " + .title'

# Find failing checks on a PR
gh pr checks 123 --json name,state,bucket --jq '.[] | select(.bucket == "fail")'

# Export open bug issue numbers into an array
BUG_ISSUES=$(gh issue list --label "bug" --json number --jq '.[].number')
```

---

## 7. Productivity Aliases

```bash
# Checkout PR alias
gh alias set co 'pr checkout'

# My open PRs
gh alias set mine 'pr list --author "@me"'

# PRs waiting on my review
gh alias set reviews 'pr list --search "review-requested:@me is:open"'
```

---

[<- Back to Cheatsheets Overview](./README.md) | [Previous Cheatsheet: GitHub Actions Syntax <-](./04-github-actions-syntax.md) | [Next Cheatsheet: Git Config & Aliases ->](./06-git-config-and-aliases.md)
