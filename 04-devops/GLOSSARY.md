# 04-DevOps Master Glossary & Technical Lexicon

This glossary provides authoritative definitions and architectural context for foundational concepts across Linux systems, container runtimes, version control, web proxies, and cloud infrastructure.

---

## A
- **AMI (Amazon Machine Image)**: A pre-configured virtual machine template containing an operating system, software packages, and configurations used to launch EC2 instances on AWS.
- **AppArmor**: A Linux Security Module (LSM) that restricts programs' capabilities (file access, network sockets, raw capabilities) using per-program profiles. Default on Debian and Ubuntu.
- **Availability Zone (AZ)**: One or more discrete data centers within an AWS Region, engineered to be physically isolated from failures in other AZs while interconnected via ultra-low-latency private fiber.

## B
- **Bare Repository**: A Git repository initialized without a working tree (`git init --bare`). Used exclusively on central remote servers to receive pushes and serve clones.
- **Bastion Host (Jump Host)**: A hardened, minimal server placed in a public subnet used as a secure SSH proxy to access instances located in private, isolated subnets.
- **Blob (Binary Large Object)**: In Git, an immutable object type that stores raw file contents identified by the SHA-1/SHA-256 hash of its header and payload (does not store file names or permissions).
- **Bridge Network**: A software-defined Linux virtual network switch connecting multiple network interfaces, allowing containers running on the same host to communicate while isolating them from the external network.
- **BuildKit**: The modern, high-performance container build backend for Docker Engine featuring concurrent multi-stage execution, secrets mounting, and granular build-cache management.

## C
- **cgroup (Control Group)**: A Linux kernel feature that isolates, meters, and limits resource usage (CPU, memory, disk I/O, network bandwidth) for groups of processes. cgroup v2 provides a unified single-hierarchy resource model.
- **CI/CD (Continuous Integration & Continuous Delivery/Deployment)**: Software development methodologies that automate the validation, testing, container packaging, and production release of code changes.
- **CIDR (Classless Inter-Domain Routing)**: An IP address allocation notation (`10.0.0.0/16`) specifying the network prefix and total number of available host addresses in a subnet.
- **CloudFront**: AWS's global Content Delivery Network (CDN) service that caches static and dynamic web content at edge locations worldwide to reduce user latency.
- **CloudWatch**: An AWS observability and monitoring service collecting metrics, logs, events, and alarms across cloud infrastructure and application workloads.
- **Container**: An isolated, lightweight userspace runtime instance running directly on the host kernel, created using Linux namespaces, cgroups, and a layered root filesystem.
- **Containerd**: An industry-standard, lightweight container runtime manager supervising the complete container lifecycle (image transfer, execution via runc, storage attachment, network plumbing).

## D
- **Daemon**: A background service process operating without an attached controlling terminal, typically managed by systemd on Linux (`dockerd`, `sshd`, `nginx`).
- **Dockerfile**: A declarative text file containing sequential instructions (`FROM`, `RUN`, `COPY`, `ENTRYPOINT`) used by BuildKit to construct an OCI container image.
- **DynamoDB**: A fully managed, serverless NoSQL database service on AWS providing single-digit millisecond latency at any scale using partition keys and sort keys.

## E
- **EBS (Elastic Block Store)**: High-performance, persistent block storage volumes designed for use with Amazon EC2 instances within a single Availability Zone.
- **EC2 (Elastic Compute Cloud)**: Amazon's core IaaS offering providing resizable virtual compute capacity (instances) across diverse CPU families (Intel, AMD, Graviton).
- **ECS (Elastic Container Service)**: A fully managed container orchestration service by AWS supporting both EC2 instance pools and serverless AWS Fargate compute.
- **epoll**: An event notification facility in the Linux kernel scaling to tens of thousands of concurrent file descriptors in $O(1)$ time; powers modern web servers like Nginx and Node.js.

## F
- **Fast-Forward Merge**: A Git merge operation where the target branch pointer is simply advanced to the tip of the incoming branch because no divergent commits exist on the target branch.
- **Fargate**: A serverless compute engine for containers on AWS that runs ECS tasks or EKS pods without requiring management of underlying EC2 virtual machines.

## G
- **Git**: A distributed version control system tracking changes in source code using an immutable directed acyclic graph (DAG) of cryptographic commit objects.
- **GitHub Actions**: A cloud-native CI/CD and workflow automation platform integrated into GitHub repositories, driven by YAML workflow definitions.
- **Graviton**: Custom 64-bit ARM-based processors designed by AWS (Graviton3, Graviton4) offering up to 40% superior price-performance over traditional x86_64 architectures.

## H
- **HTTP/3**: The third major version of the Hypertext Transfer Protocol, replacing TCP with the UDP-based QUIC transport protocol to eliminate head-of-line blocking and accelerate handshakes.

## I
- **IAM (Identity and Access Management)**: An AWS security service governing authentication and authorization of human users, machine roles, and service identities via JSON policy documents.
- **IaC (Infrastructure as Code)**: The practice of provisioning, managing, and tracking infrastructure resources using declarative configuration files (Terraform, OpenTofu, CloudFormation, CDK).
- **Image**: An immutable, read-only package containing application binaries, libraries, runtimes, and dependencies organized as a cryptographic stack of OCI layers.
- **Inode (Index Node)**: A data structure on a Linux filesystem storing metadata about a file (size, permissions, owner, timestamps, block pointers), excluding its file name.

## J
- **Journalctl**: The command-line utility used to query and inspect binary logs collected by the `systemd-journald` service on modern Linux distributions.

## K
- **KMS (Key Management Service)**: An AWS managed service that simplifies the creation, rotation, and lifecycle control of cryptographic keys used to encrypt cloud data.

## L
- **Layer**: An immutable tarball representing the filesystem delta produced by an individual instruction in a Dockerfile. Stacked using OverlayFS to form a rootfs.
- **Load Balancer**: A device or software reverse proxy (ALB, NLB, Nginx) distributing network or application traffic evenly across multiple backend worker servers.

## M
- **Multi-Stage Build**: A Dockerfile pattern employing multiple `FROM` instructions, allowing artifacts built in heavyweight compiler stages to be copied into minimal runtime images.

## N
- **Namespace (Linux)**: A kernel feature that partitions kernel resources so that a group of processes perceives an isolated instance of that resource (PID, Mount, Network, IPC, UTS, User).
- **NAT Gateway (Network Address Translation Gateway)**: A managed AWS networking service allowing instances in private subnets to initiate outbound traffic to the internet while preventing inbound connections.
- **Nginx**: A high-performance, event-driven, asynchronous HTTP web server, reverse proxy, load balancer, and HTTP cache.

## O
- **OCI (Open Container Initiative)**: An open governance structure under the Linux Foundation establishing vendor-neutral specifications for container images (Image Spec) and execution runtimes (Runtime Spec).
- **OIDC (OpenID Connect)**: An identity layer built on top of OAuth 2.0; enables CI/CD runners (like GitHub Actions) to authenticate directly to AWS IAM without static credentials.
- **OverlayFS**: A modern union mount filesystem in Linux that combines multiple directories into a single unified mount point; the foundation of Docker's copy-on-write storage driver.

## P
- **Process Identifier (PID)**: An integer assigned by the Linux kernel to uniquely identify an active process in a PID namespace. PID 1 is the init process (`systemd`).
- **Proxy Pass**: An Nginx directive (`proxy_pass http://backend;`) that forwards inbound HTTP requests to a designated upstream backend server or socket.

## Q
- **QUIC**: A multiplexed, stream-oriented transport protocol built over UDP featuring integrated TLS 1.3 encryption and zero round-trip connection establishment; standard transport for HTTP/3.

## R
- **Rebase**: A Git operation that takes commits from one branch and replays them sequentially onto the tip of another base branch, maintaining a linear commit history.
- **Region**: A distinct geographic area worldwide containing multiple isolated, physically separated Availability Zones (e.g. `us-east-1`, `eu-west-1`).
- **Registry**: A hosted repository service storing and distributing versioned OCI container images (Docker Hub, Amazon ECR, GitHub GHCR).
- **Reverse Proxy**: A server intermediary sitting in front of web servers, intercepting client requests to handle TLS termination, caching, load balancing, and routing.
- **Route 53**: A highly available and scalable cloud Domain Name System (DNS) web service provided by AWS.

## S
- **S3 (Simple Storage Service)**: An object storage service provided by AWS offering industry-leading scalability, data availability, security, and 11 nines of durability.
- **SELinux (Security-Enhanced Linux)**: A Linux kernel security module implementing mandatory access control (MAC) based on security contexts and labels. Default on RHEL/Fedora/AL2023.
- **Systemd**: The standard system and service manager for Linux, initializing userspace, supervising services, mounting filesystems, and managing system timers.

## T
- **TLS (Transport Layer Security)**: A cryptographic protocol designed to provide end-to-end communication security over computer networks (HTTPS). Modern standards mandate TLS 1.2 and TLS 1.3.
- **Tree**: In Git, an immutable object representing a directory listing, associating mode bits and file names with corresponding blob and child tree SHA identifiers.

## U
- **Upstream**: In Nginx, a configuration block (`upstream { ... }`) defining a pool of backend application servers for load balancing and proxy forwarding.

## V
- **VPC (Virtual Private Cloud)**: A logically isolated, user-defined virtual network within the AWS cloud where compute and data resources are launched with dedicated IP ranges.

## W
- **Working Tree**: The local filesystem directory containing the checked-out, editable files of a Git repository derived from the repository's commit graph.
