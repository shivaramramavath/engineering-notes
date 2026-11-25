<!--
================================================================================
Metadata:
  Title: Quick Reference Cheat Sheet - Top 25 Docker Common Errors and Fixes
  Track: Docker / DevOps
  Difficulty: All Levels
  Estimated Time: 30 minutes
  Key Terminology: Docker Errors, Port Collisions, CRLF line endings, Architecture mismatch, Daemon socket
================================================================================
-->

# Top 25 Docker Errors, Root Causes, and Immediate Fixes

[<- 07-debugging-flowchart.md](./07-debugging-flowchart.md) | [Back to Cheatsheets Overview](./README.md) | [Back to Track Root Overview ->](../README.md)

---

## Error Catalog & Quick Remediation

### 1. `permission denied while trying to connect to the Docker daemon socket`
- **Root Cause**: The current Linux user is not a member of the local `docker` group.
- **Immediate Fix**:
  ```bash
  sudo usermod -aG docker $USER && newgrp docker
  ```

### 2. `Bind for 0.0.0.0:80 failed: port is already allocated`
- **Root Cause**: Another container or host daemon (e.g., Apache, NGINX, or IIS) is already bound to that host port.
- **Immediate Fix**:
  ```bash
  sudo lsof -i :80 || sudo ss -tulpn | grep :80
  # Kill host process or change host mapping: -p 8080:80
  ```

### 3. `Error response from daemon: Conflict. The container name "/web" is already in use`
- **Root Cause**: A stopped or running container with that name already exists in Docker's metadata store.
- **Immediate Fix**:
  ```bash
  docker rm -f /web
  ```

### 4. `exec /entrypoint.sh: no such file or directory`
- **Root Cause**: The script was saved with Windows **CRLF (`\r\n`)** line endings instead of Linux **LF (`\n`)**, or the shebang (`#!/bin/bash`) points to a shell not installed in the image.
- **Immediate Fix**:
  ```bash
  dos2unix entrypoint.sh
  # In Dockerfile: RUN sed -i 's/\r$//' /entrypoint.sh && chmod +x /entrypoint.sh
  ```

### 5. `exec user process caused: exec format error`
- **Root Cause**: CPU architecture mismatch (e.g. running an `amd64` x86 binary on an ARM64 Apple Silicon or AWS Graviton machine).
- **Immediate Fix**:
  ```bash
  # Rebuild with Buildx for target platform:
  docker buildx build --platform linux/amd64,linux/arm64 -t my-app --load .
  ```

### 6. `no space left on device` (with free disk space)
- **Root Cause**: Filesystem **inode exhaustion** from millions of tiny untracked layers or build caches.
- **Immediate Fix**:
  ```bash
  docker system prune -af --volumes
  ```

### 7. `toomanyrequests: You have reached your pull rate limit`
- **Root Cause**: Docker Hub unauthenticated rate limit reached (100 pulls per 6 hours per IP).
- **Immediate Fix**:
  ```bash
  echo "$DOCKER_PASSWORD" | docker login -u "$DOCKER_USERNAME" --password-stdin
  # Or pull through a private cache mirror: localhost:5001/library/alpine
  ```

### 8. `dial tcp: lookup registry-1.docker.io: no such host`
- **Root Cause**: Host machine or Docker daemon has missing or broken DNS configurations.
- **Immediate Fix**:
  Add public DNS to `/etc/docker/daemon.json`:
  ```json
  { "dns": ["1.1.1.1", "8.8.8.8"] }
  ```
  Then run `sudo systemctl restart docker`.

### 9. `Container exited with code 137`
- **Root Cause**: Process killed by `SIGKILL` ($128 + 9$), almost always the Linux **Out-Of-Memory (OOM) Killer**.
- **Immediate Fix**:
  Verify with `docker inspect <id> --format '{{.State.OOMKilled}}'`. Increase memory limit (`-m 2g`) or tune application heap (`--max-old-space-size`).

### 10. `Container exited with code 127`
- **Root Cause**: "Command Not Found". The binary declared in `CMD` or `ENTRYPOINT` does not exist in the container `$PATH`.
- **Immediate Fix**:
  Verify binary exists inside image: `docker run --rm -it <image> which <binary>`.

### 11. `Container exited with code 126`
- **Root Cause**: "Permission Denied". The target executable lacks the executable bit (`+x`).
- **Immediate Fix**:
  In Dockerfile: `RUN chmod +x /entrypoint.sh`.

### 12. `standard_init_linux.go: exec user process caused: permission denied`
- **Root Cause**: The unprivileged user declared via `USER` does not have read/execute permissions for the working directory or binary.
- **Immediate Fix**:
  In Dockerfile: `COPY --chown=10001:10001 src/ /app/`.

### 13. `Error response from daemon: removal of container ... is already in progress`
- **Root Cause**: Container is stuck in `Dead` state because an orphaned host process is holding an open file descriptor on the overlay2 mount.
- **Immediate Fix**:
  ```bash
  sudo grep "<container_id>" /proc/*/mountinfo | cut -d/ -f3 | xargs -r sudo kill -9
  sudo umount -l /var/lib/docker/overlay2/<id>/merged
  docker rm -f <container_id>
  ```

### 14. `EACCES: permission denied, open '/app/data/file.txt'`
- **Root Cause**: Host directory mounted via bind mount is owned by `root` (UID 0), while container runs as non-root user (e.g. `node` UID 1000).
- **Immediate Fix**:
  ```bash
  sudo chown -R 1000:1000 ./host-data-dir
  ```

### 15. `docker-compose: command not found`
- **Root Cause**: Legacy Compose v1 is not installed. Modern Docker uses Compose v2 as a CLI plugin.
- **Immediate Fix**:
  Replace `docker-compose` (with hyphen) with `docker compose` (with space).

### 16. `failed to solve with frontend dockerfile.v0: failed to read dockerfile: open /var/lib/docker/tmp/...: no such file`
- **Root Cause**: Misspelled Dockerfile name or executing `docker build` in wrong directory.
- **Immediate Fix**:
  Specify explicit path: `docker build -f ./path/to/Dockerfile .`.

### 17. `failed to compute cache key: "/src" not found: not found`
- **Root Cause**: File or directory referenced in `COPY /src` does not exist in the build context or is excluded by `.dockerignore`.
- **Immediate Fix**:
  Check path relative to build context and inspect `.dockerignore`.

### 18. `failed to solve: rpc error: code = Unknown desc = secret mysecret not found`
- **Root Cause**: Dockerfile declared `RUN --mount=type=secret,id=mysecret`, but `--secret` was omitted from build command.
- **Immediate Fix**:
  Pass secret at build time: `docker buildx build --secret id=mysecret,src=.env .`.

### 19. `curl: (7) Failed to connect to localhost port 8080: Connection refused`
- **Root Cause**: Inside the container, the application server is bound strictly to `127.0.0.1` instead of `0.0.0.0`.
- **Immediate Fix**:
  Configure application to listen on `0.0.0.0` (all interfaces), not `127.0.0.1`.

### 20. `WARNING: IPv4 forwarding is disabled. Networking will not work.`
- **Root Cause**: Host Linux kernel has IP forwarding turned off in sysctl.
- **Immediate Fix**:
  ```bash
  echo "net.ipv4.ip_forward = 1" | sudo tee -a /etc/sysctl.d/99-docker.conf
  sudo sysctl --system
  sudo systemctl restart docker
  ```

### 21. `error while opening volume store metadata database: file is locked`
- **Root Cause**: Orphaned `dockerd` process running or abrupt host power loss left lock file.
- **Immediate Fix**:
  ```bash
  sudo killall -9 dockerd containerd
  sudo rm -f /var/lib/docker/volumes/metadata.db
  sudo systemctl start docker
  ```

### 22. `Cannot connect to the Docker daemon at unix:///var/run/docker.sock. Is the docker daemon running?`
- **Root Cause**: The Docker systemd service is stopped or failed to boot.
- **Immediate Fix**:
  ```bash
  sudo systemctl start docker && sudo journalctl -u docker.service -e --no-pager
  ```

### 23. `ERROR: for service ... Cannot start service: network ... not found`
- **Root Cause**: Compose file references an external network that has not yet been created.
- **Immediate Fix**:
  ```bash
  docker network create <network_name>
  ```

### 24. `npm ERR! code ENOSPC` during container build
- **Root Cause**: Host `/tmp` partition is out of space or Linux kernel inotify watch ceiling reached.
- **Immediate Fix**:
  ```bash
  echo fs.inotify.max_user_watches=524288 | sudo tee -a /etc/sysctl.conf && sudo sysctl -p
  ```

### 25. `The following directives are specified both as a flag and in the configuration file`
- **Root Cause**: Conflicting configuration declared in both systemd unit `ExecStart` and `/etc/docker/daemon.json`.
- **Immediate Fix**:
  Remove the duplicate flag from `daemon.json` or systemd drop-in file and run `sudo systemctl daemon-reload && sudo systemctl restart docker`.

---

[<- 07-debugging-flowchart.md](./07-debugging-flowchart.md) | [Back to Cheatsheets Overview](./README.md) | [Back to Track Root Overview ->](../README.md)
