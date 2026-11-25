# Core Concepts

A **workflow** is an automated process defined in a YAML file inside your repository. It runs when an **event** happens, and it is made of **jobs**, which are made of **steps**, which execute on a **runner**.

```
event  →  workflow  →  job(s)  →  step(s)  →  action or shell command
                          └─ each job runs on a runner (a fresh machine)
```

## The simplest workflow

```yaml
# .github/workflows/ci.yml
name: CI                         # label shown in the Actions tab

on: push                         # event that triggers it

jobs:
  test:                          # job id
    runs-on: ubuntu-latest       # runner
    steps:
      - uses: actions/checkout@v4          # step 1: an action
      - run: echo "Running tests..."       # step 2: a shell command
```

Every push to the repository now starts one run of this workflow, containing one job, containing two steps.

## The building blocks

| Term | What it is | Analogy |
|------|-----------|---------|
| **Event** | Something that happens: a push, a PR, a schedule, a manual click | The doorbell |
| **Workflow** | A YAML file describing what to do in response to events | The instructions |
| **Job** | A group of steps that run together on one runner | One worker's task list |
| **Step** | A single command (`run`) or reusable action (`uses`) | One item on the list |
| **Action** | A reusable unit of automation, such as `actions/checkout` | A power tool |
| **Runner** | The machine (VM or container host) that executes a job | The workshop |
| **Run** | One execution of a workflow | One day's work |

## How it works

1. An **event** occurs in your repository (for example, a push)
2. GitHub looks in `.github/workflows/` for files whose `on:` matches that event
3. For each match, GitHub creates a **workflow run**
4. Each **job** is queued and assigned to a **runner**
5. The runner executes the job's **steps** in order
6. Results appear in the **Actions** tab and as status checks on commits and pull requests

```
push ──► ci.yml ──► run #42
                      ├─ job: lint  ──► runner A  (parallel)
                      └─ job: test  ──► runner B  (parallel)
```

## Where workflow files live

```
your-repo/
└── .github/
    └── workflows/
        ├── ci.yml
        ├── deploy.yml
        └── nightly.yaml
```

| Rule | Detail |
|------|--------|
| Folder | Exactly `.github/workflows/` at the repository root |
| Extension | `.yml` or `.yaml` |
| Subfolders | Not scanned; files must be directly inside `workflows/` |
| Quantity | As many workflows as you like, each independent |
| Version | The workflow file is read from the commit the event ran against, so changing it on a branch affects that branch |

## Jobs: parallel by default, isolated always

```yaml
jobs:
  lint:
    runs-on: ubuntu-latest
    steps: [...]
  test:
    runs-on: ubuntu-latest
    steps: [...]
```

- `lint` and `test` start at the same time on **two separate machines**
- Files created in one job are **not visible** in the other
- To order them, use `needs:` (chapter 04); to share files, use artifacts (chapter 09)

## Steps: two kinds

```yaml
steps:
  - uses: actions/setup-node@v4     # 1. reusable action
    with:
      node-version: 20

  - run: npm test                   # 2. shell command
```

| Kind | Keyword | Use it for |
|------|---------|-----------|
| Action step | `uses:` | Reusable logic: checkout, set up a language, upload files |
| Command step | `run:` | Anything you could type in a terminal |

## Runners

A **runner** is a fresh machine created for each job and discarded afterwards.

| Type | Who manages it | Typical use |
|------|----------------|-------------|
| GitHub-hosted (`ubuntu-latest`, `windows-latest`, `macos-latest`) | GitHub | Most projects |
| Self-hosted | You | Special hardware, private networks, cost control |

Because the machine is fresh every time, **nothing persists between runs** unless you cache it or store it as an artifact. Details in chapters 06 and 09.

## Seeing results

| Where | What you see |
|-------|--------------|
| **Actions** tab | Every run, grouped by workflow |
| A run's page | Jobs, steps, timing, and live logs |
| Pull request **Checks** section | Pass or fail status for each job |
| Commit status marks | Green check or red cross next to the commit |
| Email or notification | Failure alerts, depending on your settings |

## What Actions can do

Actions is not only for CI. Anything that reacts to repository events works.

| Category | Examples |
|----------|----------|
| CI | Build, lint, test on every PR |
| CD | Deploy to a server, cloud, or registry |
| Repository chores | Label issues, close stale PRs, greet new contributors |
| Scheduled jobs | Nightly builds, dependency checks |
| Releases | Tag, build artifacts, publish a GitHub Release |

## Mental model checklist

- **What triggers it?** Look at `on:`
- **What runs?** Look at `jobs:`
- **Where does it run?** Look at `runs-on:`
- **In what order?** Parallel unless `needs:` says otherwise
- **What carries over?** Nothing, unless you pass outputs, artifacts, or cache

## Common questions

| Question | Answer |
|----------|--------|
| Do I need to install anything? | No, workflows run on GitHub's servers |
| Does it cost money? | Free for public repositories on standard runners; private repositories get an included allowance. Check GitHub's current pricing page for exact numbers |
| Can one repository have many workflows? | Yes, they run independently |
| Is a workflow the same as a job? | No, a workflow contains one or more jobs |
| Do steps in the same job share files? | Yes, same machine, same filesystem |
| Do jobs share files? | No, each job gets a fresh runner |
| Can I run workflows locally? | Third-party tools such as `act` approximate it; behavior is not identical |
| What language are workflows written in? | YAML, with `${{ }}` expressions |

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Workflow file in the wrong folder | It never runs and no error is shown | Use `.github/workflows/` exactly |
| Forgetting `actions/checkout` | The runner has no copy of your code | Add it as the first step |
| Expecting jobs to share files | Each job is a new machine | Use artifacts or a single job |
| Assuming jobs run in file order | They run in parallel | Add `needs:` |
| Editing the workflow only on a feature branch and expecting `main` to change | The file is read from the commit that triggered the run | Merge the change |

## Try it

1. Create `.github/workflows/concepts.yml` with two jobs, `one` and `two`, each echoing its name
2. Push and open the Actions tab; note that both jobs start together
3. Add `needs: one` to job `two` and push again; note the new order in the graph

## Key takeaways

- Event, workflow, job, step is the hierarchy
- Workflows are YAML files in `.github/workflows/`
- Jobs run in parallel on separate fresh machines unless ordered with `needs`
- Steps are either `uses` (actions) or `run` (commands)
- Nothing persists between jobs or runs unless you save it deliberately

**Next:** [Workflow Syntax](./02_workflow-syntax.md)
