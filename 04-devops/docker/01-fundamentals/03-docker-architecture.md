# Docker Architecture

When you type `docker run nginx`, several distinct pieces cooperate to make that happen. Understanding them clears up why some commands need network access and others don't, and what's actually running on your machine.

```
You  ──►  Docker CLI (client)  ──►  Docker daemon (dockerd)  ──►  containers
                                            │
                                            ▼
                                  Registry (e.g. Docker Hub)
```

## The three main pieces

### 1. Docker CLI (client)

The `docker` command you type. It doesn't do the actual work itself — it sends your command as an API request to the daemon and displays the response.

```bash
docker run nginx
docker ps
docker build -t myapp .
```

### 2. Docker daemon (`dockerd`)

A background service that does the real work: building images, running containers, managing networks and volumes, and talking to registries. It runs constantly in the background — on Linux directly, and on Windows/macOS inside Docker Desktop's Linux VM (see `01_docker-vs-vm.md`).

```bash
docker info
```

Shows daemon-level information — if this fails with a connection error, the daemon isn't running, and no other Docker command will work either.

The CLI and daemon can even run on different machines — this is how tools like `docker context` let you run commands against a remote Docker host as if it were local.

### 3. Registry

A server that stores and distributes images. **Docker Hub** is the default public registry, but private registries exist too (AWS ECR, GitHub Container Registry, self-hosted).

```bash
docker pull nginx          # downloads from Docker Hub by default
docker push myuser/myapp   # uploads to a registry (requires docker login first)
```

---

## Following a command through the architecture

`docker run nginx`, step by step:

1. **CLI** sends a "run container from image `nginx`" request to the **daemon**
2. **Daemon** checks if it has the `nginx` image locally
3. If not, the daemon contacts the **registry** (Docker Hub) and pulls it
4. **Daemon** creates a container from the image, using the Linux kernel's namespaces/cgroups for isolation
5. **Daemon** starts the container's main process
6. **CLI** displays the result (or, without `-d`, stays attached and streams output back)

```
docker run nginx
    │
    ▼
CLI → daemon: "start a container from nginx"
    │
    ▼
daemon: image cached locally? ──no──► pull from registry
    │yes
    ▼
daemon creates + starts the container
    │
    ▼
CLI shows the result
```

---

## Why some commands need internet, others don't

| Command                                       | Needs the registry (internet)?                             |
| --------------------------------------------- | ---------------------------------------------------------- |
| `docker pull <image>`                         | Yes — downloads from a registry                            |
| `docker run <image>` (already cached locally) | No                                                         |
| `docker run <image>` (not cached)             | Yes — pulls first, then runs                               |
| `docker ps`, `docker stop`, `docker logs`     | No — all local, daemon-only operations                     |
| `docker build`                                | Only if the Dockerfile's `FROM` image isn't already cached |
| `docker push`                                 | Yes — uploads to a registry                                |

---

## Client-server, not just a CLI wrapper

Because the CLI talks to the daemon over an API (a Unix socket locally, or TCP for remote setups), Docker Desktop's GUI, VS Code's Docker extension, and third-party tools all work by calling the **same daemon API** — the CLI is just one of several possible clients. This is also what makes `docker context` (pointing your CLI at a remote daemon) work: the CLI doesn't care whether the daemon is on your laptop or a server across the network, since it's just making API calls either way.

## Quick summary

- The **CLI** is a thin client — it sends your commands to the daemon and displays results
- The **daemon** (`dockerd`) does the actual work: building, running, networking, talking to registries
- A **registry** (Docker Hub by default) is where images are stored and shared via `pull`/`push`
- Purely local operations (`ps`, `stop`, `logs`) never touch the network; anything involving an image not yet cached locally does

## Section complete

You now have the conceptual foundation — containers vs VMs, images vs containers, and how the pieces talk to each other. Move on to **`02_docker_commands`** to start using the Docker CLI in practice.
