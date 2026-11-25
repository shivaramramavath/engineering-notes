---
title: "Advanced CLI Formatting with Go Templates: Custom Tables, JSON, and Automation"
description: "Mastering the Docker CLI Go template formatting engine: syntax grammar, built-in functions, conditionals, looping, custom tabular formatting, and JSON extraction."
category: "DevOps / Containers"
tags: ["docker", "go-templates", "formatting", "cli", "automation", "scripting", "inspect"]
difficulty: "Intermediate"
prerequisites: ["02-commands/01-run-ps-stop.md", "02-commands/02-logs-exec-inspect.md", "Basic JSON and templating familiarity"]
estimated_read_time: "25 min"
---

# Advanced CLI Formatting with Go Templates: Custom Tables, JSON, and Automation

## Table of Contents
- [1. What It Is & Why It Matters](#1-what-it-is--why-it-matters)
- [2. The Go Template Engine Syntax & Grammar](#2-the-go-template-engine-syntax--grammar)
- [3. Built-In Template Functions in Docker](#3-built-in-template-functions-in-docker)
- [4. Control Structures: Conditionals and Loops](#4-control-structures-conditionals-and-loops)
- [5. Tabular Output Formatting (`table \t`)](#5-tabular-output-formatting-table-t)
- [6. High-Yield Formatting Recipes Across Subcommands](#6-high-yield-formatting-recipes-across-subcommands)
- [7. Practical Diagnostic Demonstrations](#7-practical-diagnostic-demonstrations)
- [8. Best Practices & Production Guidelines](#8-best-practices--production-guidelines)
- [9. Common Pitfalls & Anti-Patterns](#9-common-pitfalls--anti-patterns)
- [10. Technical Interview Questions](#10-technical-interview-questions)
- [11. Hands-On Practice Exercises](#11-hands-on-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading & References](#13-further-reading--references)

---

## 1. What It Is & Why It Matters

When writing shell automation, CI/CD deployment verification scripts, or local developer aliases, parsing raw Docker CLI output with standard Unix text utilities (`awk`, `sed`, `cut`, `grep`) is notoriously brittle:
- Adding or removing columns in a new Docker version breaks column index positions in `awk '{print $1}'`.
- Long container names or image tags wrap onto multiple lines or get truncated by default column widths.
- Nested data structures (such as port mappings, volume bindings, and network IP addresses) cannot be cleanly extracted with regular expressions.

To provide deterministic, machine-readable output across all commands, Docker embeds the **Go `text/template` engine** directly into the CLI via the **`--format`** flag.

With Go templates, you can extract exact properties directly from the daemon's internal data model, execute conditional logic, format dynamic tables, and output pure JSON payloads for direct consumption by `jq`, Python, or configuration management tools.

```
+---------------------------------------------------------------------------------+
|                       GO TEMPLATE EVALUATION PIPELINE                           |
|                                                                                 |
|  [Docker CLI Subcommand]                                                        |
|  docker inspect / docker ps / docker images / docker volume ls                  |
|        |                                                                        |
|        v                                                                        |
|  [Internal Data Model (Go Structs / JSON)]                                      |
|  {                                                                              |
|    "Id": "3e7389ab...",                                                         |
|    "State": { "Running": true, "OOMKilled": false, "Pid": 18492 },              |
|    "NetworkSettings": { "IPAddress": "172.17.0.2" }                             |
|  }                                                                              |
|        |                                                                        |
|        v (Evaluated through Go text/template engine)                            |
|  --format '{{.State.Pid}} (IP: {{.NetworkSettings.IPAddress}})'                 |
|        |                                                                        |
|        v                                                                        |
|  [Clean Output: 18492 (IP: 172.17.0.2)]                                         |
|  (No awk, no sed, 100% deterministic, zero parsing bugs!)                       |
+---------------------------------------------------------------------------------+
```

---

## 2. The Go Template Engine Syntax & Grammar

Every template expression is enclosed within double curly braces: `{{ ... }}`.

### 2.1. The Dot (`.`) Operator
The dot represents the **current data context**:
- `{{.}}`: The entire current data object.
- `{{.ID}}`: Accesses the top-level `ID` field.
- `{{.State.Running}}`: Traverses nested structures (accessing `Running` inside `State`).

### 2.2. Whitespace Trimming
By default, whitespace (spaces, tabs, newlines) outside curly braces is printed literally. To strip leading or trailing whitespace:
- `{{- ...}}`: Trims whitespace to the left.
- `{{... -}}`: Trims whitespace to the right.

---

## 3. Built-In Template Functions in Docker

Docker augments standard Go templating with several specialized helper functions:

```
+---------------------------------------------------------------------------------+
|                       BUILT-IN TEMPLATE FUNCTIONS                               |
+-----------+-------------------------+-------------------------------------------+
| FUNCTION  | SYNTAX                  | DESCRIPTION                               |
+-----------+-------------------------+-------------------------------------------+
| `json`    | `{{json .Field}}`       | Encodes the object or field as valid JSON.|
+-----------+-------------------------+-------------------------------------------+
| `println` | `{{println .Field}}`    | Prints value followed by a newline.       |
+-----------+-------------------------+-------------------------------------------+
| `join`    | `{{join .Slice ","}}`   | Concatenates a string slice with delimiter|
+-----------+-------------------------+-------------------------------------------+
| `lower`   | `{{lower .String}}`     | Converts string to lowercase.             |
+-----------+-------------------------+-------------------------------------------+
| `upper`   | `{{upper .String}}`     | Converts string to uppercase.             |
+-----------+-------------------------+-------------------------------------------+
| `title`   | `{{title .String}}`     | Title-cases words in string.              |
+-----------+-------------------------+-------------------------------------------+
| `split`   | `{{split .String ":"}}` | Splits string into a slice by separator.  |
+-----------+-------------------------+-------------------------------------------+
```

---

## 4. Control Structures: Conditionals and Loops

### 4.1. Conditionals (`if / else / end`)
Execute conditional rendering based on boolean properties or presence of values:

```bash
# Print container name and alert if OOM killed
docker ps -a --format '{{.Names}}: {{if .State.OOMKilled}}CRASHED BY OOM!{{else}}Normal{{end}}'

# Print only containers that are NOT running
docker ps -a --format '{{if not .State.Running}}{{.Names}} is stopped{{end}}'
```

### 4.2. Looping (`range / end`)
Iterate over slices, arrays, and maps:

```bash
# Print all environment variables for a container
docker inspect --format '{{range .Config.Env}}{{println .}}{{end}}' <container>

# Iterate over all network names and assigned IP addresses
docker inspect --format '{{range $net, $conf := .NetworkSettings.Networks}}{{$net}}: {{$conf.IPAddress}}{{println}}{{end}}' <container>
```

### 4.3. Context Scoping (`with / end`)
Temporarily binds the dot context (`.`) to a sub-field:

```bash
docker inspect --format '{{with .State}}Status: {{.Status}}, PID: {{.Pid}}{{end}}' <container>
```

---

## 5. Tabular Output Formatting (`table \t`)

When formatting output for human consumption, you can instruct Docker to render aligned tabular columns by starting the format string with `table` followed by tab characters (`\t`):

```bash
docker ps --format "table {{.ID}}\t{{.Names}}\t{{.Status}}\t{{.Image}}"
```
*Output:*
```text
CONTAINER ID   NAMES         STATUS         IMAGE
3e7389ab294e   web-gateway   Up 4 hours     nginx:alpine
d74b102941fa   auth-api      Up 2 days      node:20-alpine
```
Docker automatically calculates column widths, formats header labels based on field names, and aligns data cleanly!

---

## 6. High-Yield Formatting Recipes Across Subcommands

### 6.1. `docker ps` Recipes
```bash
# Extract only Container Names
docker ps --format '{{.Names}}'

# Tabular view with human-readable published ports and labels
docker ps --format "table {{.Names}}\t{{.Status}}\t{{.Ports}}\t{{.Labels}}"

# List stopped containers and their exit codes
docker ps -a --format '{{if ne .Status "running"}}{{.Names}}: Exit Code {{.State}}{{end}}'
```

### 6.2. `docker images` Recipes
```bash
# Clean table of Repository, Tag, Image ID, and Size
docker images --format "table {{.Repository}}\t{{.Tag}}\t{{.ID}}\t{{.Size}}"

# Output only full 64-character Image IDs (no truncation)
docker images --format '{{.ID}}' --no-trunc
```

### 6.3. `docker inspect` Recipes
```bash
# Extract container IPv4 address directly
docker inspect --format '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' <container>

# Extract host port mapped to container port 80
docker inspect --format '{{(index (index .NetworkSettings.Ports "80/tcp") 0).HostPort}}' <container>

# Extract host source directory of all volume mounts
docker inspect --format '{{range .Mounts}}{{.Source}} -> {{.Destination}}{{println}}{{end}}' <container>

# Extract container log file path on host disk
docker inspect --format '{{.LogPath}}' <container>

# Extract memory limit in bytes
docker inspect --format '{{.HostConfig.Memory}}' <container>
```

### 6.4. `docker volume ls` and `docker network ls` Recipes
```bash
# List volumes and their driver
docker volume ls --format "table {{.Name}}\t{{.Driver}}\t{{.Labels}}"

# List networks with subnet scope
docker network ls --format "table {{.Name}}\t{{.Driver}}\t{{.Scope}}"
```

---

## 7. Practical Diagnostic Demonstrations

### Example 1: Direct JSON Extraction for `jq` Processing
Extract the entire `.NetworkSettings` sub-tree as valid JSON and pipe into `jq` or Python:

```bash
# Start container
docker run -d --name fmt-demo -p 8089:80 nginx:alpine

# Extract clean JSON
docker inspect --format '{{json .NetworkSettings}}' fmt-demo | python3 -m json.tool | head -n 20
```

### Example 2: Extracting Dynamically Allocated Random Ports
When running containers with `-p 80` (Docker allocates a random high port on host), query the exact host port without guessing:

```bash
# Launch container with dynamic port allocation
docker run -d --name dynamic-port-app -p 80 nginx:alpine

# Extract host port dynamically via Go template
ASSIGNED_PORT=$(docker inspect --format '{{(index (index .NetworkSettings.Ports "80/tcp") 0).HostPort}}' dynamic-port-app)
echo "Service is listening on Host Port: $ASSIGNED_PORT"

# Verify HTTP connectivity
curl -I http://localhost:$ASSIGNED_PORT

# Cleanup
docker rm -f dynamic-port-app
```

### Example 3: Auditing Restart Policies Across All Running Containers
Generate a clean audit report of restart configurations:

```bash
docker ps --format "table {{.Names}}\t{{.Image}}\t{{.RestartCount}}"
```

### Example 4: Extracting Environment Variables into Key-Value Files
Extract all environment variables configured inside a container directly into a `.env` format:

```bash
docker run -d --name env-extract-lab -e DB_HOST=db.internal -e DB_PORT=5432 alpine sleep 100

# Format environment variables directly to file
docker inspect --format '{{range .Config.Env}}{{println .}}{{end}}' env-extract-lab > ./extracted.env
cat ./extracted.env

# Cleanup
docker rm -f env-extract-lab && rm -f ./extracted.env
```

### Example 5: Iterating Over Mount Bindings
Inspect volume mount modes (Read-Only vs Read-Write):

```bash
docker run -d --name mount-audit-lab -v /tmp:/data:ro -v /var/log:/logs:rw alpine sleep 100

docker inspect --format '{{range .Mounts}}Mount: {{.Destination}} | Host: {{.Source}} | Mode: {{.Mode}} | RW: {{.RW}}{{println}}{{end}}' mount-audit-lab

# Cleanup
docker rm -f mount-audit-lab fmt-demo
```

---

## 8. Best Practices & Production Guidelines

1. **Always Use `--format` in Shell Scripts**: Never parse standard table output with `awk '{print $1}'`. Column positions and header names vary between Docker CLI versions. Go template formatting is stable and backwards-compatible.
2. **Use `{{json .}}` for External Script Ingestion**: When passing Docker state to Python, Node.js, or jq scripts, output `{{json .}}` to produce RFC8259-compliant JSON rather than building manual string key-value pairs.
3. **Guard Against Nil Index Lookups**: When indexing nested maps (like `.NetworkSettings.Ports`), verify the port is exposed or handle nil references carefully to prevent template rendering errors.
4. **Configure CLI Default Formatting in `~/.docker/config.json`**: You can set your personal preferred default output format globally across all CLI commands without typing `--format` every time:
   ```json
   {
     "psFormat": "table {{.Names}}\t{{.Image}}\t{{.Status}}\t{{.Ports}}",
     "imagesFormat": "table {{.Repository}}\t{{.Tag}}\t{{.Size}}"
   }
   ```
5. **Combine `range` and `println` for Clean Multi-Line Iteration**: When iterating over slices, always invoke `{{println .}}` inside the loop to ensure clean line breaks.

---

## 9. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Root Cause | Impact | Proper Solution |
|:---|:---|:---|:---|
| **Using `awk '{print $1}'` to extract Container IDs** | Relying on column splitting. | Fails when columns wrap or when table headers change; extracts wrong strings. | Use `docker ps -q` or `docker ps --format '{{.ID}}'`. |
| **Invalid indexing in `.Ports` map** | Attempting `.Ports.80/tcp` directly. | Go template syntax error due to slash `/` and map typing. | Use index function: `{{index .NetworkSettings.Ports "80/tcp"}}`. |
| **Missing `table` keyword when tabular output is desired** | Writing `--format '{{.ID}}\t{{.Names}}'`. | Output does not align columns; tabs render with inconsistent spacing. | Prepend `table`: `--format 'table {{.ID}}\t{{.Names}}'`. |
| **Iterating maps without variables** | Running `{{range .Networks}}{{.}}{{end}}`. | Returns raw struct dump rather than specific keys and values. | Use key-value syntax: `{{range $k, $v := .Networks}}{{$k}}: {{$v.IPAddress}}{{end}}`. |
| **Template rendering error on empty slices** | Accessing `index 0` on an empty array. | Template evaluation aborts with `index out of range`. | Guard with `if`: `{{if .Mounts}}{{index .Mounts 0}}{{end}}`. |
| **Unescaped quotes in bash one-liners** | Double quotes conflicting with template braces in bash. | Bash attempts parameter expansion on `$var` inside template. | Use single quotes: `--format '{{...}}'`. |

---

## 10. Technical Interview Questions

<details>
<summary><strong>1. Why is using <code>--format</code> with Go templates superior to piping Docker output to <code>awk</code> or <code>cut</code>?</strong></summary>

Piping CLI output to `awk` or `cut` relies on positional column parsing of whitespace-separated text. This is fragile because:
1. Column positions shift if new columns are added in newer Docker CLI releases.
2. Long container names or repository paths containing spaces or wrapping lines corrupt column indexes.
3. Positional text parsing cannot access nested object properties (such as specific port bindings or cgroup memory limits).
`--format` with Go templates accesses the engine's internal data structures directly, providing deterministic, strongly-typed, and backward-compatible extraction.
</details>

<details>
<summary><strong>2. How do you query the host port mapped to container port <code>80/tcp</code> using <code>docker inspect --format</code>?</strong></summary>

Because port maps use keys containing slashes (e.g., `"80/tcp"`), dot notation cannot be used directly. The Go template `index` function must be used:
```bash
docker inspect --format '{{(index (index .NetworkSettings.Ports "80/tcp") 0).HostPort}}' <container>
```
The first `index` extracts the array of port bindings for `"80/tcp"`, the second `index` accesses array element `0`, and `.HostPort` extracts the host port string.
</details>

<details>
<summary><strong>3. What is the purpose of prepending <code>table</code> to a <code>--format</code> template?</strong></summary>

The `table` keyword instructs the Docker CLI's output printer to use a tab-writer (`tabwriter.Writer`). It automatically parses the tab-separated (`\t`) values, measures the maximum length of items in each column, and aligns data with dynamic padding. Furthermore, Docker automatically generates clean uppercase table headers matching the field names.
</details>

<details>
<summary><strong>4. How can you output valid JSON for each running container using <code>docker ps</code>?</strong></summary>

By passing the `json` built-in function to `--format`:
```bash
docker ps --format '{{json .}}'
```
This produces one valid JSON object per line for every active container, containing all container attributes formatted as standard key-value pairs.
</details>

<details>
<summary><strong>5. How does Go template whitespace trimming (<code>{{-</code> and <code>-}}</code>) work?</strong></summary>

- `{{- ...}}`: Trims all whitespace (spaces, tabs, newlines) immediately preceding the opening template marker.
- `{{... -}}`: Trims all whitespace immediately following the closing template marker.
This is essential when building single-line comma-separated outputs inside loops without introducing unwanted trailing newlines.
</details>

<details>
<summary><strong>6. How can an administrator configure default CLI output formatting globally across all terminal sessions?</strong></summary>

By adding configuration keys to `~/.docker/config.json`:
```json
{
  "psFormat": "table {{.Names}}\t{{.Image}}\t{{.Status}}\t{{.Ports}}",
  "imagesFormat": "table {{.Repository}}\t{{.Tag}}\t{{.Size}}"
}
```
The Docker CLI evaluates these keys and applies the specified template format automatically to all subsequent `docker ps` and `docker images` commands without requiring the `--format` flag.
</details>

<details>
<summary><strong>7. How do you conditionally display output in Go templates based on a boolean flag?</strong></summary>

Using the `{{if ...}} ... {{else}} ... {{end}}` control structure:
```bash
docker inspect --format '{{if .State.Running}}ACTIVE{{else}}STOPPED{{end}}' <container>
```
To invert logic, use the `not` boolean operator: `{{if not .State.Running}}...{{end}}`.
</details>

<details>
<summary><strong>8. How can you iterate over all user-defined networks attached to a container and print their IP addresses?</strong></summary>

Using the `range` control structure with key-value variable assignment:
```bash
docker inspect --format '{{range $net, $conf := .NetworkSettings.Networks}}{{$net}}: {{$conf.IPAddress}} {{end}}' <container>
```
This iterates over the `.NetworkSettings.Networks` map, assigning the network name to `$net` and the network configuration struct to `$conf`.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Format Clean Container Name and Image Listing</strong></summary>

**Goal**: Print container names and image references without extra clutter.

```bash
docker ps --format 'Container: {{.Names}} (Base: {{.Image}})'
```
</details>

<details>
<summary><strong>Exercise 2: Query IP Address of All Active Containers in One Line</strong></summary>

**Goal**: Output `<Name> -> <IP>` pairs for all running containers.

```bash
docker ps --format '{{.Names}} -> {{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}'
```
</details>

<details>
<summary><strong>Exercise 3: Test Dynamic JSON Formatting with `jq`</strong></summary>

**Goal**: Extract container ID and process state as structured JSON.

```bash
# Start container
docker run -d --name json-fmt-lab alpine sleep 60

# Extract JSON and query with python
docker inspect --format '{{json .State}}' json-fmt-lab | python3 -c 'import sys, json; data=json.load(sys.stdin); print("Status:", data["Status"], "| PID:", data["Pid"])'

# Cleanup
docker rm -f json-fmt-lab
```
</details>

<details>
<summary><strong>Exercise 4: Print Table of Volume Mounts with Permissions</strong></summary>

**Goal**: Render an aligned table of container volume destinations and read-write flags.

```bash
docker run -d --name vol-fmt-lab -v /tmp:/app:ro alpine sleep 60
docker inspect --format 'table {{range .Mounts}}{{.Destination}}\t{{.Source}}\t{{.RW}}{{println}}{{end}}' vol-fmt-lab
docker rm -f vol-fmt-lab
```
</details>

<details>
<summary><strong>Exercise 5: Conditional Extraction of Exited Containers</strong></summary>

**Goal**: Print only containers that terminated with non-zero exit codes.

```bash
# Create one successful and one failing container
docker run --name ok-demo alpine true
docker run --name fail-demo alpine false

# Filter via Go template
docker ps -a --format '{{if ne .State.ExitCode 0}}FAILED: {{.Names}} (Code {{.State.ExitCode}}){{end}}'

# Cleanup
docker rm ok-demo fail-demo
```
</details>

<details>
<summary><strong>Exercise 6: Test Custom Tabular Output on `docker images`</strong></summary>

**Goal**: Display image repository, tag, creation age, and size with custom headers.

```bash
docker images --format "table {{.Repository}}\t{{.Tag}}\t{{.CreatedSince}}\t{{.Size}}"
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------------------------------------------------+
|                         GO TEMPLATE FORMATTING CHEAT SHEET                            |
+---------------------------------------------------------------------------------------+
| Extract Field:         {{.Field}} / {{.Nested.Field}}                                 |
| Extract JSON:          {{json .Field}}                                                |
| Custom Table:          docker ps --format "table {{.ID}}\t{{.Names}}\t{{.Status}}"    |
| Extract IP:            docker inspect --format                                        |
|                        '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}' <id>|
| Extract Host Port:     docker inspect --format                                        |
|                        '{{(index (index .NetworkSettings.Ports "80/tcp") 0).HostPort}}'|
| Conditionals:          {{if .State.Running}}RUNNING{{else}}STOPPED{{end}}             |
| Iteration:             {{range .Config.Env}}{{println .}}{{end}}                      |
| Golden Rule:           Never use awk/sed to parse Docker CLI; always use --format!    |
+---------------------------------------------------------------------------------------+
```

---

## 13. Further Reading & References

- [Docker Documentation: Format Command and Log Output](https://docs.docker.com/config/formatting/)
- [Go Language Documentation: Package text/template Reference](https://pkg.go.dev/text/template)
- [Docker CLI Command Reference: docker inspect](https://docs.docker.com/reference/cli/docker/inspect/)
- [Docker Documentation: Configure the Docker CLI with config.json](https://docs.docker.com/reference/cli/docker/#configuration-files)

---

| [< Previous: 09-docker-events-stats-and-top.md](./09-docker-events-stats-and-top.md) | [Next: Module 03 Dockerfile Deep Dive >](../03-dockerfile/README.md) |
| :--- | :--- |
