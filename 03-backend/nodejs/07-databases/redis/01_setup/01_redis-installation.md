# Redis Installation

Pick one method. **Docker is recommended**: it is identical on every OS and easy to throw away.

## Option 1: Docker (recommended)

### Single container

```bash
docker run -d \
  --name redis \
  -p 6379:6379 \
  -v redis-data:/data \
  redis:7 redis-server --appendonly yes
```

- `-p 6379:6379` exposes Redis on localhost
- `-v redis-data:/data` keeps data across restarts
- `--appendonly yes` enables AOF persistence

Manage it:

```bash
docker stop redis
docker start redis
docker logs -f redis
docker rm -f redis      # remove container (volume stays)
```

### Docker Compose (Redis + Insight)

Create `docker-compose.yml` in your project:

```yaml
services:
  redis:
    image: redis:7
    container_name: redis
    command: ["redis-server", "--appendonly", "yes"]
    ports:
      - "6379:6379"
    volumes:
      - redis-data:/data
    healthcheck:
      test: ["CMD", "redis-cli", "ping"]
      interval: 5s
      timeout: 3s
      retries: 5

  redis-insight:
    image: redis/redisinsight:latest
    container_name: redis-insight
    ports:
      - "5540:5540"
    depends_on:
      - redis

volumes:
  redis-data:
```

```bash
docker compose up -d
```

Open Insight at <http://localhost:5540>. When adding the database inside Insight, use host `redis` (the Compose service name), not `localhost`.

## Option 2: macOS (Homebrew)

```bash
brew install redis
brew services start redis     # run in background
# or run in foreground:
redis-server
```

## Option 3: Ubuntu / Debian

Use the official Redis package repository for a recent version:

```bash
sudo apt-get install -y lsb-release curl gpg
curl -fsSL https://packages.redis.io/gpg | sudo gpg --dearmor -o /usr/share/keyrings/redis-archive-keyring.gpg
echo "deb [signed-by=/usr/share/keyrings/redis-archive-keyring.gpg] https://packages.redis.io/deb $(lsb_release -cs) main" | sudo tee /etc/apt/sources.list.d/redis.list
sudo apt-get update
sudo apt-get install -y redis

sudo systemctl enable --now redis-server
```

> Check the [official install guide](https://redis.io/docs/latest/operate/oss_and_stack/install/install-stack/) if these steps have changed.

## Option 4: Windows

Redis does not officially support native Windows. Use one of:

1. **WSL2** (recommended): install Ubuntu from the Microsoft Store, then follow the Ubuntu steps above
2. **Docker Desktop**: use Option 1

## Verify the installation

```bash
redis-cli ping
# PONG
```

With Docker and no local `redis-cli`:

```bash
docker exec -it redis redis-cli ping
```

Check the version:

```bash
redis-server --version
redis-cli INFO server | grep redis_version
```

## Useful default settings

| Setting     | Default       | Notes                                                   |
| ----------- | ------------- | ------------------------------------------------------- |
| Port        | `6379`        | Change with `--port` or `redis.conf`                    |
| Bind        | `127.0.0.1`   | Local only. Do not expose publicly without auth and TLS |
| Databases   | `16` (0-15)   | Cluster mode supports only DB 0                         |
| Persistence | RDB snapshots | Enable AOF for stronger durability                      |

## Redis Stack vs plain Redis

- **Redis (Community/OSS)**: core data structures. This is all this course needs.
- **Redis Stack**: bundles extras like JSON, Search and Time Series. Only install it if you plan to use those modules.

## Common problems

| Problem                                        | Cause                                | Fix                                          |
| ---------------------------------------------- | ------------------------------------ | -------------------------------------------- |
| `Could not connect to Redis at 127.0.0.1:6379` | Server not running                   | Start the service or container               |
| `port is already allocated`                    | Another Redis is running             | Stop it or map another port (`-p 6380:6379`) |
| Insight cannot reach Redis in Docker           | Using `localhost` inside a container | Use the service name `redis`                 |
| Data gone after restart                        | No volume or persistence             | Mount `/data` and enable AOF                 |

**Next:** [Redis CLI and Insight](./02_redis-cli-and-insight.md)
