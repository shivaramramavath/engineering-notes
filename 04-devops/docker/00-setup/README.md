# 00 — Setup

Everything you need before running your first container: installing Docker and confirming it actually works.

## In this section

| File                 | Covers                                                                                                                |
| -------------------- | --------------------------------------------------------------------------------------------------------------------- |
| `getting-started.md` | Installing Docker Desktop/Engine, verifying the install, and running your first container to confirm everything works |

## Why this comes first

Every later section assumes Docker is installed and running. Docker Desktop (or the Docker daemon on Linux) needs to actually be running in the background before any `docker` command will work — this section gets you from "nothing installed" to "ran a container successfully."

## What you should be able to do after this section

- Run `docker --version` and see a version number
- Confirm the Docker daemon is running (`docker info` doesn't error out)
- Run `docker run hello-world` and see the success message

## Next

Once setup is confirmed, move on to **`01_docker_fundamentals`** to understand what an image and a container actually are before running more commands.
