# 01 — Docker Fundamentals

Before learning more commands, it helps to understand what Docker is actually doing — what a container is, what an image is, and how the pieces talk to each other. This section covers the core concepts, not the commands themselves.

## In this section

| File                        | Covers                                                                                            |
| --------------------------- | ------------------------------------------------------------------------------------------------- |
| `01_docker-vs-vm.md`        | How containers differ from virtual machines, and why that difference is the whole point of Docker |
| `02_images-containers.md`   | The core distinction: an image is a blueprint, a container is a running instance of it            |
| `03_docker-architecture.md` | The daemon, the CLI client, and registries — how a `docker` command actually gets executed        |

## Why this comes before commands

Commands like `run`, `build`, and `pull` are just operations on these underlying concepts. Understanding what an image _is_ before typing `docker build` means the command makes sense the first time, instead of feeling like a magic incantation.

## What you should understand after this section

- Why containers are lighter weight than virtual machines, and what they do (and don't) isolate
- That an image is immutable and read-only, while a container is a running, writable instance of one
- That the `docker` command you type is a client talking to a background daemon, which is what actually runs containers and talks to registries like Docker Hub

## Next

Move on to **`02_docker_commands`** to start using these concepts through the actual Docker CLI.
