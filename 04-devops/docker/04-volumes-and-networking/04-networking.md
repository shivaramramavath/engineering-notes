# Docker Networking

Containers are isolated by default — two containers can't reach each other unless you explicitly connect them. This file covers Docker's default networks, getting containers to talk to each other by name, and mapping ports to the host.

```
Docker network       →  a virtual network containers can be attached to
Container DNS         →  containers on the same network can reach each other by container name
-p (port mapping)     →  exposes a container's port to the host machine
```

---

## Docker's built-in networks

```bash
docker network ls
```

```
NETWORK ID     NAME      DRIVER    SCOPE
a1b2c3d4e5f6   bridge    bridge    local
d4e5f6a7b8c9   host      host      local
g7h8i9j0k1l2   none      null      local
```

- **`bridge`** — the default network every container joins unless told otherwise. Containers on it get their own internal IP and can reach the outside world, but aren't reachable from the host without explicit port mapping.
- **`host`** — the container shares the host's network stack directly, no isolation at all. Rarely needed; mostly a Linux-specific option.
- **`none`** — no networking at all.

The default `bridge` network is what you're using any time you run `docker run` without specifying `--network`.

---

## The problem with the default bridge network

Containers on the default `bridge` network **cannot reach each other by name** — only by IP address, and that IP can change. This makes the default bridge network awkward for multi-container setups.

```bash
docker run -d --name db postgres
docker run -d --name app myapp
# `app` cannot reliably reach `db` by the name "db" on the default bridge network
```

---

## The fix: create a custom network

```bash
docker network create my-network

docker run -d --name db --network my-network postgres
docker run -d --name app --network my-network myapp
```

On a **custom** (user-defined) network, Docker provides automatic DNS resolution by container name:

```bash
docker exec app ping db
```

```
PING db (172.18.0.2): 56 data bytes
64 bytes from 172.18.0.2: Seq=0 time=0.1 ms
```

Inside `app`'s code, you'd connect to the database using the hostname `db` — not an IP address, and not `localhost`:

```js
// inside the app container
const dbHost = "db"; // resolves automatically via Docker's DNS
```

This is the mechanism that makes multi-container setups (app + database + cache, etc.) actually practical, and it's exactly what `docker-compose` sets up for you automatically (see `05_docker_compose`).

---

## Managing networks

```bash
docker network ls                     # list networks
docker network create my-network       # create a custom network
docker network inspect my-network      # see connected containers, subnet, etc.
docker network rm my-network            # remove a network (must have no attached containers)
docker network connect my-network db    # attach an already-running container to a network
docker network disconnect my-network db # detach it
```

A single container can be attached to multiple networks at once, useful for isolating which containers can reach which others (e.g. a database only reachable from an app tier, not from a public-facing web tier).

---

## Port mapping: reaching a container from the host

Everything above is about container-to-container communication. To reach a container **from your host machine** (or the outside world), you need explicit port mapping — this is separate from, and in addition to, whatever internal Docker network the container is on.

```bash
docker run -d -p 8080:80 nginx
```

`-p <host-port>:<container-port>` — requests to `localhost:8080` on your machine are forwarded to port `80` inside the container.

### Binding to a specific host interface

```bash
docker run -d -p 127.0.0.1:8080:80 nginx
```

Restricts the mapping to only be reachable from `localhost` on the host, rather than every network interface — a reasonable default for anything not meant to be reachable from other machines on your network.

### Mapping multiple ports

```bash
docker run -d -p 8080:80 -p 8443:443 nginx
```

### Letting Docker pick the host port

```bash
docker run -d -P nginx
```

`-P` (capital) publishes every port the image's `EXPOSE` instructions declare, each to a random available host port. Check what got assigned with:

```bash
docker port <container>
```

---

## Container-to-container vs container-to-host: don't mix them up

|                       | How                                           | Needed for                                                                   |
| --------------------- | --------------------------------------------- | ---------------------------------------------------------------------------- |
| Container ↔ container | Shared custom network + DNS by container name | App reaching its database, services reaching each other                      |
| Host ↔ container      | `-p host:container` port mapping              | You (or the outside world) reaching a container from outside Docker entirely |

A container on the same custom network as another **does not need** `-p` to reach it — port mapping is purely about the host boundary, not inter-container communication.

## Quick summary

- Containers are isolated by default; the default `bridge` network doesn't support DNS resolution by name
- Create a custom network (`docker network create`) to get automatic container-name-based DNS — the standard way to let containers find each other
- `-p host:container` maps a container's port to the host machine — a separate concern from container-to-container networking
- `-P` auto-publishes all `EXPOSE`d ports to random host ports

## Section complete

You now understand how containers persist data and communicate. **`05_docker_compose`** shows how to describe volumes, networks, and multiple services declaratively in one file, instead of typing long `docker run`/`network create` commands by hand every time.
