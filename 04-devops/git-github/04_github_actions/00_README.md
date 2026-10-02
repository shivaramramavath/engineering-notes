# GitHub Actions

**GitHub Actions** is GitHub's built-in automation platform. You describe what should happen in a YAML file, tell GitHub **when** it should happen, and GitHub runs it on a machine for you.

```
event (push, PR, schedule...)  →  workflow (YAML file)  →  jobs  →  steps  →  result
```

This section takes you from "what is a workflow?" to building secure CI/CD pipelines for real projects.

## Prerequisites

| You should know | Where to learn it |
|-----------------|-------------------|
| Commits, branches, merging | `01_git_fundamentals/`, `02_git_commands/` |
| Pushing to and pulling from a remote | `02_git_commands/08_remotes.md` |
| Basic shell commands (`cd`, `echo`, `ls`) | Any terminal tutorial |
| YAML basics (indentation, lists, key-value pairs) | Refreshed in `02_workflow-syntax.md` |
| A GitHub account and a repository you can experiment in | Free account is enough |

## Learning path

| # | Chapter | What you learn | Level |
|---|---------|----------------|-------|
| 01 | Core Concepts | Events, workflows, jobs, steps, runners | Beginner |
| 02 | Workflow Syntax | YAML structure and every top-level key | Beginner |
| 03 | Events and Triggers | `push`, `pull_request`, `schedule`, `workflow_dispatch`, filters | Beginner |
| 04 | Jobs and Steps | Parallel vs sequential, `needs`, passing outputs | Beginner |
| 05 | Actions and Marketplace | Using and pinning third-party actions | Beginner |
| 06 | Runners | GitHub-hosted vs self-hosted | Beginner |
| 07 | Variables and Secrets | `env`, `vars`, `secrets`, scopes | Intermediate |
| 08 | Contexts, Expressions, Conditions | `${{ }}`, `if:`, status functions | Intermediate |
| 09 | Artifacts, Caching, Dependencies | Sharing and reusing data | Intermediate |
| 10 | Matrix and Concurrency | Many jobs from one definition | Intermediate |
| 11 | Permissions and Environments | `GITHUB_TOKEN`, approvals, protection rules | Intermediate |
| 12 | Reusable Workflows | `workflow_call` | Intermediate |
| 13 | Custom Actions | Composite, JavaScript, Docker | Advanced |
| 14 | CI Pipelines | Build, lint, test (Node.js example) | Intermediate |
| 15 | CD and Deployment | Release and deploy flows | Advanced |
| 16 | Docker CI/CD | Buildx, GHCR, layer caching | Advanced |
| 17 | Security Hardening | OIDC, script injection, pinning | Advanced |
| 18 | Debugging and Workflow Commands | Debug logs, annotations, step summaries | Intermediate |

Complete workflows referenced by the chapters live in `examples/`. A one-page reference lives in `CHEATSHEET.md`.

## How to use this section

1. Read the chapters **in order**; later ones assume earlier ones
2. Create a throwaway repository to practice in
3. Type the examples yourself instead of copy-pasting; YAML mistakes teach you a lot
4. Do the **Try it** exercise at the end of each chapter
5. Use `CHEATSHEET.md` once the basics feel familiar

## Your first workflow in 60 seconds

Create `.github/workflows/hello.yml` in any repository:

```yaml
name: Hello
on: push

jobs:
  greet:
    runs-on: ubuntu-latest
    steps:
      - run: echo "Hello from GitHub Actions"
```

Commit and push, then open the repository's **Actions** tab. You will see a run named "Hello" with a green check.

## Chapter layout

Every chapter follows the same shape:

1. **Definition** and a minimal example
2. **How it works**
3. **Syntax and variations**
4. **Common questions**
5. **Pitfalls**
6. **Key takeaways**
7. **Try it** and a link to the next chapter

## Conventions used

| Convention | Meaning |
|------------|---------|
| `ubuntu-latest` | Used in examples because it is the most common runner |
| `actions/checkout@v4` | Major-version tags for readability; chapter 17 explains why production code should pin to a SHA |
| `# comment` in YAML | Explains the line above or beside it |
| `$VAR` vs `${{ }}` | Shell variable vs Actions expression; chapter 08 explains the difference |

## Key takeaways

- Actions runs YAML-defined automation in response to repository events
- Workflows live in `.github/workflows/` inside your repository
- The path is: event, workflow, job, step
- You can start learning with a free account and one small repository

**Next:** [Core Concepts](./01_core-concepts.md)
