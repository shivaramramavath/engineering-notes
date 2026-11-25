# 05 — Docker Compose

Everything so far — `run`, `-p`, `-v`, `network create` — works, but typing it all out by hand for a multi-container setup gets unwieldy fast. Docker Compose lets you describe your whole setup declaratively in one YAML file and bring it up with a single command.

## In this section

| File                          | Covers                                                                                                               |
| ----------------------------- | -------------------------------------------------------------------------------------------------------------------- |
| `01_compose-basics.md`        | The structure of a `docker-compose.yml` file, and `up`/`down`/`ps`/`logs`                                            |
| `02_compose-multi-service.md` | Running multiple linked services together — app + database + cache — with `depends_on`, shared networks, and volumes |

## Why this matters

Recall the long commands from `04_volumes_networking`:

```bash
docker network create my-network
docker run -d --name db --network my-network -v mydata:/var/lib/postgresql/data postgres
docker run -d --name app --network my-network -p 3000:3000 myapp
```

Compose turns that into one declarative file and one command:

```bash
docker compose up
```

Everything you already know — images, containers, volumes, networks, ports, environment variables — is still exactly what Compose configures under the hood. Compose doesn't introduce new concepts; it's a more convenient way to express the ones from earlier sections.

## What you should be able to do after this section

- Write a `docker-compose.yml` for a single service, replacing an equivalent `docker run` command
- Bring a whole multi-container setup up or down with one command
- Define a database + app + cache setup where services can reach each other by name, without manually creating a network
- Understand `depends_on` and its limits (start order, not readiness)

## Next

**`06_production`** covers taking a setup like this beyond your own machine — environment/secrets handling, image optimization, and building images in CI.
