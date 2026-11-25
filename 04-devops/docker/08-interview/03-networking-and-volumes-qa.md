<!--
================================================================================
Metadata:
  Title: Technical Interview Preparation - Networking and Storage Volumes Q&A
  Track: Docker / DevOps
  Difficulty: Junior to Senior
  Estimated Time: 65 minutes
  Key Terminology: Volumes, Bind Mounts, tmpfs, Bridge Network, Overlay Network, VXLAN, iptables, DOCKER-USER, Embedded DNS 127.0.0.11, Macvlan
================================================================================
-->

# Storage Volumes and Networking: 25 In-Depth Interview Questions & Answers

[<- 02-images-and-dockerfile-qa.md](./02-images-and-dockerfile-qa.md) | [Back to Interview Overview](./README.md) | [Next Note: 04-compose-qa.md ->](./04-compose-qa.md)

---

## Overview

This guide provides 25 production-grade interview questions covering Docker storage architecture (Named Volumes, Bind Mounts, tmpfs, volume plugins) and Linux container networking internals (Bridge, Overlay, Host, Macvlan, iptables chains, and DNS resolution).

---

## Questions Index

1. [Q01: What are the three primary storage mount types in Docker and when do you use each?](#q01-what-are-the-three-primary-storage-mount-types-in-docker-and-when-do-you-use-each)
2. [Q02: How do named Docker volumes differ architecturally from host bind mounts?](#q02-how-do-named-docker-volumes-differ-architecturally-from-host-bind-mounts)
3. [Q03: Why is writing persistent database state to the container writable layer an anti-pattern?](#q03-why-is-writing-persistent-database-state-to-the-container-writable-layer-an-anti-pattern)
4. [Q04: What happens when you mount a named volume to a container path that already contains data?](#q04-what-happens-when-you-mount-a-named-volume-to-a-container-path-that-already-contains-data)
5. [Q05: How do you safely back up and restore a named Docker volume without stopping sibling containers?](#q05-how-do-you-safely-back-up-and-restore-a-named-docker-volume-without-stopping-sibling-containers)
6. [Q06: What is a `tmpfs` mount and why is it used for security and performance?](#q06-what-is-a-tmpfs-mount-and-why-is-it-used-for-security-and-performance)
7. [Q07: How do volume file permissions work and how do you resolve UID/GID mismatch errors?](#q07-how-do-volume-file-permissions-work-and-how-do-you-resolve-uidgid-mismatch-errors)
8. [Q08: What are Docker volume driver plugins and how do they enable cloud block storage?](#q08-what-are-docker-volume-driver-plugins-and-how-do-they-enable-cloud-block-storage)
9. [Q09: Explain the five built-in Docker network drivers: bridge, host, overlay, macvlan, and none.](#q09-explain-the-five-built-in-docker-network-drivers-bridge-host-overlay-macvlan-and-none)
10. [Q10: Why should you avoid using the default `bridge` network in production environments?](#q10-why-should-you-avoid-using-the-default-bridge-network-in-production-environments)
11. [Q11: How does Docker's embedded DNS server (`127.0.0.11`) resolve container service names?](#q11-how-does-dockers-embedded-dns-server-1270011-resolve-container-service-names)
12. [Q12: What is the exact difference between `EXPOSE 8080`, `-p 8080:80`, and `-P`?](#q12-what-is-the-exact-difference-between-expose-8080--p-808080-and--p)
13. [Q13: How does Docker interact with Linux iptables and why does it bypass UFW firewall rules?](#q13-how-does-docker-interact-with-linux-iptables-and-why-does-it-bypass-ufw-firewall-rules)
14. [Q14: What is the purpose of the `DOCKER-USER` iptables chain?](#q14-what-is-the-purpose-of-the-docker-user-iptables-chain)
15. [Q15: How does Docker communicate from a container to the host machine?](#q15-how-does-docker-communicate-from-a-container-to-the-host-machine)
16. [Q16: How does container-to-container communication work across different user-defined networks?](#q16-how-does-container-to-container-communication-work-across-different-user-defined-networks)
17. [Q17: What is Docker Swarm Overlay networking and how does VXLAN encapsulation work?](#q17-what-is-docker-swarm-overlay-networking-and-how-does-vxlan-encapsulation-work)
18. [Q18: What is Macvlan networking and when is it strictly necessary?](#q18-what-is-macvlan-networking-and-when-is-it-strictly-necessary)
19. [Q19: What is IPvlan and how does it compare to Macvlan?](#q19-what-is-ipvlan-and-how-does-it-compare-to-macvlan)
20. [Q20: What is the `docker-proxy` userland binary and why should you disable it in production?](#q20-what-is-the-docker-proxy-userland-binary-and-why-should-you-disable-it-in-production)
21. [Q21: How do you configure Docker behind an enterprise HTTP/HTTPS outbound proxy?](#q21-how-do-you-configure-docker-behind-an-enterprise-httphttps-outbound-proxy)
22. [Q22: How can Docker containers be assigned static, fixed IP addresses?](#q22-how-can-docker-containers-be-assigned-static-fixed-ip-addresses)
23. [Q23: What are the security risks of using `--net=host`?](#q23-what-are-the-security-risks-of-using---nethost)
24. [Q24: How do you troubleshoot network connectivity issues between containers using CLI tools?](#q24-how-do-you-troubleshoot-network-connectivity-issues-between-containers-using-cli-tools)
25. [Q25: What happens when Docker bridge network subnets collide with corporate VPN IP subnets?](#q25-what-happens-when-docker-bridge-network-subnets-collide-with-corporate-vpn-ip-subnets)

---

## Technical Q&A

<details>
<summary><strong>Q01: What are the three primary storage mount types in Docker and when do you use each?</strong></summary>

1. **Volumes (Named Volumes)**: Stored in a Docker-managed area of the host filesystem (`/var/lib/docker/volumes/`). Completely isolated from host OS structure. Ideal for databases, persistent state, and production storage.
2. **Bind Mounts**: Maps an arbitrary path on the host filesystem (e.g. `/home/user/app` or `./src`) directly into the container. Ideal for local source code hot-reloading during development.
3. **tmpfs Mounts**: Stored strictly in host memory (RAM) and never written to disk. Ephemeral. Ideal for storing sensitive secrets or high-throughput temporary caches.
</details>

<details>
<summary><strong>Q02: How do named Docker volumes differ architecturally from host bind mounts?</strong></summary>

- **Management**: Volumes are fully managed by the Docker Engine API (`docker volume create/ls/rm`); bind mounts are raw host filesystem paths managed by the host OS.
- **Portability**: Volumes decouple containers from host filesystem layouts; bind mounts depend on specific host folder structures and absolute paths.
- **Driver Support**: Volumes support third-party storage plugins (AWS EBS, NFS, Ceph, GlusterFS) for network-attached storage; bind mounts only point to local host paths.
- **Initialization**: Named volumes automatically copy initial files from the container image path into the volume upon initial creation; bind mounts hide existing image directory content.
</details>

<details>
<summary><strong>Q03: Why is writing persistent database state to the container writable layer an anti-pattern?</strong></summary>

1. **Catastrophic Data Loss**: The writable layer is completely coupled to the container lifecycle. Running `docker rm` permanently destroys the container and all data in the writable layer.
2. **I/O Performance Degradation**: Writes to the container writable layer must travel through the storage driver union filesystem (`overlay2` copy-up mechanism), adding significant CPU context switching and disk latency. Volumes bypass the union filesystem and write directly to native host block storage at full hardware speed.
3. **Immutability Violation**: Containers should be stateless, ephemeral, and easily replaced during rollouts.
</details>

<details>
<summary><strong>Q04: What happens when you mount a named volume to a container path that already contains data?</strong></summary>

When a **named volume** is mounted to a non-empty container path:
- If the named volume is **brand new and empty**, Docker automatically **pre-populates the volume** by copying all files and subdirectories existing in the container image path into the volume, preserving initial file permissions.
- If the named volume **already contains data**, the contents of the volume take precedence, masking and overriding any files present in the image path.
*(Note: Bind mounts do NOT pre-populate; they always immediately mask the target container directory).*
</details>

<details>
<summary><strong>Q05: How do you safely back up and restore a named Docker volume without stopping sibling containers?</strong></summary>

Using a temporary Alpine container with volume mounts:
```bash
# Backup:
docker run --rm \
  --volumes-from my-db-container:ro \
  -v $(pwd):/backup \
  alpine tar -czvf /backup/db_backup.tar.gz -C /var/lib/postgresql/data .

# Restore to a new volume:
docker volume create new_db_data
docker run --rm \
  -v new_db_data:/data \
  -v $(pwd):/backup \
  alpine tar -xzvf /backup/db_backup.tar.gz -C /data
```
Mounting `--volumes-from ...:ro` guarantees read-only access to prevent corruption during backup archive creation.
</details>

<details>
<summary><strong>Q06: What is a `tmpfs` mount and why is it used for security and performance?</strong></summary>

A `tmpfs` mount allocates memory directly from host RAM backed by the Linux kernel virtual memory subsystem:
- **Security**: Sensitive temporary tokens, encryption keys, and session cookies stored in `tmpfs` are never written to physical disk or swap. When the container stops, the data is instantly wiped from memory.
- **Performance**: High-frequency read/write operations (such as temporary batch files, socket pipes, and compilation caches) execute at RAM bus speeds without generating disk I/O thrashing.
</details>

<details>
<summary><strong>Q07: How do volume file permissions work and how do you resolve UID/GID mismatch errors?</strong></summary>

Linux file ownership is governed by numeric UID/GID values inside the kernel, not username strings. If a host directory is owned by UID 0 (root) with `0755` permissions, and a container process runs as unprivileged UID 10001, write operations fail with `EACCES: permission denied`.
**Remediation Options**:
1. Change host folder ownership: `chown -R 10001:10001 /host/path`.
2. Use an entrypoint fix-up script running briefly as root to `chown` the mount before dropping privileges via `gosu` or `su-exec`.
3. Enable Docker User Namespaces (`userns-remap`) to map root inside the container to an unprivileged host UID.
</details>

<details>
<summary><strong>Q08: What are Docker volume driver plugins and how do they enable cloud block storage?</strong></summary>

Docker Volume Drivers extend the Engine's storage subsystem via HTTP/gRPC plugin APIs. Instead of storing data on the local node disk, volume plugins dynamically provision and mount network-attached storage:
- **AWS RexRay / EBS Plugin**: Automatically attaches and formats AWS EBS volumes to EC2 instances when a container starts on that node.
- **NFS Plugin**: Mounts central enterprise NFS/EFS shares directly into containers without requiring host-level fstab configurations:
  ```bash
  docker volume create --driver local \
    --opt type=nfs \
    --opt o=addr=192.168.1.50,rw \
    --opt device=:/exports/data nfs_volume
  ```
</details>

<details>
<summary><strong>Q09: Explain the five built-in Docker network drivers: bridge, host, overlay, macvlan, and none.</strong></summary>

1. **`bridge` (Default)**: Creates a software Linux bridge (`docker0` or custom) on the host. Containers receive private IPs (`172.x.x.x`) and communicate through NAT and port forwarding.
2. **`host`**: Removes network isolation. The container shares the host's network namespace, IP address, and port space directly.
3. **`overlay`**: Creates a multi-host distributed network across Docker Swarm nodes using VXLAN encapsulation, enabling seamless inter-node container communication.
4. **`macvlan`**: Assigns a unique MAC address to the container, making it appear as a physical network device connected directly to the physical LAN router.
5. **`none`**: Disables all networking. The container receives only a loopback interface (`127.0.0.1`), completely isolated from the outside world.
</details>

<details>
<summary><strong>Q10: Why should you avoid using the default `bridge` network in production environments?</strong></summary>

1. **No Automatic DNS Resolution**: Containers on the default `bridge` cannot resolve sibling containers by name (e.g. `ping db` fails); they can only communicate via fragile IP addresses or deprecated `--link` flags.
2. **Lack of Isolation**: All unassigned containers join the default bridge automatically, allowing untrusted containers to snoop or communicate unrestricted with production workloads.
3. Custom user-defined bridges provide automatic service name DNS resolution, customizable MTU/subnet sizing, and strict network isolation.
</details>

<details>
<summary><strong>Q11: How does Docker's embedded DNS server (`127.0.0.11`) resolve container service names?</strong></summary>

On user-defined networks, Docker configures `/etc/resolv.conf` inside containers to point nameservers to loopback IP `127.0.0.11`:
1. When an application queries a hostname (e.g. `curl http://backend-api:3000`), the query hits `127.0.0.11:53`.
2. Docker daemon's internal DNS resolver intercepts the query.
3. If the name matches an active container or service on that network, Docker returns its private container IP.
4. If the name is external (e.g. `google.com`), Docker forwards the query to the external upstream DNS servers configured on the host machine.
</details>

<details>
<summary><strong>Q12: What is the exact difference between `EXPOSE 8080`, `-p 8080:80`, and `-P`?</strong></summary>

- **`EXPOSE 8080` (Dockerfile metadata)**: Documentation only. Informs developers which port the application listens on. It does **not** publish or expose the port to the host machine.
- **`-p 8080:80` (Explicit Publishing)**: Maps host port 8080 to container port 80. Injects iptables NAT rules forwarding traffic from the host interface to the container.
- **`-P` (Publish All)**: Scans the Dockerfile for all `EXPOSE` directives and automatically maps each one to a randomly selected high-range ephemeral port (typically 32768–60999) on the host.
</details>

<details>
<summary><strong>Q13: How does Docker interact with Linux iptables and why does it bypass UFW firewall rules?</strong></summary>

When Docker initializes, it creates custom iptables chains (`PREROUTING` in the `nat` table and `DOCKER` in the `filter` table).
When you publish a port (`-p 8080:80`), Docker adds a rule to `PREROUTING` redirecting incoming traffic to the container IP before the packet reaches standard host firewall chains (such as UFW's `ufw-user-input`).
Consequently, even if UFW has a default policy of `DENY` for port 8080, Docker forwards the packet directly into the container. To fix this, administrators must add custom filtering rules to Docker's dedicated `DOCKER-USER` chain.
</details>

<details>
<summary><strong>Q14: What is the purpose of the `DOCKER-USER` iptables chain?</strong></summary>

Whenever the Docker daemon restarts or network interfaces change, Docker flushes and rebuilds its internal `DOCKER` iptables chain. Any custom firewall rules added to `INPUT` or `DOCKER` are wiped out.
Docker creates the `DOCKER-USER` chain specifically for system administrators. Any rule added to `DOCKER-USER` is evaluated **before** Docker's internal routing rules and is **never** flushed by Docker restarts, making it the proper place to implement IP whitelisting or rate-limiting firewalls.
</details>

<details>
<summary><strong>Q15: How does Docker communicate from a container to the host machine?</strong></summary>

1. **Via `host.docker.internal`**: On Docker Desktop (macOS/Windows) and Linux (Docker 20.10+ when passing `--add-host=host.docker.internal:host-gateway`), Docker maps `host.docker.internal` to the host's internal bridge gateway IP.
2. **Via Default Bridge Gateway**: By default, the host machine is accessible from inside containers using the default bridge gateway IP (typically `172.17.0.1` or the gateway reported by `ip route`).
3. **Via `--net=host`**: Runs the container directly in the host network namespace.
</details>

<details>
<summary><strong>Q16: How does container-to-container communication work across different user-defined networks?</strong></summary>

By default, containers on different Docker networks **cannot** communicate; iptables isolation rules drop inter-network traffic.
To allow communication:
A container can be dynamically connected to multiple networks simultaneously:
```bash
docker network connect backend-net frontend-api
```
The `frontend-api` container receives a secondary network interface (`eth1`) and an IP address on `backend-net`, allowing direct DNS resolution and TCP/UDP communication with backend services.
</details>

<details>
<summary><strong>Q17: What is Docker Swarm Overlay networking and how does VXLAN encapsulation work?</strong></summary>

Docker Swarm Overlay networking enables multi-host container clustering:
- It uses **VXLAN (Virtual Extensible LAN)** encapsulation running over UDP port 4789.
- Layer 2 Ethernet frames generated by a container are wrapped inside Layer 4 UDP packets by the Linux kernel.
- The host transmits the UDP packet across physical networks to the destination node.
- The destination host decapsulates the packet and delivers the raw Ethernet frame to the target container, simulating a flat, private local network across data centers.
</details>

<details>
<summary><strong>Q18: What is Macvlan networking and when is it strictly necessary?</strong></summary>

Macvlan connects containers directly to the physical host network interface (e.g. `eth0`):
- Each container receives its own unique MAC address and a routable IP directly from the physical network DHCP server or router subnet.
- The container bypasses Docker NAT and port forwarding entirely.
**When necessary**:
1. Legacy applications that require direct Layer 2 network adjacency or broadcast traffic.
2. Network monitoring tools (Wireshark, packet sniffers).
3. Telephony and VoIP systems (Asterisk/SIP) sensitive to NAT traversal.
</details>

<details>
<summary><strong>Q19: What is IPvlan and how does it compare to Macvlan?</strong></summary>

- **Macvlan**: Assigns a unique MAC address to every container. Can overwhelm network switch CAM tables if hundreds of containers run on a single physical host interface.
- **IPvlan**: All containers share the **same parent physical MAC address** but have unique IP addresses.
  - **IPvlan L2**: Operates at Layer 2 (ARP requests handled directly).
  - **IPvlan L3**: Operates at Layer 3 (routing handled without ARP broadcasts), providing massive scalability for thousands of microservices without switch port exhaustion.
</details>

<details>
<summary><strong>Q20: What is the `docker-proxy` userland binary and why should you disable it in production?</strong></summary>

When a port is published (`-p 8080:80`), Docker spawns a userspace binary called `docker-proxy` to forward packets between host and container as a fallback when hairpin NAT fails.
`docker-proxy` consumes approximately 5–10MB of RAM per exposed port and degrades packet throughput under heavy concurrency.
Setting `"userland-proxy": false` in `/etc/docker/daemon.json` disables the binary, directing all traffic strictly through high-performance Linux kernel iptables/NAT rules.
</details>

<details>
<summary><strong>Q21: How do you configure Docker behind an enterprise HTTP/HTTPS outbound proxy?</strong></summary>

1. **For Image Pulls (`dockerd`)**: Configure systemd drop-in file `/etc/systemd/system/docker.service.d/http-proxy.conf`:
   ```ini
   [Service]
   Environment="HTTP_PROXY=http://proxy.corp.internal:8080"
   Environment="HTTPS_PROXY=http://proxy.corp.internal:8080"
   Environment="NO_PROXY=localhost,127.0.0.1,docker-registry.corp"
   ```
2. **For Containers**: Configure client config in `~/.docker/config.json`:
   ```json
   {
     "proxies": {
       "default": {
         "httpProxy": "http://proxy.corp.internal:8080",
         "httpsProxy": "http://proxy.corp.internal:8080"
       }
     }
   }
   ```
</details>

<details>
<summary><strong>Q22: How can Docker containers be assigned static, fixed IP addresses?</strong></summary>

Static IPs can only be assigned on **user-defined networks** with a pre-configured subnet:
```bash
# 1. Create network with custom subnet
docker network create --subnet=192.168.10.0/24 custom-net

# 2. Run container with fixed IP
docker run -d --name static-db --net custom-net --ip 192.168.10.50 postgres:16-alpine
```
*(Attempting to assign a static IP on the default bridge network triggers an error).*
</details>

<details>
<summary><strong>Q23: What are the security risks of using `--net=host`?</strong></summary>

1. **Zero Network Isolation**: The container can listen on any host port, intercept host loopback traffic (`127.0.0.1`), and read unencrypted communications between other host processes.
2. **Cloud Metadata Exposure**: In cloud environments (AWS, GCP), processes inside `--net=host` containers have direct access to the Instance Metadata Service (IMDS at `169.254.169.254`), allowing potential theft of host IAM instance profile credentials.
3. **Port Collisions**: Two containers cannot bind to the same port on the host machine.
</details>

<details>
<summary><strong>Q24: How do you troubleshoot network connectivity issues between containers using CLI tools?</strong></summary>

1. Inspect container network settings and IP addresses:
   ```bash
   docker inspect <id> --format '{{range .NetworkSettings.Networks}}{{.IPAddress}}{{end}}'
   ```
2. Test DNS resolution and ping using an ephemeral debug container attached to the same network:
   ```bash
   docker run --rm --net <network_name> nicolaka/netshoot nslookup target-service
   ```
3. Capture live network packets using `tcpdump` inside the container network namespace:
   ```bash
   docker run --rm --net=container:<target_id> nicolaka/netshoot tcpdump -nn -i eth0 port 80
   ```
</details>

<details>
<summary><strong>Q25: What happens when Docker bridge network subnets collide with corporate VPN IP subnets?</strong></summary>

By default, Docker allocates subnets in `172.17.0.0/16` and `172.18.0.0/16`. Many corporate VPNs and internal VPCs use these exact CIDR ranges.
When a collision occurs:
- The Linux kernel routing table favors the local Docker bridge route (`dev docker0`) over the VPN tun/tap interface because of route metrics.
- All outbound traffic destined for corporate internal servers or git repos is misrouted into Docker's empty local bridge, causing connection timeouts.
*Fix*: Reconfigure `"default-address-pools"` in `/etc/docker/daemon.json` to allocate subnets in an unused private range (e.g. `10.200.0.0/16`).
</details>

---

[<- 02-images-and-dockerfile-qa.md](./02-images-and-dockerfile-qa.md) | [Back to Interview Overview](./README.md) | [Next Note: 04-compose-qa.md ->](./04-compose-qa.md)
