# Custom Actions

A **custom action** is an action you write yourself, described by an `action.yml` file. Once written, you can call it with `uses:` just like `actions/checkout`. There are three kinds: **composite** (a bundle of steps), **JavaScript** (Node.js code), and **Docker** (a container).

```
action.yml  →  declares inputs, outputs, and HOW it runs
              ├─ composite   →  a list of steps
              ├─ javascript  →  a Node.js file
              └─ docker      →  a container image
```

## The simplest custom action

`.github/actions/hello/action.yml`:

```yaml
name: Hello
description: Print a greeting
inputs:
  who:
    description: Whom to greet
    default: world
runs:
  using: composite
  steps:
    - run: echo "Hello, ${{ inputs.who }}"
      shell: bash
```

Use it in a workflow:

```yaml
steps:
  - uses: actions/checkout@v4              # required so ./ paths exist
  - uses: ./.github/actions/hello
    with:
      who: Actions
```

## Where an action lives

| Location | How you call it | Use for |
|----------|-----------------|---------|
| `.github/actions/<name>/action.yml` in your repo | `uses: ./.github/actions/<name>` | Actions private to one repository |
| Root of its own repository | `uses: owner/repo@v1` | Sharing across repositories or publishing |
| A subfolder of a repository | `uses: owner/repo/path/to/action@v1` | Several actions in one repository |

Local actions need `actions/checkout` to run first, since the folder only exists after checkout.

## The metadata file: `action.yml`

```yaml
name: "My Action"                  # required
description: "What it does"        # required
author: "Your Name"

inputs:
  token:
    description: "API token"
    required: true
  mode:
    description: "Run mode"
    required: false
    default: "fast"

outputs:
  result:
    description: "What the action produced"
    # composite actions also need a value: here

runs:                              # required; type-specific
  using: composite

branding:                          # optional, used by the Marketplace
  icon: "check"
  color: "green"
```

| Key | Notes |
|-----|-------|
| `inputs` | Always arrive as **strings**; convert in your code |
| `outputs` | Values the caller reads through `steps.<id>.outputs.<name>` |
| `runs.using` | `composite`, a Node runtime (such as `node20`), or `docker` |
| `branding` | Icon and color for Marketplace listings |

File name must be `action.yml` or `action.yaml`.

## Kind 1: Composite actions

A composite action is a **sequence of steps** packaged as one reusable step.

```yaml
# .github/actions/setup-project/action.yml
name: Set up project
description: Install Node and dependencies with caching

inputs:
  node-version:
    description: Node.js version
    default: "20"

outputs:
  cache-hit:
    description: Whether the npm cache was restored
    value: ${{ steps.node.outputs.cache-hit }}

runs:
  using: composite
  steps:
    - id: node
      uses: actions/setup-node@v4
      with:
        node-version: ${{ inputs.node-version }}
        cache: npm

    - run: npm ci
      shell: bash

    - run: echo "Installed for Node ${{ inputs.node-version }}"
      shell: bash
```

Used like this:

```yaml
- uses: actions/checkout@v4
- uses: ./.github/actions/setup-project
  with:
    node-version: "22"
- run: npm test
```

### Composite rules

| Rule | Detail |
|------|--------|
| Every `run` step needs `shell:` | `bash`, `pwsh`, `python`, and so on |
| Outputs need a `value:` | Usually mapped from an inner step |
| Inputs are read via `${{ inputs.name }}` | |
| Steps can call other actions | `uses:` is allowed inside |
| Same runner, same job | Shares the caller's filesystem and environment |
| No direct `secrets` context | Pass secrets in as inputs |
| Files inside the action | Reference with `${{ github.action_path }}` |

```yaml
- run: ${{ github.action_path }}/scripts/check.sh
  shell: bash
```

### When composite is the right choice

You repeat the same 3 to 10 steps across workflows and want to name them once. No programming language needed.

## Kind 2: JavaScript actions

JavaScript actions run Node.js code **directly on the runner**, so they start fast and work on Linux, Windows, and macOS.

### Files

```
my-js-action/
├── action.yml
├── package.json
├── index.js
└── dist/
    └── index.js        ← bundled output that is committed
```

### `action.yml`

```yaml
name: Greeter
description: Greets someone and sets an output
inputs:
  who:
    description: Whom to greet
    required: true
outputs:
  greeting:
    description: The generated greeting
runs:
  using: node20                 # use a Node runtime that GitHub currently supports
  main: dist/index.js
```

Check GitHub's documentation for the currently supported `node` versions, since the runtime is updated periodically.

### `index.js`

```js
const core = require("@actions/core");
const github = require("@actions/github");

async function run() {
  try {
    const who = core.getInput("who", { required: true });
    const greeting = `Hello, ${who}!`;

    core.info(greeting);                       // log line
    core.setOutput("greeting", greeting);      // output
    core.notice(`Event: ${github.context.eventName}`);   // annotation
  } catch (error) {
    core.setFailed(error.message);             // marks the step as failed
  }
}

run();
```

### Build and bundle

```bash
npm init -y
npm install @actions/core @actions/github
npm install --save-dev @vercel/ncc
npx ncc build index.js -o dist
git add dist && git commit -m "Build action"
```

The runner does **not** run `npm install` for an action. Bundling all dependencies into `dist/index.js` with `ncc` is the standard approach, and the `dist/` folder must be committed.

### Useful toolkit packages

| Package | Provides |
|---------|----------|
| `@actions/core` | Inputs, outputs, logging, `setFailed`, secrets masking |
| `@actions/github` | Authenticated Octokit client and the event `context` |
| `@actions/exec` | Run command-line programs |
| `@actions/io` | File and directory helpers |
| `@actions/cache` | Cache API from code |
| `@actions/artifact` | Artifact API from code |
| `@actions/tool-cache` | Download and cache tools |

### Pre and post hooks

```yaml
runs:
  using: node20
  pre: dist/setup.js        # before the job's steps
  main: dist/index.js
  post: dist/cleanup.js     # after the job (runs even on failure by default)
```

Useful for cleanup, such as stopping a background service or saving a cache.

### When JavaScript is the right choice

You need logic, API calls, or parsing that is awkward in shell, and you want fast startup and cross-platform support.

## Kind 3: Docker container actions

A Docker action packages the tool and its environment into an image. The runner builds or pulls the image and runs the container.

### Files

```
my-docker-action/
├── action.yml
├── Dockerfile
└── entrypoint.sh
```

### `action.yml`

```yaml
name: Container greeter
description: Runs inside a container
inputs:
  who:
    description: Whom to greet
    default: world
outputs:
  greeting:
    description: The greeting
runs:
  using: docker
  image: Dockerfile             # build locally, or: docker://ghcr.io/org/image:1.0
  args:
    - ${{ inputs.who }}
```

### `Dockerfile`

```dockerfile
FROM alpine:3.20
COPY entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh
ENTRYPOINT ["/entrypoint.sh"]
```

### `entrypoint.sh`

```bash
#!/bin/sh -l
echo "Hello, $1"
echo "greeting=Hello, $1" >> "$GITHUB_OUTPUT"
```

Inputs are also available as environment variables named `INPUT_<NAME>` in uppercase (`INPUT_WHO`).

### Docker action rules

| Rule | Detail |
|------|--------|
| Linux runners only | Not available on Windows or macOS runners |
| Slower start | The image must be built or pulled first |
| Use `docker://` image reference | Skips the build if you publish the image to a registry |
| Workspace is mounted | `GITHUB_WORKSPACE` is available at `/github/workspace` |
| Runs as a separate container | Does not see the host's installed tools |

### When Docker is the right choice

You need a precise toolchain or language that is not on the runner, and you accept slower startup and Linux-only use.

## Choosing the type

| | Composite | JavaScript | Docker |
|---|-----------|-----------|--------|
| Written in | YAML + shell | JavaScript | Any language |
| Startup speed | Fast | **Fastest** | Slowest |
| OS support | Any | Any | Linux only |
| Needs a build step | No | Yes (bundle `dist/`) | Yes (image) |
| Complexity | Lowest | Medium | Medium to high |
| Best for | Bundling steps | API logic, parsing, portable tools | Custom environments |

Start with **composite**. Move to JavaScript or Docker only when you need what they provide.

## Outputs and logging from any action type

| Mechanism | How | Works in |
|-----------|-----|----------|
| Set an output | Append `name=value` to `$GITHUB_OUTPUT` | Composite (shell), Docker |
| Set an output | `core.setOutput("name", value)` | JavaScript |
| Fail the step | `exit 1` or `core.setFailed(msg)` | All |
| Add a log annotation | `echo "::warning::message"` or `core.warning()` | All |
| Mask a value | `echo "::add-mask::$VALUE"` or `core.setSecret()` | All |

More on workflow commands in chapter 18.

## Testing your action

Put a workflow in the same repository that uses the action with a **local path**.

```yaml
name: Test action
on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - id: run
        uses: ./                       # action.yml at the repo root
        with:
          who: tester
      - run: test "${{ steps.run.outputs.greeting }}" = "Hello, tester!"
```

## Versioning and releasing

1. Commit your action, including `dist/` for JavaScript actions
2. Tag a release: `git tag -a v1.0.0 -m "First release"`
3. Maintain a **moving major tag** so users can write `@v1`:

```bash
git tag -fa v1 -m "Update v1 to v1.0.1"
git push origin v1 --force
```

| Tag | Meaning |
|-----|---------|
| `v1.0.1` | Exact release; never moves |
| `v1` | Moves to the latest `v1.x.y` release |

Follow semantic versioning: breaking changes only in a new major version.

## Publishing to the Marketplace

1. The repository must be **public** and contain an `action.yml` at its root
2. The action `name` must be unique on the Marketplace
3. Create a **release** and tick **Publish this Action to the GitHub Marketplace**
4. Choose categories and add a README with inputs, outputs, and a usage example
5. Accept the Marketplace developer agreement on first publish

You do not need the Marketplace to share an action; any repository that users can access works.

## Writing a good action

| Practice | Why |
|----------|-----|
| Clear `description` on every input and output | Shows up in docs and editors |
| Sensible `default` values | Fewer required inputs |
| Fail loudly with a helpful message | Easier debugging |
| Do not log secrets or full inputs | Logs can be public |
| Pass inputs to shell via `env`, not interpolated into scripts | Avoids injection |
| Keep it single-purpose | Easier to reuse and test |
| Document in the README with a copy-paste example | Adoption |
| Pin dependencies and commit lockfiles | Reproducible builds |

## Mental model checklist

- `action.yml` is the contract: inputs, outputs, and how to run
- Composite = steps, JavaScript = fast portable code, Docker = full control on Linux
- Inputs are strings; outputs are set through `$GITHUB_OUTPUT` or `core.setOutput`
- Local actions need checkout; published actions need tags
- Start small and simple

## Common questions

| Question | Answer |
|----------|--------|
| Do I need to publish to the Marketplace to use my action elsewhere? | No, a repository reference like `owner/repo@v1` works |
| Can a private repository host an action? | Yes, with settings that allow other private repositories in the organization to use it |
| Why did my JavaScript action fail with "Cannot find module"? | `dist/` was not rebuilt or committed, so bundle with `ncc` |
| How do I read an input in a composite action's shell? | `${{ inputs.name }}`, or pass it through `env:` for safety |
| Can a composite action use secrets? | Not directly; accept a secret as an input |
| Can an action have multiple steps in JavaScript? | One `main`, optional `pre` and `post`; do the sequence in code |
| What is `github.action_path`? | The directory where the action's files live on the runner |
| How do I debug an action? | Add `core.debug()` or `echo "::debug::..."` and enable debug logging (chapter 18) |

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Missing `shell:` in a composite `run` step | Workflow fails validation | Add `shell: bash` |
| Forgetting `checkout` before a local action | Folder not found | Check out first |
| Not committing `dist/` | JavaScript action cannot load dependencies | Build with `ncc` and commit |
| Using `${{ inputs.x }}` directly inside a shell script | Script injection if the input is untrusted | `env: X: ${{ inputs.x }}` then `"$X"` |
| Moving a tag without telling anyone | Consumers get surprise changes | Follow semver; only move the major tag for compatible releases |
| Docker action on a Windows runner | Not supported | Use JavaScript or composite |
| Expecting booleans and numbers as real types | Inputs are strings | Compare with `== 'true'` or parse |
| Large Docker images | Slow starts | Use small base images or prebuilt `docker://` images |

## Try it

1. Create a composite action in `.github/actions/setup-project` that sets up Node and runs `npm ci`; call it from a workflow
2. Add an output to it and print the output in a later step
3. Write a tiny JavaScript action that reads an input, calls `core.setOutput`, and bundle it with `ncc`

## Key takeaways

- A custom action is defined by `action.yml` and called with `uses:`
- Composite bundles steps, JavaScript is fast and portable, Docker gives a custom environment (Linux only)
- Inputs are strings; outputs go through `$GITHUB_OUTPUT` or `core.setOutput`
- Commit `dist/` for JavaScript actions and tag releases with a moving major tag
- Start with composite and graduate only if you need more

**Next:** [CI Pipelines](./14_ci-pipelines.md)
