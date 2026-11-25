# Environment Variables & Secrets in Docker

Configuration and secrets need to reach a running container somehow. This file covers the right and wrong ways to do that, and how it differs between local development and production.

```
Local dev    →  .env file + env_file / -e, same as running the app without Docker
Production   →  the orchestrator's secret mechanism — never baked into the image
```

---

## The wrong way: baking secrets into the image

```dockerfile
# ❌ never do this
ENV DATABASE_PASSWORD=supersecret123
```

or

```dockerfile
# ❌ also unsafe
COPY .env .env
```

Both of these put the secret **inside an image layer**, permanently. This matters more than it might seem:

- Anyone who can `docker pull` or `docker inspect` the image can extract the value — including from a layer that a _later_ instruction supposedly "removes," since removal in a later layer doesn't erase it from the earlier one (see `03_dockerfile/02_layers-caching.md` — layers stack, they don't overwrite history)
- If the image is ever pushed to any registry, even a "private" one, the secret is now wherever that registry (and its backups, and anyone with access) is
- Rotating the secret means rebuilding and redistributing the image, instead of just updating a config value

```bash
docker history myapp --no-trunc
```

This shows every layer's exact command — a secret baked in via `ENV` or `RUN echo ... > file` is visible right there in the image's own history.

---

## Local development: environment variables

For local work, containers can receive configuration the same way any app does — via `.env` files and environment variables, exactly as covered in the dotenv/envalid reference docs.

```yaml
# docker-compose.yml
services:
  api:
    build: .
    env_file:
      - .env
```

or, without Compose:

```bash
docker run --env-file .env myapp
```

Inside the container, the app reads `process.env` completely normally — Docker (or Compose) is just the mechanism that gets the `.env` file's contents there. The app code doesn't need to know or care that it's running in a container at all.

```bash
docker run -e DATABASE_URL=postgresql://localhost/dev myapp
```

Individual `-e` flags work too, for one-off overrides.

### Why this is fine for local dev but not production

Local `.env` files are already excluded from Git (per the dotenv reference), and passing them at `run`/`up` time keeps them out of the image itself — the risk of baking them into a layer doesn't apply here, since they're supplied at container-start time, not build time. The remaining risk in production is different: production environments have real-world consequences if a secret leaks, and typically involve automated deployment where there's no human typing `--env-file` by hand anyway.

---

## Production: use the platform's secret mechanism

Once you're deploying (not just running locally), the right approach depends on where the container runs — but the principle is the same everywhere: **secrets are injected at runtime by the platform, never baked into the image or committed anywhere.**

### Docker Swarm secrets

```bash
echo "supersecret123" | docker secret create db_password -
```

```yaml
services:
  api:
    image: myapp
    secrets:
      - db_password

secrets:
  db_password:
    external: true
```

The secret is mounted as a file (`/run/secrets/db_password`) inside the container, not as an environment variable — reducing the risk of it leaking via `docker inspect`, crash dumps, or child-process environment inheritance.

### Kubernetes Secrets

```bash
kubectl create secret generic db-password --from-literal=password=supersecret123
```

Referenced in a pod spec as either an environment variable or a mounted file, managed by the cluster rather than the image.

### Cloud provider secret managers

- AWS Secrets Manager / Parameter Store
- Google Secret Manager
- Azure Key Vault
- HashiCorp Vault

These are typically fetched by the application at startup (using the provider's SDK) or injected by the deployment platform, rather than passed as a plain environment variable at all.

---

## A practical middle ground: environment variables from a secure source

Many production setups still ultimately deliver secrets as environment variables — the improvement over local dev isn't necessarily _how_ the app receives them, but _where they come from_:

```
Local:       .env file, committed nowhere, read by dotenv
Production:  secret manager → injected as env vars by the platform at deploy time → same process.env the app already reads
```

This is why the earlier `envalid` reference doc's validation approach (`cleanEnv`) works identically in both environments — the app doesn't need separate logic for "how do I get my config," only for "what do I do once I have it."

---

## Checklist

- [ ] No secret ever appears in a `Dockerfile` via `ENV`, `ARG` (build args can leak into image history too), or a `COPY`ed `.env` file
- [ ] `.dockerignore` excludes `.env` and any credential files (see `03_dockerfile/04_dockerignore.md`)
- [ ] Local secrets come from a `.env` file, excluded from Git, passed via `--env-file`/`env_file` at run time
- [ ] Production secrets come from the platform's secret mechanism (Swarm secrets, Kubernetes Secrets, a cloud secret manager) — not a `.env` file copied into a production image
- [ ] Secrets are rotated by updating the secret store, not by rebuilding an image

## Quick summary

- Never bake a secret into an image via `ENV`, `ARG`, or `COPY` — it becomes permanently recoverable from that layer
- Locally, `.env` files passed at `run`/`up` time (via `--env-file`/`env_file`) are the normal, low-risk approach
- In production, secrets should come from the deployment platform's own secret mechanism, injected at runtime
- The application code itself usually doesn't need to change between the two — only where the values come from

## Next

**`02_image-optimization.md`** covers the other major production concern: keeping the image itself small.
