# Troubleshooting & Emergency Incident Flowcharts: High-Density Cheatsheet

| Metadata | Details |
| :--- | :--- |
| **Topic** | Linux Production Emergency Runbooks & Triage Decision Flowcharts |
| **Module** | `09-cheatsheets` — High-Density Linux Production Cheatsheets |
| **Track** | `04-devops/linux` |
| **Target Level**| SRE / On-Call Incident Responders |
| **Last Updated**| 2026 |

---

## 1. Incident 1: High CPU Saturation & Host Sluggishness

```
                                  [ Incident: High CPU Load Alert ]
                                                  |
                                                  v
                                     Execute: uptime && mpstat 1 3
                                                  |
                     +----------------------------+----------------------------+
                     |                                                         |
                     v                                                         v
        [ %usr or %sys is High (> 85%) ]                         [ %iowait is High (> 20%) ]
                     |                                                         |
                     v                                                         v
         Execute: pidstat -u 1 5                                   Execute: iostat -xz 1 3
                     |                                                         |
         Identify Top Consuming PID                                Is await > 15ms or %util == 100%?
                     |                                             +-----------+-----------+
         +-----------+-----------+                                 |                       |
         |                       |                                 v [ YES ]               v [ NO ]
         v                       v                        [ Disk I/O Saturated ]    [ Stuck on NFS / Lock ]
   [ Single Thread ]      [ Multi-Threaded ]                       |                       |
   Hotspot Loop           Contention / GC                          v                       v
         |                       |                        Run: iotop -oPa        Check: cat /proc/<pid>/wchan
         v                       v                        Find I/O Hog PID       Unmount hung network mount
   perf top -p <PID>      Profile via eBPF                Adjust ionice -c 3
```

### Emergency Commands
```bash
uptime                                      # Verify 1, 5, 15 min load vs core count
mpstat -P ALL 1 2                           # Check CPU distribution across cores
pidstat -u 1 5                              # Identify CPU hog process ID
sudo perf top -p <PID>                      # Identify function burning instructions
sudo renice +10 -p <PID>                    # Lower priority immediately to restore SSH control
```

---

## 2. Incident 2: Out-Of-Memory (OOM) & Swap Thrashing

```
                                  [ Incident: Memory Exhaustion / OOM ]
                                                  |
                                                  v
                                       Execute: free -h && vmstat 1 3
                                                  |
                     +----------------------------+----------------------------+
                     |                                                         |
                     v                                                         v
      [ MemAvailable < 5% ]                                     [ Swap "si" / "so" > 0 continuously ]
                     |                                                         |
                     v                                                         v
         Check: dmesg -T | grep -i oom                            [ High Swap Thrashing ]
         Has kernel OOM-killer executed?                                       |
         +-----------+-----------+                                             v
         |                       |                                 Identify Memory Hog:
         v [ YES ]               v [ NO ]                          ps -eo pid,ppid,cmd,%mem --sort=-%mem
   [ Process Terminated ]  [ Direct Compaction ]                               |
         |                       |                                             v
   Identify Killed PID     Check: grep -i compact /proc/vmstat     Restart / Isolate Leaking Process
   Verify cgroup limits    Disable THP (Transparent Huge Pages)   Add Emergency Swapfile (if disk space exists)
```

### Emergency Commands
```bash
free -h                                     # Check MemAvailable vs MemTotal
dmesg -T | grep -E -i "oom|killed process"  # Verify OOM killer invocations
ps -eo pid,user,%mem,rss,args --sort=-rss | head -n 10 # Top 10 RAM consumers
# Emergency: Disable Transparent Huge Pages live to stop direct compaction freeze:
echo never | sudo tee /sys/kernel/mm/transparent_hugepage/enabled
# Create 2GB Emergency Swapfile:
sudo fallocate -l 2G /swapfile_emergency && sudo chmod 600 /swapfile_emergency
sudo mkswap /swapfile_emergency && sudo swapon /swapfile_emergency
```

---

## 3. Incident 3: "No Space Left on Device" Discrepancy

```
                             [ Incident: "No space left on device" ]
                                                |
                                                v
                                         Execute: df -h
                                                |
                   +----------------------------+----------------------------+
                   |                                                         |
                   v [ Capacity is 100% ]                                    v [ Capacity is NOT Full ]
         Execute: du -sh /*                                                  |
                   |                                                         v
       Does du account for space?                                     Execute: df -i
       +-----------+-----------+                                             |
       |                       |                                             v
       v [ YES ]               v [ NO (Discrepancy!) ]               Is Inodes at 100%?
 [ Real Large File ]     [ Unlinked Open File Descriptor ]           +-------+-------+
       |                       |                                     |               |
 Purge old logs          Execute: sudo lsof +L1                      v [ YES ]       v [ NO ]
 Clean apt archives      Find holding PID and FD             [ Inode Exhaustion ] [ Read-Only Filesystem ]
                         Truncate: : > /proc/<PID>/fd/<FD>           |               |
                                                             Locate directory with   Check dmesg for I/O errors
                                                             millions of small files Remount rw: mount -o remount,rw /
```

### Emergency Commands
```bash
df -h                                       # Check disk capacity percentage
df -ih                                      # Check filesystem inode percentage (df -i)
sudo lsof +L1                               # Find unlinked files held open by running processes
sudo truncate -s 0 /proc/<PID>/fd/<FD>      # ZERO-DOWNTIME disk space reclamation!
# Locate Inode Hogs:
sudo find / -xdev -printf '%h\n' 2>/dev/null | sort | uniq -c | sort -rn | head -n 10
```

---

## 4. Incident 4: Network Connection Drops & Port Freezes

```
                             [ Incident: Network Connection Failures ]
                                                |
                                                v
                                  Execute: ss -tulpn && ping -c 3 <GW>
                                                |
                   +----------------------------+----------------------------+
                   |                                                         |
                   v [ Port is Listening ]                                   v [ Gateway Unreachable ]
         Execute: ss -lnt '( sport = :PORT )'                                |
         Compare Recv-Q vs Send-Q                                            v
                   |                                                Check: ip route show
       Is Recv-Q == Send-Q?                                         Verify physical cable / veth link:
       +-----------+-----------+                                    ip link show eth0
       |                       |
       v [ YES ]               v [ NO ]
 [ Accept Queue Full! ]  [ Connection Reaching Host ]
       |                       |
 Application thread hung       Check Firewall: sudo nft list ruleset || sudo ufw status
 or backlog too small          Check Conntrack: dmesg -T | grep nf_conntrack
 Enlarge: somaxconn            Check SYN Drops: netstat -s | grep "listen drops"
```

### Emergency Commands
```bash
ss -tulpn                                   # Confirm process is actually bound and listening
ss -lnt                                     # Inspect Accept Queue overflow (Recv-Q == Send-Q)
netstat -s | grep -i "listen drops"         # Incrementing drops indicate application is too slow
dmesg -T | grep -i "conntrack"              # Check if conntrack table is full
ip -s link show eth0                        # Check hardware drops and RX/TX errors
```

---
*Next Cheatsheet: [08-kernel-and-sysctl-quick-ref.md](file:///04-devops/linux/09-cheatsheets/08-kernel-and-sysctl-quick-ref.md)*
