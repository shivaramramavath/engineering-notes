# Advanced Debugging

Logs and metrics tell you *that* something is wrong. When they don't tell you *why*, you observe the program from the outside: the system calls it makes, the files and sockets it holds, the packets it sends, and where its CPU time goes. This note covers the four workhorses: **`strace`**, **`lsof`**, **`tcpdump`**, and **`perf`**, plus how to choose between them.

Prerequisites: [Troubleshooting](../05-production/02-troubleshooting.md) (use the cheap checks first), [Processes and Signals](../03-system/01-processes-and-signals.md), [Networking Basics](../04-networking/01-networking-basics.md), and [Namespaces and cgroups](./01-namespaces-and-cgroups.md) for containers.

## Which tool answers which question

| Question | Tool |
|---|---|
| What is it doing right now? Why does it hang? | `strace -p` |
| Which file/config does it open? Where does "Permission denied" come from? | `strace -e trace=file` |
| Who holds this file, port, or mount? Is it leaking descriptors? | `lsof` |
| Are packets arriving? Who is it talking to? Where does the handshake fail? | `tcpdump` |
| Where is the CPU time going? | `perf top`, `perf record` |

Start with the cheapest and least intrusive: logs, `ss`, `ps`, `/proc`, then these.

## Before you attach to anything

- **These tools can be powerful and intrusive.** `strace` slows the target (it stops it at every syscall), which is risky on a busy production process. Filter narrowly, trace briefly, and prefer `perf` or a replica when load matters.
- **Output may contain secrets.** A trace or packet capture can include passwords, tokens, and personal data. Only capture on systems you're authorized to debug, and handle the files like credentials.
- **You need privileges.** Attaching to another user's process requires root. On Ubuntu, `kernel.yama.ptrace_scope=1` means non-root users can only trace their own children, so use `sudo`.

```bash
sudo apt install strace lsof tcpdump
sudo apt install linux-tools-common linux-tools-generic   # perf on Ubuntu; the package is "perf" on RHEL
```

## strace: watch the system calls

A **system call** is how a program asks the kernel to do something: open a file, read a socket, create a process. Since almost everything interesting goes through one, strace shows what a program is *really* doing, whatever language it's written in.

```bash
strace ls /nonexistent                      # run a command under strace
sudo strace -f -p 1234                      # attach to a running process (Ctrl+C detaches; the process continues)
strace -f -o trace.txt ./myapp              # write to a file, follow child processes/threads
```

Always add **`-f`** unless you know there's a single thread: servers, shells, and wrappers fork or use threads, and without `-f` you'll trace only the parent and see nothing useful.

Useful options:

| Flag | Effect |
|---|---|
| `-f` | follow forks and threads |
| `-o file` | write output to a file |
| `-e trace=openat,connect` | only these syscalls |
| `-e trace=file` / `network` / `process` | a class of syscalls |
| `-s 200` | show up to 200 chars of strings (default 32 truncates a lot) |
| `-tt` | wall-clock timestamps |
| `-T` | time spent *inside* each syscall |
| `-y` / `-yy` | show the file path (or socket address) behind each fd |
| `-c` | summary table of counts and time per syscall |

### Reading the output

```text
openat(AT_FDCWD, "/etc/myapp/config.yml", O_RDONLY) = -1 ENOENT (No such file or directory)
openat(AT_FDCWD, "/home/app/.myapprc", O_RDONLY)    = 3
read(3, "port: 8080\n", 4096)                       = 11
connect(5, {sa_family=AF_INET, sin_port=htons(5432), sin_addr=inet_addr("10.0.1.5")}, 16) = -1 ECONNREFUSED
```

Format: `syscall(arguments) = return value`. A negative return comes with an **errno name**, which is your clue:

| errno | Meaning |
|---|---|
| `ENOENT` | no such file or directory |
| `EACCES` / `EPERM` | permission denied / operation not permitted |
| `EADDRINUSE` | port already in use |
| `ECONNREFUSED` | nothing listening at that address |
| `ETIMEDOUT` | connection timed out |
| `EMFILE` | process hit its file-descriptor limit |
| `EAGAIN` | try again; **normal** for non-blocking I/O, not an error by itself |

Many `ENOENT` lines are normal: programs probe several locations (library search paths, config fallbacks) until one exists. Look at the **last** attempt and what happens right after, not the count.

### Common recipes

```bash
# Which config file does it actually read, and in what order does it look?
strace -f -e trace=openat,stat,newfstatat ./myapp 2>&1 | grep -i config

# Where is "Permission denied" coming from?
strace -f -e trace=file ./myapp 2>&1 | grep -E 'EACCES|EPERM'

# Why is it hung? Attach and see the blocked call; -y shows what the fd is.
sudo strace -f -y -p 1234
#   read(7<TCP:[10.0.0.15:41234->10.0.1.5:5432]>, ...   ← waiting on the database
#   futex(0x7f..., FUTEX_WAIT, ...)                     ← waiting on a lock/condition
#   epoll_wait(...)                                     ← idle event loop (normal for Node)

# Which hosts and ports does it connect to?
strace -f -e trace=connect -yy ./myapp 2>&1 | grep connect

# Where does the time go? (slowest syscalls)
strace -f -T -o trace.txt ./myapp && sort -t'<' -k2 -rn trace.txt | head

# Summary: which syscalls dominate?
sudo strace -c -f -p 1234        # let it run a while, Ctrl+C for the table
```

A high-level runtime like Node or Python will show lots of `epoll_wait`/`futex`: that's the runtime idling, not the problem. strace shows syscalls, **not** your function calls or library calls. If strace shows nothing wrong, the time is probably spent in user-space code, which is `perf`'s job.

## lsof: who has what open

On Unix everything is a file, and `lsof` ("list open files") shows regular files, directories, sockets, pipes, and devices open by each process. Use `-n -P` to skip DNS and port-name lookups (faster and clearer).

```bash
sudo lsof -p 1234                          # everything process 1234 has open
sudo lsof -i :3000                         # who uses port 3000
sudo lsof -iTCP -sTCP:LISTEN -nP           # all listening TCP sockets
sudo lsof -i @10.0.1.5                     # connections to a host
sudo lsof -u alice                         # files opened by a user
sudo lsof -c nginx                         # by command name
sudo lsof /var/log/app.log                 # who has this file open
sudo lsof +D /mnt/data                     # who is using anything under a directory (recursive, slow)
sudo lsof +L1                              # open files with zero links: deleted but still held
```

Typical uses:

- **Why won't it unmount?** `lsof +D /mnt/data` (or `fuser -vm /mnt/data`) finds the process using it.
- **Disk is full but `du` disagrees:** `lsof +L1` finds deleted-but-open files ([storage](../03-system/05-storage-and-filesystems.md)).
- **Port in use:** `lsof -i :PORT` names the owner.
- **File descriptor leak:** count and categorize descriptors over time:

```bash
sudo lsof -p 1234 | wc -l
sudo lsof -p 1234 | awk 'NR>1 {print $5}' | sort | uniq -c | sort -nr    # by type: REG, IPv4, unix, FIFO…
ls /proc/1234/fd | wc -l            # same count without lsof
```

A steadily growing count of `IPv4` entries (often in `CLOSE_WAIT`) means connections aren't being closed. Compare with the limit in `/proc/1234/limits` ([performance](../05-production/03-performance.md)).

## tcpdump: look at the packets

When logs say "connection timed out" or "reset" and you need to know what really crossed the wire, capture packets.

```bash
sudo tcpdump -ni any port 3000              # -n no name lookups, -i any interface
sudo tcpdump -ni eth0 host 203.0.113.5      # traffic to/from one host
sudo tcpdump -ni any udp port 53            # DNS queries and answers
sudo tcpdump -ni lo port 5432               # local traffic (loopback)
sudo tcpdump -ni any -c 20 port 80          # stop after 20 packets
sudo tcpdump -ni any -A port 80             # show payload as ASCII (plain HTTP only)
sudo tcpdump -ni eth0 -w capture.pcap port 443   # save to a file
tcpdump -nr capture.pcap                    # read it back (or open in Wireshark / tshark)
```

Filters use BPF syntax and combine with `and`, `or`, `not`: `host 10.0.0.5 and tcp port 443`, `net 10.0.0.0/24`, `src 1.2.3.4`, `not port 22` (hide your own SSH session). Always use a filter and `-n` on busy machines; capturing everything is slow and may drop packets (the exit summary shows "packets dropped by kernel").

### Reading TCP flags

```text
14:02:11.101 IP 203.0.113.5.51512 > 10.0.0.15.3000: Flags [S],  seq 100, length 0     SYN
14:02:11.101 IP 10.0.0.15.3000 > 203.0.113.5.51512: Flags [S.], seq 500, ack 101      SYN-ACK
14:02:11.102 IP 203.0.113.5.51512 > 10.0.0.15.3000: Flags [.],  ack 501                ACK
```

| Flag | Meaning |
|---|---|
| `[S]` | SYN: connection request |
| `[S.]` | SYN-ACK: server accepts |
| `[.]` | ACK |
| `[P.]` | data pushed (with ACK) |
| `[F.]` | FIN: graceful close |
| `[R]` / `[R.]` | RST: connection refused or aborted |

This turns vague errors into diagnoses:

| What you see | Conclusion |
|---|---|
| Repeated `[S]` from the client, **no reply** | packets dropped: firewall, cloud security group, or routing. If the SYN never even appears on the server, the block is *upstream* |
| `[S]` answered immediately by `[R.]` | nothing is listening (or a REJECT rule): "connection refused" |
| Handshake completes, then `[R]` mid-conversation | the app or a proxy aborted the connection |
| `[S.]` returns but the client never ACKs | return path problem (asymmetric routing, firewall) |
| Long gap between request and response | the app/backend is slow, not the network |
| Many retransmissions | packet loss on the path |

Combine with the firewall note: if the SYN arrives on the server and the server sends **no** reply, suspect the host firewall or app; if it never arrives, look upstream ([firewall](../04-networking/03-firewall.md)).

Handy SYN-only filter: `sudo tcpdump -ni any 'tcp[tcpflags] & (tcp-syn|tcp-ack) == tcp-syn'`.

### Limits and care

- **TLS payloads are encrypted**, so for HTTPS you see handshakes, sizes, and timing, not content. That's often enough to locate a problem.
- **Captures contain sensitive data**; restrict who can read `.pcap` files and delete them when done.
- To keep a long capture from filling the disk, rotate: `-C 100 -W 5` (five files of 100 MB, overwriting the oldest).
- For a container, capture **inside its network namespace** with `nsenter` ([previous note](./01-namespaces-and-cgroups.md)), or capture on `docker0`/the container's veth on the host:

```bash
PID=$(docker inspect -f '{{.State.Pid}}' mycontainer)
sudo nsenter -t "$PID" -n tcpdump -ni eth0 port 5432
```

## perf: where does the CPU time go?

`perf` samples what the CPU is executing, so it shows hot functions without modifying the program. Use it when `top` says a process is CPU-bound and you need to know *where*.

```bash
sudo perf top                                   # live view of the hottest functions system-wide
sudo perf top -p 1234                           # for one process
sudo perf stat -p 1234 -- sleep 10              # counters: task-clock, context-switches, page-faults, cycles…
sudo perf record -F 99 -g -p 1234 -- sleep 30   # sample 99 times/sec for 30s, with call stacks
sudo perf report --stdio | head -50             # summarize recorded samples
```

`-F 99` samples at 99 Hz (deliberately not 100, to avoid lock-step with timers); `-g` records call graphs so you can see *who called* the hot function. The output can be turned into a **flame graph**, where wide boxes are where time goes.

Practical caveats:

- **Symbols**: you may see raw hex addresses unless the binary has symbols (install the package's debug symbols, or `-dbgsym`/`debuginfo` packages).
- **Stack traces** can be truncated if binaries are built without frame pointers; try `--call-graph dwarf` (heavier).
- **JIT runtimes need help.** For Node.js, start with `node --perf-basic-prof server.js` so perf can map JIT-compiled code to function names.
- **Virtual machines** often lack hardware performance counters, so `cycles` shows `<not supported>`. Fall back to software sampling: `perf record -e cpu-clock ...`.
- Access can be restricted by `kernel.perf_event_paranoid`; running with `sudo` avoids that.

`perf trace` is a lower-overhead cousin of strace for syscalls.

## Beyond: eBPF and core dumps

- **eBPF tools** (`bcc-tools`, `bpftrace`) can trace file opens, process execs, disk latency, and more with very low overhead, safe even on busy production hosts. A taste, showing every file opened and by which command:

```bash
sudo bpftrace -e 'tracepoint:syscalls:sys_enter_openat { printf("%s %s\n", comm, str(args->filename)); }'
```

- **Crashes**: systemd can keep core dumps; inspect them with `coredumpctl list`, `coredumpctl info <pid>`, and `coredumpctl gdb <pid>` for a backtrace. Check `ulimit -c` and the core handler if none are saved.

## Debugging a containerized process

Container images often have none of these tools, and containers typically lack the `CAP_SYS_PTRACE` capability that strace needs. Run the tools **from the host** against the container's host-visible PID:

```bash
PID=$(docker inspect -f '{{.State.Pid}}' mycontainer)
sudo strace -f -p "$PID"          # syscalls
sudo lsof -p "$PID"               # open files
sudo nsenter -t "$PID" -n ss -tlnp   # its sockets, with host tools
sudo perf record -F 99 -g -p "$PID" -- sleep 30
```

Adding `--cap-add SYS_PTRACE` to a container is possible but widens its privileges; don't leave it on in production.

## Worked workflows

**"The app hangs and never responds."** `ps -o pid,stat,wchan:20,cmd -p PID` (is it `D` state?) → `sudo strace -f -y -p PID` (what call is it blocked in, and on which fd?) → `lsof`/`ss` on that fd → `tcpdump` against the peer to see if it's replying at all.

**"It can't find its config / Permission denied somewhere."** `strace -f -e trace=file ./myapp 2>&1 | grep -E 'ENOENT|EACCES'` and read the last attempts.

**"One endpoint is slow."** `curl -w` timings to see which phase → `strace -f -T` to find slow syscalls (a slow `connect` or `read` points at a dependency) → `perf record` if the time is CPU in user space → `tcpdump` to compare network time vs server think-time.

**"Something is making strange connections."** `ss -tnp` and `lsof -i` for who → `tcpdump -ni any host X` for what → `strace -f -e trace=connect -p PID` for where in the program.

**"CPU is pinned."** `top -H` for the hot thread → `perf top -p PID` → `perf record -g` and a flame graph.

## Common mistakes

- **Running strace without `-f`** on a server and tracing only a silent parent.
- **Tracing a production hot path with no filter**, slowing it to a crawl.
- **Treating every `ENOENT` or `EAGAIN` as a bug.**
- **`tcpdump` without `-n` and a filter**, which floods the screen and drops packets.
- **Forgetting HTTPS payloads are encrypted** and trying to read them.
- **Leaving captures or traces around** with credentials inside.
- **`perf` with no symbols**, then giving up on a screen of hex.
- **Using these tools first**, before the basic checks (`ss`, logs, `/proc`, `journalctl`).
- **Inspecting a container from inside** when the host has better tools.
- **Concluding from strace that the application is fine** when the time is in user-space computation; switch to `perf`.

## Quick Summary

- Pick by question: `strace` (what is it asking the kernel?), `lsof` (what does it hold open?), `tcpdump` (what's on the wire?), `perf` (where is the CPU time?).
- strace: use `-f`, filter with `-e trace=`, add `-y`/`-T`/`-c`; read the errno; ignore normal `ENOENT`/`EAGAIN`.
- lsof: owners of ports and files, deleted-but-open files (`+L1`), descriptor leaks.
- tcpdump: filter and use `-n`; SYN with no reply = dropped, SYN then RST = refused; use `-w` and Wireshark for deep dives.
- perf: `record -F 99 -g` + `report` or a flame graph; mind symbols, frame pointers, JIT, and VMs.
- For containers, debug from the host with the container's PID or `nsenter`; treat all captures as sensitive.

**Next:** [Projects](../07-projects/README.md)