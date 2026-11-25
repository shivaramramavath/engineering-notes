# 03 — Dockerfile

`04_build.md` in the previous section covered the `docker build` command itself. This section covers what actually goes **inside** a Dockerfile — the instructions, how caching works, and how to keep production images small.

## In this section

| File                       | Covers                                                                                     |
| -------------------------- | ------------------------------------------------------------------------------------------ |
| `01_dockerfile-basics.md`  | The core instructions — `FROM`, `COPY`, `RUN`, `CMD`, `WORKDIR`, `EXPOSE`, `ENV`           |
| `02_layers-caching.md`     | How each instruction becomes a cached layer, and ordering a Dockerfile for faster rebuilds |
| `03_multi-stage-builds.md` | Using multiple `FROM` stages to keep the final production image small                      |
| `04_dockerignore.md`       | `.dockerignore` — keeping the build context small and builds fast                          |

## Why this deserves its own section

A working Dockerfile and a _good_ Dockerfile look almost identical but behave very differently — a badly ordered one can turn a 5-second rebuild into a 5-minute one, and a naive one can ship a 1GB image where a well-built one would ship 100MB. These files are about the difference.

## What you should be able to do after this section

- Write a Dockerfile for a typical app from scratch, using the core instructions correctly
- Order instructions so that Docker's layer cache actually speeds up your rebuilds
- Use a multi-stage build to keep build-only tools and dependencies out of your final image
- Write a `.dockerignore` that keeps your build context (and therefore your builds) fast

## Next

**`04_volumes_networking`** covers how containers persist data and talk to each other and the outside world — the two things a Dockerfile alone can't handle.
