<!--
================================================================================
Metadata:
  Title: Quick Reference Cheat Sheet - Docker CLI Commands
  Track: Docker / DevOps
  Difficulty: All Levels
  Estimated Time: 25 minutes
  Key Terminology: docker run, docker exec, docker inspect, docker system prune, Go Templates
================================================================================
-->

# Docker CLI Quick Reference Cheat Sheet

[<- Back to Cheatsheets Overview](./README.md) | [Next Cheatsheet: 02-dockerfile-instructions.md ->](./02-dockerfile-instructions.md)

---

## 1. Container Lifecycle Commands

```bash
# Run a new container in detached background mode with port publishing and name
docker run -d --name <name> -p <host_port>:<container_port> <image>

# Run an interactive container with automatic removal upon exit
docker run --rm -it <image> /bin/sh

# Run container with specific restart policy
docker run -d --restart unless-stopped <image>

# Run with environment variables (inline and file)
docker run -d -e ENV_VAR=value --env-file .env <image>

# Run with CPU and Memory resource constraints
docker run -d --cpus="1.5" -m 512m --memory-swap 512m --pids-limit 150 <image>

# Run with custom hostname and custom DNS
docker run -d --hostname my-host --dns 8.8.8.8 <image>

# Start, stop, and restart containers
docker start <container>              # Start stopped container
docker stop -t <seconds> <container>  # Graceful SIGTERM stop (default 10s)
docker restart <container>            # Restart running container
docker pause <container>              # Freeze processes via cgroup freezer
docker unpause <container>            # Thaw processes
docker kill --signal=SIGHUP <cont>    # Send specific POSIX signal

# Container deletion
docker rm <container>                 # Remove stopped container
docker rm -f <container>              # Force-stop and remove running container
```

---

## 2. Container Inspection, Logs, and Metrics

```bash
# Listing Containers
docker ps                             # List active running containers
docker ps -a                          # List all containers (running and stopped)
docker ps -q                          # Return only container numeric IDs
docker ps -s                          # Display disk size consumed by writable layer

# Logging
docker logs <container>               # Dump stdout/stderr logs
docker logs -f --tail 100 <container> # Follow live logs showing last 100 lines
docker logs -t --since 15m <container># Show logs from last 15 mins with timestamps

# Inspection and Filtering
docker inspect <container>            # Full JSON configuration metadata
docker port <container>               # List published port mappings
docker top <container>                # List active processes running inside container
docker diff <container>               # Show modified/added/deleted files in writable layer
docker stats                          # Live CPU, memory, network I/O streaming table
docker stats --no-stream              # Single-shot snapshot of resource metrics
docker events --since '1h'            # Stream real-time engine events from last hour
```

---

## 3. Advanced Go Template Formatting (`--format`)

```bash
# Extract IP address of a container
docker inspect <id> --format '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}'

# Check if a container was killed by Linux OOM Killer
docker inspect <id> --format 'OOMKilled: {{.State.OOMKilled}} | ExitCode: {{.State.ExitCode}}'

# Extract healthcheck status
docker inspect <id> --format '{{.State.Health.Status}}'

# List all running containers formatted as custom table
docker ps --format "table {{.ID}}\t{{.Names}}\t{{.Status}}\t{{.Ports}}"

# List container mounts and destinations
docker inspect <id> --format '{{range .Mounts}}{{.Source}} -> {{.Destination}} ({{.Type}}){{"\n"}}{{end}}'
```

---

## 4. Execution and File Transfer

```bash
# Execute interactive shell inside running container
docker exec -it <container> /bin/sh
docker exec -it <container> /bin/bash

# Execute command as specific user inside container
docker exec -u 0 -it <container> whoami   # Run as root inside container

# Copy files between host and container
docker cp ./local-file.txt <container>:/app/data.txt  # Host -> Container
docker cp <container>:/app/logs.tar.gz ./logs.tar.gz  # Container -> Host
```

---

## 5. Image Management Commands

```bash
# Building Images
docker build -t <repository>:<tag> .                     # Build using standard builder
DOCKER_BUILDKIT=1 docker build -t <repo>:<tag> .         # Build using BuildKit
docker build --no-cache -t <repo>:<tag> .                # Force full rebuild without cache
docker build --target <stage_name> -t <repo>:test .      # Target specific build stage
docker build --build-arg ENV=production -t <repo> .      # Pass build arguments

# Listing and Searching
docker images                                            # List locally cached images
docker images -a                                         # Include intermediate layers
docker images -f "dangling=true"                         # List untagged dangling images
docker history --no-trunc <image>                        # Inspect layer history commands

# Registry Interaction
docker login -u <username> [registry_url]                # Authenticate with registry
docker pull <image>:<tag>                                # Pull image from registry
docker pull <image>@sha256:<digest>                      # Pull immutable image digest
docker tag <source_image> <target_repo>:<tag>            # Add alias tag to image
docker push <target_repo>:<tag>                          # Push image to registry
docker logout [registry_url]                             # Log out from registry

# Tarball Serialization
docker save -o image.tar <image>:<tag>                   # Export image to tar archive
docker load -i image.tar                                 # Import image from tar archive
docker rmi <image>                                       # Remove local image
```

---

## 6. System Clean-up and Pruning

```bash
# Prune stopped containers
docker container prune -f

# Prune dangling (untagged) images
docker image prune -f

# Prune ALL unused images (not just dangling ones)
docker image prune -a -f

# Prune unused named volumes (CAUTION: Deletes persistent data!)
docker volume prune -f

# Prune unused networks
docker network prune -f

# The Nuclear Option: Clean all unused containers, networks, images, and build cache
docker system prune -a --volumes -f

# Inspect disk space consumed by Docker
docker system df
docker system df -v                                      # Verbose layer breakdown
```

---

## 7. Recommended Shell Aliases

Add these productivity aliases to your `~/.bashrc` or `~/.zshrc`:

```bash
alias d="docker"
alias dps="docker ps --format 'table {{.Names}}\t{{.Status}}\t{{.Ports}}'"
alias dpsa="docker ps -a --format 'table {{.Names}}\t{{.Status}}\t{{.Ports}}'"
alias dstats="docker stats --format 'table {{.Name}}\t{{.CPUPerc}}\t{{.MemUsage}}\t{{.NetIO}}'"
alias dstopall='docker stop $(docker ps -q)'
alias drmall='docker rm -f $(docker ps -aq)'
alias dprune='docker system prune -af --volumes'
```

---

[<- Back to Cheatsheets Overview](./README.md) | [Next Cheatsheet: 02-dockerfile-instructions.md ->](./02-dockerfile-instructions.md)
