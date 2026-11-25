---
title: "Sendfile, Direct I/O, and Large File Delivery in NGINX"
description: "Mastering zero-copy kernel streaming: sendfile(), tcp_nopush, tcp_nodelay, asynchronous I/O thread pools (aio threads), and directio for high-throughput static delivery."
module: "03-traffic-management"
track: "devops-nginx"
order: 9
difficulty: "Advanced"
prerequisites:
  - "01-fundamentals/04-static-files"
  - "02-core-features/07-compression-gzip-and-brotli"
tags:
  - nginx
  - sendfile
  - zero-copy
  - tcp-nopush
  - aio-threads
  - directio
  - performance
last_updated: "2026-03-30"
---

# Sendfile, Direct I/O, and Large File Delivery in NGINX

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Directives Reference](#4-syntax--directives-reference)
- [5. Practical Implementation Examples](#5-practical-implementation-examples)
  - [Example 1: The Canonical Zero-Copy Static Delivery Stack](#example-1-the-canonical-zero-copy-static-delivery-stack)
  - [Example 2: Multi-Gigabyte Media Streaming with Direct I/O (`directio`)](#example-2-multi-gigabyte-media-streaming-with-direct-io-directio)
  - [Example 3: Non-Blocking Disk Reads with AIO Thread Pools (`aio threads`)](#example-3-non-blocking-disk-reads-with-aio-thread-pools-aio-threads)
  - [Example 4: Hybrid Small-Asset Cache vs Large-File Direct Streaming](#example-4-hybrid-small-asset-cache-vs-large-file-direct-streaming)
  - [Example 5: Vagrant / VirtualBox / NFS Shared Folder Workaround](#example-5-vagrant--virtualbox--nfs-shared-folder-workaround)
- [6. Production Best Practices](#6-production-best-practices)
- [7. Common Pitfalls & Anti-Patterns](#7-common-pitfalls--anti-patterns)
- [8. Performance, Security & Troubleshooting](#8-performance-security--troubleshooting)
- [9. Architectural Comparison Table](#9-architectural-comparison-table)
- [10. Technical Interview Questions](#10-technical-interview-questions)
- [11. Hands-On Practice Exercises](#11-hands-on-practice-exercises)
- [12. Summary & Quick Reference](#12-summary--quick-reference)
- [13. Further Reading & Official Documentation](#13-further-reading--official-documentation)

---

## 1. What & Why

In traditional web server implementations, serving a static file (image, JavaScript bundle, or video) from disk to a network socket involves standard POSIX `read()` and `write()` system calls. This standard I/O path introduces significant performance overhead:
1. **Redundant Memory Copies**: Data is copied from disk to kernel page cache, from kernel page cache to user-space application memory, from user space back to kernel socket buffer, and finally to the Network Interface Card (NIC).
2. **Context Switching Overhead**: Every read/write operation requires transitioning CPU privilege levels between user mode and kernel mode four times per transfer chunk.
3. **Event Loop Starvation**: While NGINX's `epoll` event loop is non-blocking for network sockets, standard POSIX disk I/O does *not* support non-blocking operations on Linux filesystems. If a requested file is not in RAM, an NGINX worker thread can block synchronously waiting for physical disk read heads, freezing thousands of other client connections.

To achieve saturation-level line-rate throughput (40Gbps to 100Gbps+ on modern NICs), NGINX leverages **zero-copy kernel streaming (`sendfile`)**, socket packet packaging (`tcp_nopush`), and **asynchronous I/O thread pools (`aio threads`)**.

---

## 2. Core Concepts

### Traditional I/O vs Zero-Copy `sendfile()`
- **Traditional I/O (4 Copies, 4 Context Switches)**:
  `read(disk_fd, buf, len)` $\rightarrow$ `write(socket_fd, buf, len)`
  Requires copying data through user-space memory buffers.
- **Zero-Copy `sendfile()` (2 Copies, 2 Context Switches)**:
  `sendfile(socket_fd, disk_fd, offset, count)`
  Instructs the Linux kernel to stream data directly from the OS page cache to the network socket buffer using Direct Memory Access (DMA). The data never enters user-space RAM, consuming zero user CPU cycles.

### The TCP Socket Trinity: `sendfile`, `tcp_nopush`, and `tcp_nodelay`
In NGINX, maximum network throughput is achieved by combining three interdependent directives:
1. **`sendfile on;`**: Activates kernel-level zero-copy streaming.
2. **`tcp_nopush on;`**: Activates the Linux `TCP_CORK` socket option. NGINX accumulates outgoing HTTP header packets and payload chunks until they fill a complete Maximum Segment Size (MSS, typically 1460 bytes) before pushing them onto the wire, eliminating fractional packet fragmentation.
3. **`tcp_nodelay on;`**: Disables Nagle's algorithm (`TCP_NODELAY`), forcing the socket to send final trailing packets immediately without waiting for TCP acknowledgments.

### Direct I/O (`directio`)
When serving multi-gigabyte files (ISOs, raw video streams, database backups), reading through the OS page cache is detrimental:
- The multi-gigabyte file pushes out frequently accessed web pages, CSS, and database indices from RAM (page cache thrashing).
- The file is read once and never reused.
The `directio size;` directive instructs NGINX to use the Linux `O_DIRECT` flag for files larger than `size` (e.g., `directio 4m;`). This bypasses the OS page cache entirely, streaming data directly from physical disk to the network card via DMA.

### Asynchronous I/O Thread Pools (`aio threads`)
Because disk reads can block, modern NGINX supports offloading blocking disk operations to background worker thread pools (`aio threads=default;`). When a worker process needs to read a file not in page cache, it delegates the I/O task to a thread pool and immediately resumes serving other client events on the `epoll` event loop.

---

## 3. How It Works Internally

### Memory Dataflow: Traditional I/O vs Zero-Copy sendfile()

```
Traditional I/O Path (read / write)
+-------------+      read()      +-------------+   Copy to User   +-------------+
|  Hard Disk  | ---------------> | Page Cache  | ---------------> | NGINX User  |
|             |                  |  (Kernel)   |                  | Space Buffer|
+-------------+                  +-------------+                  +------+------+
                                                                         |
                                                                   write() (Kernel)
                                                                         v
+-------------+     DMA Transfer +-------------+  Socket Buffer   +-------------+
|     NIC     | <--------------- | Socket Buf  | <--------------- | Kernel Space|
| (Wire Out)  |                  |  (Kernel)   |                  +-------------+
+-------------+                  +-------------+

Zero-Copy sendfile() Path
+-------------+   DMA Transfer   +-------------+
|  Hard Disk  | ---------------> | Page Cache  |
|             |                  |  (Kernel)   |
+-------------+                  +------+------+
                                        |
                            Kernel sendfile() System Call
                            (Zero user-space copies!)
                                        v
+-------------+   DMA Transfer   +-------------+
|     NIC     | <--------------- | Socket Buf  |
| (Wire Out)  |                  | (Descriptors|
+-------------+                  +-------------+
```

---

## 4. Syntax & Directives Reference

| Directive | Context | Default | Purpose / Details |
| :--- | :--- | :--- | :--- |
| `sendfile on\|off` | `http`, `server`, `location`, `if in location` | `off` | Toggles the use of the `sendfile()` kernel system call for static files. |
| `sendfile_max_chunk size` | `http`, `server`, `location` | `0` (Uncapped) | Caps the chunk size sent in a single `sendfile()` call to prevent worker hogging. |
| `tcp_nopush on\|off` | `http`, `server`, `location` | `off` | Enables `TCP_CORK` on Linux; sends HTTP headers and start of file in one packet. |
| `tcp_nodelay on\|off` | `http`, `server`, `location` | `on` | Disables Nagle's algorithm for keepalive and interactive connections. |
| `aio on\|off\|threads[=pool]` | `http`, `server`, `location` | `off` | Enables asynchronous I/O; `aio threads` enables non-blocking thread pools. |
| `thread_pool name threads=number [max_queue=number]` | `main` | `default 32` | Declares background thread pool in the top-level main context. |
| `directio size\|off` | `http`, `server`, `location` | `off` | Enables `O_DIRECT` for files exceeding `size`, bypassing OS page cache. |
| `directio_alignment size` | `http`, `server`, `location` | `512` | Alignment for Direct I/O (typically 512 bytes or 4KB for modern 4Kn SSDs). |

---

## 5. Practical Implementation Examples

### Example 1: The Canonical Zero-Copy Static Delivery Stack

The standard high-performance configuration for serving frontend JavaScript, CSS, and web assets at line rate.

```nginx
# /etc/nginx/conf.d/static_zerocopy.conf

server {
    listen 80;
    server_name static.example.com;
    root /var/www/assets;

    # 1. Zero-Copy Kernel Streaming
    sendfile on;

    # 2. Package headers and file chunks into full MSS packets
    tcp_nopush on;

    # 3. Disable Nagle's algorithm for instant packet delivery
    tcp_nodelay on;

    # 4. Prevent a single fast client on a massive download from monopolizing worker
    sendfile_max_chunk 512k;

    location / {
        expires 30d;
        add_header Cache-Control "public, immutable";
        try_files $uri =404;
    }
}
```

### Example 2: Multi-Gigabyte Media Streaming with Direct I/O (`directio`)

Serving large video files (MP4, MKV) and OS ISO images without destroying the operating system's page cache.

```nginx
# /etc/nginx/conf.d/media_streaming.conf

server {
    listen 80;
    server_name media.example.com;
    root /var/www/downloads;

    location /videos/ {
        sendfile on;
        tcp_nopush on;

        # Files under 8MB use sendfile() via OS page cache
        # Files 8MB and larger use O_DIRECT, bypassing page cache to avoid cache thrashing!
        directio 8m;
        directio_alignment 4k; # Optimized for modern 4Kn NVMe SSDs

        # HTTP Range header support for video seeking
        max_ranges 5;

        add_header Accept-Ranges bytes;
    }
}
```

### Example 3: Non-Blocking Disk Reads with AIO Thread Pools (`aio threads`)

Offloading disk reads to background worker thread pools so the epoll event loop is never blocked by cold disk reads.

#### Top-Level Configuration (`/etc/nginx/nginx.conf`):
```nginx
user nginx;
worker_processes auto;

# Define thread pool in the main context
# 32 background threads with a queue of 65,536 tasks
thread_pool default threads=32 max_queue=65536;

events {
    worker_connections 65536;
    use epoll;
}

http {
    include /etc/nginx/mime.types;
    include /etc/nginx/conf.d/*.conf;
}
```

#### Virtual Host Configuration (`/etc/nginx/conf.d/thread_pool.conf`):
```nginx
server {
    listen 80;
    server_name content.example.com;
    root /var/www/content;

    sendfile on;
    tcp_nopush on;
    tcp_nodelay on;

    location / {
        # Offload file reads to the default thread pool asynchronously!
        aio threads=default;

        # Cap chunk size to maintain thread responsiveness
        sendfile_max_chunk 1m;
        
        try_files $uri =404;
    }
}
```

### Example 4: Hybrid Small-Asset Cache vs Large-File Direct Streaming

A comprehensive multi-location architecture handling small API/HTML assets through RAM while streaming large binaries through Direct I/O.

```nginx
# /etc/nginx/conf.d/hybrid_delivery.conf

server {
    listen 80;
    server_name portal.example.com;

    # 1. High-Performance Static Web Assets (<1MB)
    location ~* \.(css|js|svg|png|jpg|woff2)$ {
        root /var/www/portal/dist;
        sendfile on;
        tcp_nopush on;
        tcp_nodelay on;
        expires 1y;
        add_header Cache-Control "public, immutable";
    }

    # 2. Large Artifact / Software Downloads (>10MB)
    location /artifacts/ {
        root /var/www/portal/storage;
        sendfile on;
        tcp_nopush on;

        # Direct I/O kicks in for artifacts > 4MB
        directio 4m;
        directio_alignment 4k;

        # Asynchronous thread pool offload
        aio threads=default;

        # Cap bandwidth per client socket
        limit_rate 2m;
    }
}
```

### Example 5: Vagrant / VirtualBox / NFS Shared Folder Workaround

Addressing the infamous VirtualBox shared folder caching bug that serves stale, truncated, or corrupted CSS/JS files during local development.

```nginx
# /etc/nginx/conf.d/vagrant_dev.conf

server {
    listen 80;
    server_name dev.local;
    root /vagrant/app/public;

    # VIRTUALBOX / DOCKER DESKTOP MOUNT FIX:
    # VirtualBox shared folders (vboxsf) and certain NFS mounts implement
    # a broken memory-mapping mmap() cache. When sendfile is enabled,
    # edited files return corrupted chunks, zero-byte responses, or 404s.
    sendfile off; # Mandatory on VirtualBox shared mounts!

    # Disable caching for local development
    expires -1;
    add_header Cache-Control "no-store, no-cache, must-revalidate" always;

    location / {
        try_files $uri $uri/ /index.html;
    }
}
```

---

## 6. Production Best Practices

1. **Always Enable the Trinity in Production**: On Linux production hosts, always configure `sendfile on;`, `tcp_nopush on;`, and `tcp_nodelay on;` in the `http {}` context.
2. **Always Configure `sendfile_max_chunk`**: On servers serving both small and large files, set `sendfile_max_chunk 512k;` or `1m;`. Without this limit, a single client downloading a 10GB file on a high-speed link can monopolize an NGINX worker process's write cycle, delaying other concurrent requests.
3. **Use `aio threads` for Spinning Disks or Heavy Random Reads**: If your storage is on spinning SAS HDDs, network-attached storage (NFS/EBS), or experiences heavy random reads, always enable `aio threads=default;`.
4. **Align `directio_alignment` with Physical Sector Sizes**: On modern NVMe SSDs and advanced format hard drives, sector sizes are 4096 bytes (4KB). Always set `directio_alignment 4k;` (default is 512 bytes) to avoid kernel misalignment penalties.
5. **Set `directio` Threshold to 4MB–8MB**: Never set `directio 0;` (which forces Direct I/O on everything). Small files (CSS, JS, icons) benefit immensely from the OS kernel page cache. Reserve Direct I/O strictly for files exceeding 4MB to 8MB.
6. **Remember `sendfile off;` in VirtualBox/Vagrant**: In local development environments using shared filesystem mounts (`vboxsf`), disable `sendfile` to prevent corrupted static file delivery.

---

## 7. Common Pitfalls & Anti-Patterns

| Anti-Pattern | Why It Fails | Correct Approach |
| :--- | :--- | :--- |
| **`sendfile on;` in VirtualBox shared folders** | VirtualBox host-guest filesystem sync does not update the guest page cache; browser receives truncated, empty, or outdated CSS/JS files. | Set `sendfile off;` inside local development Vagrant/VirtualBox environments. |
| **Omitting `sendfile_max_chunk`** | A multi-gigabyte file transfer blocks the NGINX worker thread in a continuous sendfile loop, starving other sockets on that worker. | Configure `sendfile_max_chunk 512k;` or `1m;`. |
| **Using `directio` for small files (<1MB)** | Bypasses OS page cache for small files; every single request for `logo.png` triggers physical disk read I/O, degrading performance. | Use `directio 8m;` so small files remain in memory page cache. |
| **`aio on;` on Linux without Direct I/O** | Native POSIX AIO on Linux (`io_submit`) only functions on files opened with `O_DIRECT`. Without `directio`, `aio on;` is silently ignored or blocks. | Use `aio threads;` (thread pools) instead of raw `aio on;`. |
| **Misaligned `directio_alignment`** | Direct I/O requires buffer and file offsets aligned to underlying disk sector boundaries (512 or 4096). Misalignment returns OS I/O errors. | Ensure `directio_alignment 4k;` matches SSD sector geometry. |
| **Using `tcp_nopush` without `sendfile`** | In NGINX, `tcp_nopush` requires `sendfile on;` to be activated; without sendfile, `tcp_nopush` is inert. | Always configure `sendfile on;` alongside `tcp_nopush on;`. |

---

## 8. Performance, Security & Troubleshooting

### Diagnostic CLI Commands

```bash
# 1. Verify sendfile System Call Execution via strace
# Attach to running NGINX worker process and filter for sendfile system calls:
strace -f -p $(pgrep -f "nginx: worker" | head -n 1) -e trace=sendfile,sendfile64
# Trigger static file download; verify output shows:
# sendfile64(socket_fd, disk_fd, [offset], count) = count

# 2. Inspect Direct I/O (Check if page cache is bypassed)
# Drop system page cache:
sync; echo 3 > /proc/sys/vm/drop_caches
# Download large file:
curl -s http://media.example.com/videos/movie.mp4 > /dev/null
# Check memory: page cache should NOT increase if directio was triggered!
free -m

# 3. Monitor Thread Pool Utilization
# (Available via NGINX Plus status or debugging logs)
tail -f /var/log/nginx/error.log | grep -i "thread"
```

### Context Switch Reduction
In benchmarks on a 40Gbps network link:
- Traditional `read/write`: Maxes out at ~12Gbps with 100% CPU utilization across all cores due to kernel context switches and memory copy bus saturation.
- `sendfile` + `tcp_nopush`: Saturates the full **40Gbps line rate** with less than 25% total CPU utilization.

---

## 9. Architectural Comparison Table

| Feature / Trait | Standard I/O (`sendfile off`) | Zero-Copy (`sendfile on`) | Direct I/O (`directio`) | AIO Thread Pools (`aio threads`) |
| :--- | :--- | :--- | :--- | :--- |
| **Data Path** | Disk $\rightarrow$ RAM $\rightarrow$ App $\rightarrow$ Socket | Disk $\rightarrow$ RAM $\rightarrow$ Socket | Disk $\rightarrow$ Socket (Direct) | Thread Pool $\rightarrow$ `sendfile()` |
| **User Space Copy**| **Yes** | **No** (Zero Copy) | **No** (Zero Copy) | **No** |
| **Page Cache Use** | Yes | Yes (Page cache dependent)| **Bypassed completely** | Yes |
| **Blocking Risk** | High | Low (if cached in RAM) | None | **Zero (Non-blocking)** |
| **Best Used For** | Dynamic filters, Vagrant mounts | Standard web assets, CSS, JS | Multi-gigabyte videos, ISOs | Cold disks, high random read load |

---

## 10. Technical Interview Questions

<details>
<summary><strong>Q1: Why does sendfile() provide massive performance gains compared to standard read() and write() system calls?</strong></summary>

**Answer:**
Under standard POSIX I/O:
1. `read()` copies file bytes from physical disk via DMA into the kernel page cache.
2. The CPU copies data from the kernel page cache into user-space application memory (NGINX process heap).
3. `write()` copies data from user-space application memory back into kernel socket buffer memory.
4. The CPU transfers data from the socket buffer to the Network Interface Card (NIC) via DMA.
This process requires **4 context switches** between user mode and kernel mode, and **4 data copies** (two of which saturate CPU memory buses).

With `sendfile()`:
1. NGINX calls `sendfile(socket_fd, file_fd, offset, length)` once.
2. The kernel copies data via DMA directly from disk into the page cache, and transfers descriptors directly to the NIC ring buffer.
Data never touches user-space memory, requiring **only 2 context switches and zero CPU memory copies** (zero-copy), reducing CPU overhead by over 70%.
</details>

<details>
<summary><strong>Q2: Explain how tcp_nopush and tcp_nodelay work together in NGINX.</strong></summary>

**Answer:**
At first glance, `tcp_nopush` (Linux `TCP_CORK`) and `tcp_nodelay` (disabling Nagle's algorithm) appear to be opposites:
- `tcp_nopush` forces the socket to withhold packets until it accumulates enough data to fill a full Maximum Segment Size (MSS) frame.
- `tcp_nodelay` forces the socket to send packets immediately without buffering.

In NGINX, when `sendfile on;` is active:
1. When a client requests a static file, NGINX uses `tcp_nopush on;` to pack the HTTP response headers and the initial chunk of the file into a single, fully-filled TCP packet, sending maximum data per frame.
2. Once the final chunk of the file is transferred, NGINX automatically removes the cork and uses `tcp_nodelay on;` to transmit the trailing partial packet instantly without waiting for the TCP ACK.
This combination provides maximum bandwidth efficiency and minimal latency.
</details>

<details>
<summary><strong>Q3: Why is sendfile_max_chunk critical when serving both small web pages and large file downloads?</strong></summary>

**Answer:**
By default, `sendfile()` attempts to stream as much data as possible in a single system call. 

If a client with a fast 10Gbps fiber connection downloads an 8GB file, calling `sendfile()` without a chunk cap causes the NGINX worker process to stay trapped in a tight kernel transmission loop for hundreds of milliseconds. Because NGINX uses an asynchronous single-threaded event loop per worker, that worker cannot process new incoming connection events or handle other active sockets until the sendfile block finishes.

Configuring `sendfile_max_chunk 512k;` (or `1m;`) forces NGINX to yield execution back to the `epoll` event loop after transmitting 512KB, ensuring fair scheduling across all concurrent client connections.
</details>

<details>
<summary><strong>Q4: What problem does directio solve, and what is "Page Cache Thrashing"?</strong></summary>

**Answer:**
The Linux kernel uses free system RAM as a page cache to store recently read disk blocks. This works brilliantly for web assets (HTML, CSS, JS, images) because frequently requested assets remain in RAM.

However, when a user streams or downloads a 10GB video file, the kernel reads the 10GB file into the page cache. Because RAM is finite, the kernel evicts hot web assets, database caches, and frequently accessed files to make room for the 10GB video. Once the video download completes, the 10GB in RAM is useless (rarely accessed again), while the evicted web assets now have to be re-read from slow disk, causing high system latency (**page cache thrashing**).

`directio 8m;` solves this by bypassing the page cache entirely for files $\ge 8\text{MB}$, streaming them directly from disk to network without evicting hot web assets from RAM.
</details>

<details>
<summary><strong>Q5: Why did NGINX introduce aio threads (thread pools) if it is already an asynchronous event-driven server?</strong></summary>

**Answer:**
While NGINX uses `epoll` for non-blocking network socket I/O, the Linux kernel **does not support non-blocking asynchronous file reads on standard buffered filesystems**. A standard disk read system call blocks the calling OS thread until the physical disk returns the data.

If an NGINX worker needs to read a file that is not in the kernel page cache (a "cold cache miss" on a slow disk), the entire single-threaded worker process freezes. While frozen, it cannot process any other client requests, even if those other requests are ready in memory.

With `aio threads=default;`, NGINX offloads the blocking disk read to a separate background worker thread pool. The main NGINX worker thread remains completely unblocked, immediately returning to the `epoll` loop to process other client events.
</details>

<details>
<summary><strong>Q6: Why must sendfile be turned off in Vagrant and VirtualBox environments?</strong></summary>

**Answer:**
In Vagrant and VirtualBox development environments, project source code is typically mounted from the host operating system into the Linux guest using VirtualBox shared folders (`vboxsf`) or certain NFS configurations.

The `vboxsf` driver implements a buggy, non-standard memory mapping architecture: when a file is modified on the host OS, VirtualBox fails to invalidate the page cache inside the guest Linux kernel. If `sendfile on;` is enabled, NGINX reads stale or misaligned memory pointers directly from the guest page cache, causing browsers to receive corrupted CSS/JS files, 0-byte responses, or 404 errors. Turning `sendfile off;` forces standard user-space reads, bypassing the driver bug.
</details>

<details>
<summary><strong>Q7: What is the purpose of directio_alignment?</strong></summary>

**Answer:**
When reading files with the `O_DIRECT` flag (Direct I/O), the operating system completely bypasses kernel memory buffers. As a result, the memory buffer addresses, file offsets, and read lengths must be strictly aligned to the physical sector boundary of the underlying block storage device.

Historically, hard drives used 512-byte physical sectors (`directio_alignment 512;`). Modern Advanced Format HDDs and NVMe SSDs use 4096-byte (4KB) physical sectors. If NGINX attempts a Direct I/O read that is misaligned with the storage geometry, the Linux kernel returns `EINVAL` (Invalid Argument) and the request fails. Setting `directio_alignment 4k;` ensures compatibility with modern SSD architectures.
</details>

<details>
<summary><strong>Q8: Does sendfile work over SSL/TLS connections?</strong></summary>

**Answer:**
Historically, `sendfile()` could not be used with HTTPS because data had to be pulled into user-space memory so OpenSSL could encrypt the plaintext bytes before sending them to the socket.

However, in modern Linux kernels (4.13+) with **Kernel TLS (kTLS)** enabled, the TLS encryption framing is handled directly inside the Linux kernel network stack. NGINX can use `sendfile()` over HTTPS, streaming plaintext from the page cache directly through kernel cryptographic acceleration (AES-NI) to the network card without entering user space.
</details>

---

## 11. Hands-On Practice Exercises

<details>
<summary><strong>Exercise 1: Configure the High-Throughput Trinity Stack</strong></summary>

**Goal:** Configure NGINX to serve static files from `/var/www/static` using `sendfile`, `tcp_nopush`, `tcp_nodelay`, and a 512KB chunk cap.

**Solution:**
```nginx
server {
    listen 80;
    server_name fast-static.local;
    root /var/www/static;

    sendfile on;
    tcp_nopush on;
    tcp_nodelay on;
    sendfile_max_chunk 512k;

    location / {
        try_files $uri =404;
    }
}
```
</details>

<details>
<summary><strong>Exercise 2: Setup Direct I/O for Files Exceeding 4MB</strong></summary>

**Goal:** Configure an endpoint that uses standard sendfile for small assets, but switches to Direct I/O with 4KB alignment for files larger than 4MB.

**Solution:**
```nginx
server {
    listen 80;
    server_name download.local;
    root /var/www/files;

    location / {
        sendfile on;
        tcp_nopush on;

        directio 4m;
        directio_alignment 4k;
    }
}
```
</details>

<details>
<summary><strong>Exercise 3: Configure Asynchronous Thread Pool Offloading</strong></summary>

**Goal:** Define a 16-thread pool in `nginx.conf` and apply it to a static location via `aio threads`.

**Solution:**
In `/etc/nginx/nginx.conf` (main context):
```nginx
thread_pool io_pool threads=16 max_queue=10000;
```

In virtual host:
```nginx
server {
    listen 80;
    server_name aio.local;
    root /var/www/data;

    location / {
        sendfile on;
        aio threads=io_pool;
        sendfile_max_chunk 1m;
    }
}
```
</details>

<details>
<summary><strong>Exercise 4: Verify sendfile Execution via strace</strong></summary>

**Goal:** Trace a running NGINX worker process to confirm that `sendfile64` is executed during file downloads.

**Solution:**
```bash
# Run in background:
strace -f -p $(pgrep -f "nginx: worker" | head -n 1) -e sendfile64 2>&1 | grep sendfile64 &
# Trigger download:
curl -s http://fast-static.local/image.png > /dev/null
# Verify output shows sendfile64(...) returning byte count.
```
</details>

<details>
<summary><strong>Exercise 5: Fix VirtualBox Shared Folder Glitch</strong></summary>

**Goal:** Identify and apply the fix for corrupted static files in a local development environment.

**Solution:**
```nginx
server {
    listen 80;
    server_name vagrant.local;
    root /vagrant/public;

    # Fix VirtualBox/Docker volume cache bug:
    sendfile off;

    expires -1;
}
```
</details>

<details>
<summary><strong>Exercise 6: Test HTTP Range Requests on Large Media Files</strong></summary>

**Goal:** Request a specific 100-byte slice of a file using curl and verify that NGINX returns HTTP 206 Partial Content.

**Solution:**
```bash
curl -i -H "Range: bytes=500-599" http://download.local/bigfile.bin
# Verify output starts with:
# HTTP/1.1 206 Partial Content
# Content-Range: bytes 500-599/...
```
</details>

---

## 12. Summary & Quick Reference

```nginx
# Production Static Delivery Cheat Sheet

# 1. Main Context: Thread Pool Definition
thread_pool default threads=32 max_queue=65536;

# 2. Server Context: Zero-Copy & Socket Optimization
server {
    listen 80;
    server_name cdn.example.com;
    root /var/www/cdn;

    # Core Trinity
    sendfile on;
    tcp_nopush on;
    tcp_nodelay on;

    # Yield control after 1MB to prevent worker monopolization
    sendfile_max_chunk 1m;

    # Bypass page cache for files > 8MB (Direct I/O)
    directio 8m;
    directio_alignment 4k;

    # Offload blocking disk reads to background thread pool
    aio threads=default;

    location / {
        expires 30d;
        add_header Cache-Control "public, immutable";
        try_files $uri =404;
    }
}
```

---

## 13. Further Reading & Official Documentation
- [Official NGINX ngx_http_core_module sendfile Documentation](https://nginx.org/en/docs/http/ngx_http_core_module.html#sendfile)
- [Thread Pools in NGINX Boost Performance 9x (NGINX Blog)](https://www.nginx.com/blog/thread-pools-boost-performance-9x/)
- [Linux man pages: sendfile(2)](https://man7.org/linux/man-pages/man2/sendfile.2.html)
- [Linux man pages: tcp(7) - TCP_CORK and TCP_NODELAY](https://man7.org/linux/man-pages/man7/tcp.7.html)

---

[← Previous: Circuit Breaking & Upstream Health Checks](08-circuit-breaking-and-upstream-health-checks.md) | [Module Overview](README.md) | [Next Module: Internals & Architecture →](../04-internals/01-architecture-master-worker-and-event-loop.md)
