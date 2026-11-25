# 06 — Production

Everything so far runs well on your machine. This section covers what changes when a container needs to run reliably somewhere else: secrets handling, a leaner image, and building it automatically instead of by hand.

## In this section

| File                         | Covers                                                                                                                                        |
| ---------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------- |
| `01_environments-secrets.md` | Environment variables and secrets in containers — local vs production, and why `ENV`/`.env` isn't enough for real secrets                     |
| `02_image-optimization.md`   | Shrinking a production image — recapping multi-stage builds and `.dockerignore` with a production-specific checklist, plus base image choices |
| `03_docker-in-ci.md`         | Building and pushing images automatically in a CI/CD pipeline                                                                                 |

## Why this comes last

Every earlier section assumed you were running things yourself, locally. Production introduces constraints that don't exist on a laptop: secrets can't sit in a Dockerfile or get committed to a repo, image size affects deploy speed and cost, and builds need to happen automatically and repeatably rather than by typing `docker build` by hand.

## This ties back to earlier documentation

If you've gone through the dotenv, envalid, JWT, and cookie reference files elsewhere in these docs, `01_environments-secrets.md` connects those directly to how a container actually receives its configuration — nothing here replaces that content, it's about the container-specific delivery mechanism on top of it.

## What you should be able to do after this section

- Explain why baking a secret into an image (via `ENV` or a broad `COPY .`) is unsafe, even if the image is "private"
- Pass secrets into a container the right way for your target environment
- Apply multi-stage builds and `.dockerignore` with production image size specifically in mind
- Understand, at a high level, how an image gets built and pushed automatically as part of CI/CD, rather than by hand

## Guide complete

This closes out the Docker guide. Between setup, fundamentals, commands, the Dockerfile, volumes/networking, Compose, and this production section, you have what's needed to build, run, and ship containers confidently.
