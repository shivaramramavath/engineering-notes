# Runners

A **runner** is the machine that executes a job. GitHub starts one for each job, runs the steps on it, reports the results, and (for GitHub-hosted runners) throws it away.

```
job  ──runs-on: ubuntu-latest──►  fresh VM  →  steps run  →  VM destroyed
```

## The simplest runner choice

```yaml
jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - run: uname -a
```

`runs-on` is required on every job.

## GitHub-hosted runners

Virtual machines managed by GitHub. No setup, always up to date, billed (or free) through your plan.

| Label | Operating system | Typical use |
|-------|-----------------|-------------|
| `ubuntu-latest` | Linux | Default choice for most projects |
| `ubuntu-24.04`, `ubuntu-22.04` | Linux (specific version) | Pin the OS version for reproducibility |
| `windows-latest` | Windows Server | .NET, Windows-only tooling |
| `macos-latest` | macOS | iOS and macOS builds |
| `*-arm` variants | ARM architecture | ARM-native builds; availability depends on plan and repository visibility |

The `-latest` labels **move** to newer OS versions over time. Pin a specific version when an OS upgrade could break your build. Check GitHub's runner documentation for the current label list, since it changes.

What you get:

| Property | Detail |
|----------|--------|
| Fresh machine | Every job starts clean; no leftover files |
| Pre-installed tools | Git, Docker, Node, Python, Java, Go, `gh` CLI, and many more (see the `actions/runner-images` repository) |
| Admin rights | Passwordless `sudo` on Linux and macOS |
| Network | Outbound internet access |
| Lifetime | Destroyed after the job ends |
| Size | Standard runners have a small number of vCPUs and a few GB of RAM; specifications differ for public and private repositories, so check the docs |

## Billing multipliers (private repositories)

Standard Linux, Windows, and macOS runners consume your included minutes at different rates.

| OS | Minute multiplier |
|----|-------------------|
| Linux | 1x |
| Windows | 2x |
| macOS | 10x |

Public repositories are free on standard runners. Confirm the current numbers on GitHub's billing page, as pricing changes.

Practical consequence: run most work on Linux, and use macOS and Windows only for jobs that truly need them.

## Choosing an OS per job

```yaml
jobs:
  lint:
    runs-on: ubuntu-latest        # cheap and quick
    steps: [...]

  build-ios:
    runs-on: macos-latest         # only where required
    steps: [...]
```

Combine with a matrix (chapter 10) to test on several operating systems:

```yaml
strategy:
  matrix:
    os: [ubuntu-latest, windows-latest, macos-latest]
runs-on: ${{ matrix.os }}
```

## Larger runners

Paid, bigger GitHub-hosted machines (more CPU and RAM, static IPs, GPUs on some plans) available on Team and Enterprise plans. You create them in organization settings and reference them by their **name or label**.

```yaml
runs-on: my-large-linux-runner
```

## Self-hosted runners

Machines **you** own, running GitHub's runner application and registered to your repository, organization, or enterprise.

```yaml
runs-on: [self-hosted, linux, x64]
```

A job runs on a runner that has **all** the listed labels.

| Reason to self-host | Detail |
|---------------------|--------|
| Special hardware | GPUs, ARM boards, specific CPUs |
| Private network access | Databases, internal APIs, on-prem deploy targets |
| Cost control | Heavy usage on hardware you already pay for |
| Custom software | Preinstalled licensed tools |
| Compliance | Data must stay inside your infrastructure |

### Setting one up

1. **Settings → Actions → Runners → New self-hosted runner**
2. Choose OS and architecture; GitHub shows download and `config` commands
3. Run the commands on your machine to download, register (using a short-lived token), and start the runner
4. The runner appears as **Idle** and picks up jobs matching its labels

Add custom labels for targeting:

```yaml
runs-on: [self-hosted, linux, gpu]
```

### Self-hosted vs GitHub-hosted

| | GitHub-hosted | Self-hosted |
|---|---------------|-------------|
| Setup | None | You install and register |
| Maintenance | GitHub | You (OS updates, disk, tooling) |
| Clean state each job | **Yes** | **No** unless you use ephemeral runners |
| Network | Public internet | Anything you connect it to |
| Scaling | Automatic | You build or install autoscaling |
| Cost | Minutes, or free for public repositories | Your hardware, plus operations time |
| Security risk | Low, isolated VMs | Higher, runs on your infrastructure |

### Security rules for self-hosted runners

| Rule | Why |
|------|-----|
| **Do not** use them on public repositories | Anyone can open a PR whose workflow code runs on your machine |
| Prefer **ephemeral** runners (one job, then deregister) | Prevents one job from poisoning the next |
| Isolate them from sensitive networks | A compromised job can reach everything the machine can |
| Use **runner groups** to restrict which repositories may use them | Limits blast radius |
| Keep the runner software and OS patched | Standard hygiene |
| Never run them as root or with broad credentials on disk | Jobs can read the filesystem |

### Scaling

| Option | Idea |
|--------|------|
| Several long-lived runners | Simple, but state can leak between jobs |
| Ephemeral runners created per job | Safer; managed by scripts or autoscalers |
| **Actions Runner Controller (ARC)** | Kubernetes-based autoscaling of ephemeral runners |

## Running a job inside a container

You can keep the runner but run steps inside a container image of your choice.

```yaml
jobs:
  test:
    runs-on: ubuntu-latest
    container:
      image: node:20-bookworm
      env:
        NODE_ENV: test
    steps:
      - uses: actions/checkout@v4
      - run: node --version
```

| Use it when | Detail |
|-------------|--------|
| You need an exact toolchain | The image defines OS packages and versions |
| You want the same environment locally | `docker run` the same image |

Container jobs are supported only on Linux runners.

## Targeting by labels

```yaml
runs-on: ubuntu-latest                        # a single label
runs-on: [self-hosted, linux, x64]            # all of these labels
runs-on: ${{ matrix.os }}                     # chosen by a matrix
runs-on: ${{ github.repository_owner == 'my-org' && 'self-hosted' || 'ubuntu-latest' }}   # chosen by expression
```

## Useful runner information

Inside a job, the `runner` context and default environment variables describe the machine.

```yaml
- run: |
    echo "OS: ${{ runner.os }}"
    echo "Arch: ${{ runner.arch }}"
    echo "Temp dir: ${{ runner.temp }}"
    echo "Workspace: $GITHUB_WORKSPACE"
```

| Value | Example |
|-------|---------|
| `runner.os` | `Linux`, `Windows`, `macOS` |
| `runner.arch` | `X64`, `ARM64` |
| `runner.temp` | Temporary directory, cleaned after the job |
| `runner.tool_cache` | Where `setup-*` actions cache toolchains |
| `$GITHUB_WORKSPACE` | Where your repository is checked out |

## Mental model checklist

- A runner is **one machine for one job**
- GitHub-hosted means clean, easy, billed (or free for public repositories)
- Self-hosted means your hardware, your maintenance, your security responsibility
- Matching is by **labels**
- Linux is the cheapest option; reserve Windows and macOS for jobs that require them

## Common questions

| Question | Answer |
|----------|--------|
| Can I install software on the runner? | Yes, with `run:` and `sudo`, or with setup actions; it is gone after the job |
| How do I know which tools are preinstalled? | See the `actions/runner-images` repository |
| Can two jobs share a runner? | Not at the same time; a runner executes one job at once |
| Are the files from a previous run still there? | Not on GitHub-hosted runners; on self-hosted runners they may be |
| Why is my job stuck on "Waiting for a runner"? | No runner matches the labels, all matching runners are busy, or concurrency limits apply |
| Can I choose the CPU architecture? | Yes, through labels such as ARM runners where available |
| Is `ubuntu-latest` always the newest Ubuntu? | It points to a version GitHub designates as latest, which can lag the newest release |

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Relying on `ubuntu-latest` for a fragile build | An OS upgrade can break it | Pin `ubuntu-24.04` or whichever version you tested |
| Self-hosted runner on a public repository | Strangers can run code on your machine | Use GitHub-hosted runners, or restrict who can trigger workflows |
| Assuming self-hosted runners start clean | Leftover files cause flaky results | Ephemeral runners, or cleanup steps |
| Running everything on macOS | Ten times the minute cost | Use Linux unless macOS is required |
| Typo in a label | Job waits forever | Check the runner's labels in Settings |
| No `timeout-minutes` on self-hosted jobs | A stuck job blocks the runner | Set a timeout on jobs |

## Try it

1. Create a workflow with a matrix over `ubuntu-latest`, `windows-latest`, and `macos-latest` that prints `runner.os`
2. Add a `container:` job using `node:20` and print `node --version`
3. (Optional, on a private test repository) Register a self-hosted runner on your own machine and target it with `runs-on: self-hosted`

## Key takeaways

- `runs-on` picks the machine; every job gets its own
- GitHub-hosted runners are clean, preconfigured, and disposable
- Pin an OS version when stability matters, and prefer Linux for cost
- Self-hosted runners add flexibility and responsibility; never expose them to untrusted code
- Containers let you control the environment inside a runner

**Next:** [Variables and Secrets](./07_variables-and-secrets.md)
