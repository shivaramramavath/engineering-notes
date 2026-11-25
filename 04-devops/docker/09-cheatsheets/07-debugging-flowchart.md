<!--
================================================================================
Metadata:
  Title: Quick Reference Cheat Sheet - Docker Production Debugging Flowcharts
  Track: Docker / DevOps
  Difficulty: All Levels
  Estimated Time: 30 minutes
  Key Terminology: Troubleshooting Flowcharts, Exit Codes, Network Triage, Build Diagnostics
================================================================================
-->

# Docker Production Debugging Flowcharts

[<- 06-security-checklist.md](./06-security-checklist.md) | [Back to Cheatsheets Overview](./README.md) | [Next Cheatsheet: 08-common-errors-and-fixes.md ->](./08-common-errors-and-fixes.md)

---

## 1. Flowchart: Container Fails to Start or Crashes Immediately

```
                     [ CONTAINER DIES OR ENTERS CrashLoopBackOff ]
                                           |
                                           v
                             Check Container Exit Code:
                             docker inspect <id> --format '{{.State.ExitCode}}'
                                           |
           +-------------------------------+-------------------------------+
           |                               |                               |
        Code 137                        Code 127                        Code 1 / 2
           v                               v                               v
  Was it OOMKilled?            "Command Not Found"             Application Startup Error
  docker inspect <id>          Executable or shell             Inspect application logs:
  --format '{{.State.OOMKilled}}' missing in rootfs?           docker logs --tail 100 <id>
           |                               |                               |
     +-----+-----+                 +-------+-------+               +-------+-------+
     |           |                 |               |               |               |
   TRUE        FALSE              Alpine        Missing          Missing        Syntax/DB
     v           v                glibc?        binary?          Env Var?       Migration?
Increase RAM  External kill     Binary needs   Check PATH,     Check .env     Wait for DB
limit (-m) or or manual        glibc; install  WORKDIR, and    or secret      healthcheck
tune leaks    docker stop      gcompat/glibc   ENTRYPOINT      injection      before start
```

---

## 2. Flowchart: Container Unreachable over Network

```
                    [ CLIENT CANNOT REACH CONTAINER ON PORT ]
                                        |
                                        v
                 Is container running and reporting healthy?
                 docker ps --filter "name=<container>"
                                        |
                    +-------------------+-------------------+
                    | YES                                   | NO (Exited / Unhealthy)
                    v                                       v
         Is port published to host?                    Inspect healthcheck logs:
         docker port <container>                       docker inspect <id>
                    |                                  --format '{{json .State.Health}}'
        +-----------+-----------+
        | Bound to 0.0.0.0      | Not mapped / bound to 127.0.0.1
        v                       v
Is process listening on       Map port with -p <host>:<cont>
0.0.0.0 or 127.0.0.1 inside?  or connect via reverse proxy
docker exec <id> netstat -tlpn
        |
    +---+---+
    |       |
0.0.0.0   127.0.0.1
    v       v
Check     Application is bound strictly to container loopback!
iptables  Change app code / server config:
firewall  Re-bind from 'localhost' to '0.0.0.0'
```

---

## 3. Flowchart: High Latency and Sluggish Performance

```
                     [ CONTAINER HIGH LATENCY / SLUGGISH ]
                                       |
                                       v
                     Inspect real-time CPU & Memory:
                     docker stats --no-stream <container>
                                       |
           +---------------------------+---------------------------+
           |                                                       |
     CPU% near 100%                                          Memory near limit
           v                                                       v
Is container CPU throttled by CFS?                       Is container paging to swap?
Check /sys/fs/cgroup/.../cpu.stat                        Check vmstat 1 on host (si/so)
`nr_throttled` increasing rapidly?                                 |
           |                                               +-------+-------+
     +-----+-----+                                         |               |
     |           |                                       HIGH SWAP      NO SWAP
    YES          NO                                        v               v
Increase --cpus  Application bottleneck;                 Set --memory-   Profile app
or lengthen CFS  Profile code threads                   swap equal to   heap; scale
period window    (e.g., node-inspect, pprof)             memory (-m)     replicas
```

---

## 4. Flowchart: Docker Build Failing or Extremely Slow

```
                         [ DOCKER BUILD ISSUES ]
                                    |
                    +---------------+---------------+
                    |                               |
               BUILD FAILS                     BUILD VERY SLOW
                    v                               v
         Identify failing layer step        Is context transfer taking minutes?
         Check error message text           Sending build context: 850MB...
                    |                               |
        +-----------+-----------+           +-------+-------+
        |                       |           |               |
Cache Miss / Network      Missing File     YES              NO
      v                         v           v                v
Package download failed;  Check context   Add missing      Enable BuildKit cache
Use BuildKit cache mount  and .docker-    .dockerignore    mounts (--mount=type=cache)
RUN --mount=type=cache    ignore path     (.git, node_mod) & parallelize stages
```

---

## 5. Flowchart: "No Space Left on Device" Outage

```
                     [ "NO SPACE LEFT ON DEVICE" ERROR ]
                                      |
                                      v
                        Inspect Host Storage State:
                        df -h && df -i
                                      |
           +--------------------------+--------------------------+
           |                                                     |
       df -h is 100%                                         df -i is 100%
  (Block Space Exhausted)                                 (Inode Exhaustion)
           v                                                     v
Find consuming directory:                             Millions of tiny files
sudo du -sh /var/lib/docker/*                         in overlay2 layers!
           |                                                     |
     +-----+-----+                                               v
     |           |                                    Prune all dangling images
Containers/    Overlay2/                              and stopped containers:
Log Files?     Images?                                docker system prune -af --volumes
     v           v
Truncate raw   Prune unused images:
JSON logs:     docker image prune -a
truncate -s 0  and build cache:
*-json.log     docker builder prune -a
```

---

[<- 06-security-checklist.md](./06-security-checklist.md) | [Back to Cheatsheets Overview](./README.md) | [Next Cheatsheet: 08-common-errors-and-fixes.md ->](./08-common-errors-and-fixes.md)
