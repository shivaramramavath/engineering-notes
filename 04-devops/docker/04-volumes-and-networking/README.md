# Module 04: Storage and Networking

```
Track: DevOps & Containerization
Module: 04-volumes-and-networking
Prerequisites: 01-fundamentals, 02-commands, 03-dockerfile
Difficulty: Intermediate to Advanced
Estimated Duration: 7 hours
Target Docker Version: 27.x+
Last Validated: February 2025
```

---

## Overview

Container persistence and inter-service connectivity form the backbone of modern distributed application architecture. While containers are fundamentally ephemeral, stateful workloads require bulletproof persistence strategies that survive container destruction and server migration. Similarly, microservice architectures demand secure, segmented, and high-performance network topologies.

This module provides an exhaustive, production-grade guide to Docker's storage engines (Volumes, Bind Mounts, Tmpfs, Overlay2) and networking model (Bridge, Host, DNS Discovery, Port Publishing, iptables routing, Dual-Stack IPv6, and Advanced Packet Diagnostics).

---

## Module Roadmap

```
04-volumes-and-networking/
├── 01-volumes-and-bind-mounts.md                         # Named volumes, bind mounts, VFS bypass, and permissions
├── 02-tmpfs-and-storage-drivers.md                       # In-memory storage, Overlay2 architecture, copy-up, whiteouts
├── 03-backup-and-restore-of-volumes.md                   # Disaster recovery, ephemeral tar sidecars, live DB backups
├── 04-networking.md                                      # Container Network Model (CNM), veth pairs, native drivers
├── 05-user-defined-networks-and-dns.md                   # Custom bridge subnets, 127.0.0.11 embedded DNS, round-robin
├── 06-ports-publishing-and-firewalls.md                  # Port publishing (-p/-P), iptables, DOCKER-USER, UFW bypass
├── 07-container-to-host-and-container-to-container.md    # host.docker.internal, host-gateway, --net=container sidecars
├── 08-ipv6-and-proxies.md                                # Dual-stack IPv6, corporate forward proxies, custom root CAs
└── 09-network-troubleshooting.md                         # Netshoot diagnostics, nsenter, tcpdump, and decision trees
```

---

## Detailed Notes Summary

| # | Note | Core Focus | Target Skills |
|:---|:---|:---|:---|
| **01** | [Volumes and Bind Mounts](01-volumes-and-bind-mounts.md) | Persistence architecture | Configure persistent volumes, hot-reload dev bind mounts |
| **02** | [Tmpfs and Storage Drivers](02-tmpfs-and-storage-drivers.md) | Memory mounts & Overlay2 | Protect secrets in RAM, understand CoW write amplification |
| **03** | [Backup and Restore](03-backup-and-restore-of-volumes.md) | Disaster recovery pipelines | Build automated volume backup sidecars, restore databases |
| **04** | [Networking Fundamentals](04-networking.md) | CNM, drivers, packet flow | Master bridge, host, none drivers, veth pairs, packet routing |
| **05** | [User-Defined Networks & DNS](05-user-defined-networks-and-dns.md) | IPAM & Service Discovery | Build multi-tier isolated networks, DNS round-robin |
| **06** | [Ports and Firewalls](06-ports-publishing-and-firewalls.md) | iptables & firewall rules | Fix UFW bypass, secure databases with 127.0.0.1 binding |
| **07** | [Inter-Container Communication](07-container-to-host-and-container-to-container.md) | Cross-topology routing | Connect container to host, implement Pod sidecar pattern |
| **08** | [IPv6 and Proxies](08-ipv6-and-proxies.md) | Modern enterprise networks | Deploy dual-stack IPv6, handle corporate MITM proxies |
| **09** | [Network Troubleshooting](09-network-troubleshooting.md) | Deep packet diagnostics | Debug with Netshoot, capture tcpdump pcaps, use nsenter |

---

## Learning Objectives

After completing this module, you will be able to:
1. Make principled decisions between Named Volumes, Bind Mounts, and Tmpfs for stateful services.
2. Troubleshoot and mitigate filesystem write-amplification inside the Linux OverlayFS2 storage driver.
3. Architect zero-downtime backup, restore, and migration workflows for production container volumes.
4. Design secure multi-tier network topologies with strict Layer 3/4 microservice isolation.
5. Prevent catastrophic security vulnerabilities caused by Docker's default bypass of host firewalls (UFW/Firewalld).
6. Enable cross-platform communication between containers and host services using `host.docker.internal:host-gateway`.
7. Implement high-performance sidecar and proxy architectures using shared network namespaces (`--net=container`).
8. Deploy dual-stack IPv4/IPv6 container networks and configure seamless operation behind corporate forward proxies.
9. Diagnose complex networking and DNS failures systematically using packet captures (`tcpdump`), `nsenter`, and Netshoot.

---

[← Previous Module: 03-dockerfile](../03-dockerfile/README.md) | [Next Module: 05-compose →](../05-compose/README.md)
