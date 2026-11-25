# 02 — Docker Commands

With the core concepts from `01_docker_fundamentals` in place, this section covers the actual CLI commands you'll use day to day — grouped by what you're trying to do, rather than listed alphabetically.

## In this section

| File                      | Covers                                                                           |
| ------------------------- | -------------------------------------------------------------------------------- |
| `01_run-ps-stop.md`       | The container lifecycle — `run`, `ps`, `stop`, `start`, `restart`                |
| `02_logs-exec-inspect.md` | Looking inside a running container — `logs`, `exec`, `inspect`                   |
| `03_images.md`            | Managing images — `pull`, `images`, `rmi`, `tag`                                 |
| `04_build.md`             | Building your own images with `docker build`, and how it relates to a Dockerfile |
| `05_cleanup.md`           | Housekeeping — `rm`, `rmi`, `system prune`, and avoiding disk space creep        |

## How to use this section

Read `01_run-ps-stop.md` through `03_images.md` in order the first time — each builds on the last. After that, treat this section as reference: jump to whichever file covers the command you need a refresher on.

## What you should be able to do after this section

- Start, stop, and restart containers, and list what's running (or not)
- Check a container's logs, run a command inside it, and inspect its configuration
- Pull, list, tag, and remove images
- Build your own image from a Dockerfile
- Clean up stopped containers and unused images before they quietly eat your disk space

## Next

**`03_dockerfile`** goes deeper into writing a good Dockerfile — layers, caching, and multi-stage builds — since `04_build.md` here only covers the `docker build` command itself.
