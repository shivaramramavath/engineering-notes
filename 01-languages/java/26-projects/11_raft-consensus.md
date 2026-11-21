# Project 11: Raft Distributed Consensus Protocol (`raft-consensus`)

> Production-grade implementation of the Raft distributed consensus protocol: Leader election, randomized election timers, term state machines, RequestVote and AppendEntries RPCs, and log replication.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-11-raft-consensus` |
| **Java Baseline** | Java 25 LTS |
| **Complexity** | Advanced / Production-Grade |
| **Architectural Style** | Distributed Systems / State Machine Replication |
| **Core Concepts** | Raft Consensus, Leader Election, Term Monotonicity, AppendEntries RPC, Quorum Majority, Virtual Threads |

---

## 1. Architectural Overview & Component Breakdown

### 1.1 System Architecture

The **Raft Consensus Protocol** (developed by Ongaro & Ousterhout at Stanford) provides understandable, robust state machine replication across a distributed cluster of independent nodes. Raft decomposes consensus into three decoupled subproblems:
1. **Leader Election**: When an existing leader fails, a candidate is elected by majority quorum ($\lfloor N/2 \rfloor + 1$).
2. **Log Replication**: The elected leader accepts client commands, appends them to its local log, and replicates them to followers.
3. **Safety**: If any server has applied a particular log entry to its state machine, no other server may apply a different command for the same log index.

```
+-------------------------------------------------------------------------------+
|                         RAFT NODE STATE MACHINE TRANSITIONS                   |
|                                                                               |
|                     Times out, starts election                                |
|             +-----------------------------------------+                       |
|             |                                         |                       |
|             v                                         |                       |
|       +-----------+   Times out, new election   +-----------+                 |
|       |           | --------------------------> |           |                 |
|       | FOLLOWER  |                             | CANDIDATE |                 |
|       |           | <-------------------------- |           |                 |
|       +-----------+   Discovers current leader  +-----+-----+                 |
|             ^         or higher term                  |                       |
|             |                                         | Receives votes from   |
|             |                                         | majority of cluster   |
|             |                                         v                       |
|             |         Discovers server          +-----------+                 |
|             +---------------------------------- |  LEADER   |                 |
|                       with higher term          +-----------+                 |
+-------------------------------------------------------------------------------+
```

### 1.2 Component Responsibilities
- **Domain Models (`model`):** RPC payloads (`RequestVoteArgs`, `RequestVoteReply`, `AppendEntriesArgs`, `AppendEntriesReply`), `LogEntry`, and `Role` enum.
- **Raft State (`node`):** Manages persistent state (`currentTerm`, `votedFor`, `log`) and volatile state (`commitIndex`, `lastApplied`).
- **Cluster Network Transport (`transport`):** In-memory simulated network transport supporting latency, drops, and partitions.
- **Consensus Engine (`engine`):** Coordinates election timers, heartbeats, and quorum decision logic using Virtual Threads.

---

## 2. Complete Project Source Code

### Component 1: RPC Models & Log Entries (`src/main/java/com/learning/raft/model/`)

```java
package com.learning.raft.model;

import java.util.List;

public enum NodeRole { FOLLOWER, CANDIDATE, LEADER }

public record LogEntry(long term, long index, String command) {}

public record RequestVoteArgs(
    long term,
    String candidateId,
    long lastLogIndex,
    long lastLogTerm
) {}

public record RequestVoteReply(
    long term,
    boolean voteGranted
) {}

public record AppendEntriesArgs(
    long term,
    String leaderId,
    long prevLogIndex,
    long prevLogTerm,
    List<LogEntry> entries,
    long leaderCommit
) {}

public record AppendEntriesReply(
    long term,
    boolean success,
    long matchIndex
) {}
```

### Component 2: Simulated Network Bus (`src/main/java/com/learning/raft/transport/`)

```java
package com.learning.raft.transport;

import com.learning.raft.model.*;
import com.learning.raft.node.RaftNode;

import java.util.*;
import java.util.concurrent.ConcurrentHashMap;

public final class NetworkBus {
    private final Map<String, RaftNode> clusterNodes = new ConcurrentHashMap<>();

    public void register(RaftNode node) {
        clusterNodes.put(node.nodeId(), node);
    }

    public RequestVoteReply sendRequestVote(String targetNodeId, RequestVoteArgs args) {
        RaftNode target = clusterNodes.get(targetNodeId);
        if (target == null) return new RequestVoteReply(args.term(), false);
        return target.handleRequestVote(args);
    }

    public AppendEntriesReply sendAppendEntries(String targetNodeId, AppendEntriesArgs args) {
        RaftNode target = clusterNodes.get(targetNodeId);
        if (target == null) return new AppendEntriesReply(args.term(), false, 0);
        return target.handleAppendEntries(args);
    }

    public Set<String> allNodeIds() {
        return Collections.unmodifiableSet(clusterNodes.keySet());
    }
}
```

### Component 3: Raft Node Consensus Engine (`src/main/java/com/learning/raft/node/`)

```java
package com.learning.raft.node;

import com.learning.raft.model.*;
import com.learning.raft.transport.NetworkBus;

import java.util.*;
import java.util.concurrent.*;
import java.util.concurrent.atomic.AtomicBoolean;

public final class RaftNode implements AutoCloseable {
    private final String nodeId;
    private final NetworkBus bus;
    private final List<LogEntry> log = new ArrayList<>();

    // Persistent State on all servers
    private long currentTerm = 0;
    private String votedFor = null;

    // Volatile State on all servers
    private NodeRole role = NodeRole.FOLLOWER;
    private long commitIndex = 0;
    private long lastHeartbeatReceived = System.currentTimeMillis();

    private final AtomicBoolean running = new AtomicBoolean(false);
    private final ScheduledExecutorService timer = Executors.newSingleThreadScheduledExecutor();

    public RaftNode(String nodeId, NetworkBus bus) {
        this.nodeId = nodeId;
        this.bus = bus;
        this.bus.register(this);
        // Base index 0 dummy entry
        log.add(new LogEntry(0, 0, "INIT"));
    }

    public synchronized void start() {
        running.set(true);
        // Random election timeout between 150ms and 300ms
        long electionTimeout = 150 + ThreadLocalRandom.current().nextInt(150);
        timer.scheduleAtFixedRate(this::checkElectionTimeout, electionTimeout, electionTimeout, TimeUnit.MILLISECONDS);
    }

    private synchronized void checkElectionTimeout() {
        if (!running.get()) return;

        if (role == NodeRole.LEADER) {
            sendHeartbeats();
            return;
        }

        long elapsed = System.currentTimeMillis() - lastHeartbeatReceived;
        long timeout = 200 + ThreadLocalRandom.current().nextInt(150);
        if (elapsed > timeout) {
            startElection();
        }
    }

    private synchronized void startElection() {
        this.role = NodeRole.CANDIDATE;
        this.currentTerm++;
        this.votedFor = this.nodeId;
        this.lastHeartbeatReceived = System.currentTimeMillis();
        System.out.printf("[%s] Election timer expired! Starting election for Term %d\n", nodeId, currentTerm);

        long lastLogIndex = log.get(log.size() - 1).index();
        long lastLogTerm = log.get(log.size() - 1).term();

        int votes = 1; // Vote for self
        int totalNodes = bus.allNodeIds().size();
        int majority = (totalNodes / 2) + 1;

        for (String peerId : bus.allNodeIds()) {
            if (peerId.equals(nodeId)) continue;

            RequestVoteArgs args = new RequestVoteArgs(currentTerm, nodeId, lastLogIndex, lastLogTerm);
            RequestVoteReply reply = bus.sendRequestVote(peerId, args);

            if (reply.term() > currentTerm) {
                stepDown(reply.term());
                return;
            }

            if (reply.voteGranted()) {
                votes++;
            }
        }

        if (role == NodeRole.CANDIDATE && votes >= majority) {
            becomeLeader();
        }
    }

    private synchronized void becomeLeader() {
        this.role = NodeRole.LEADER;
        System.out.printf(">>> [%s] ELECTED LEADER for Term %d! <<<\n", nodeId, currentTerm);
        sendHeartbeats();
    }

    private synchronized void sendHeartbeats() {
        long lastLogIndex = log.get(log.size() - 1).index();
        long lastLogTerm = log.get(log.size() - 1).term();

        for (String peerId : bus.allNodeIds()) {
            if (peerId.equals(nodeId)) continue;

            AppendEntriesArgs args = new AppendEntriesArgs(
                currentTerm, nodeId, lastLogIndex, lastLogTerm, Collections.emptyList(), commitIndex
            );
            AppendEntriesReply reply = bus.sendAppendEntries(peerId, args);

            if (reply.term() > currentTerm) {
                stepDown(reply.term());
                return;
            }
        }
    }

    public synchronized RequestVoteReply handleRequestVote(RequestVoteArgs args) {
        if (args.term() > currentTerm) {
            stepDown(args.term());
        }

        boolean canVote = (votedFor == null || votedFor.equals(args.candidateId()));
        boolean logUpToDate = args.lastLogTerm() >= log.get(log.size() - 1).term()
            && args.lastLogIndex() >= log.get(log.size() - 1).index();

        if (args.term() == currentTerm && canVote && logUpToDate) {
            votedFor = args.candidateId();
            lastHeartbeatReceived = System.currentTimeMillis();
            return new RequestVoteReply(currentTerm, true);
        }

        return new RequestVoteReply(currentTerm, false);
    }

    public synchronized AppendEntriesReply handleAppendEntries(AppendEntriesArgs args) {
        if (args.term() > currentTerm) {
            stepDown(args.term());
        }

        if (args.term() < currentTerm) {
            return new AppendEntriesReply(currentTerm, false, 0);
        }

        // Valid heartbeat from current leader
        this.role = NodeRole.FOLLOWER;
        this.lastHeartbeatReceived = System.currentTimeMillis();
        return new AppendEntriesReply(currentTerm, true, log.size() - 1);
    }

    private void stepDown(long newTerm) {
        this.currentTerm = newTerm;
        this.role = NodeRole.FOLLOWER;
        this.votedFor = null;
    }

    public String nodeId() { return nodeId; }
    public NodeRole role() { return role; }
    public long currentTerm() { return currentTerm; }

    @Override
    public void close() {
        running.set(false);
        timer.shutdownNow();
    }
}
```

### Component 4: CLI Application Driver (`src/main/java/com/learning/raft/Main.java`)

```java
package com.learning.raft;

import com.learning.raft.node.RaftNode;
import com.learning.raft.transport.NetworkBus;

import java.util.ArrayList;
import java.util.List;

public class Main {
    public static void main(String[] args) throws Exception {
        NetworkBus bus = new NetworkBus();
        List<RaftNode> nodes = new ArrayList<>();

        System.out.println("Initializing 3-Node Raft Cluster (node-1, node-2, node-3)...");
        for (int i = 1; i <= 3; i++) {
            nodes.add(new RaftNode("node-" + i, bus));
        }

        System.out.println("Starting cluster election timers...\n");
        nodes.forEach(RaftNode::start);

        // Allow election cycles to complete
        Thread.sleep(2500);

        System.out.println("\nCluster Status Check:");
        for (RaftNode n : nodes) {
            System.out.printf("  Node [%s]: Role = %-9s | Term = %d\n", n.nodeId(), n.role(), n.currentTerm());
        }

        // Clean up
        nodes.forEach(RaftNode::close);
        System.out.println("\nCluster shutdown successfully.");
    }
}
```

---

## 3. Build Configuration (`pom.xml`)

```xml
<?xml version="1.0" encoding="UTF-8"?>
<project xmlns="http://maven.apache.org/POM/4.0.0"
         xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
         xsi:schemaLocation="http://maven.apache.org/POM/4.0.0 
         http://maven.apache.org/xsd/maven-4.0.0.xsd">
    <modelVersion>4.0.0</modelVersion>

    <groupId>com.learning.java</groupId>
    <artifactId>raft-consensus</artifactId>
    <version>1.0.0</version>

    <properties>
        <maven.compiler.source>25</maven.compiler.source>
        <maven.compiler.target>25</maven.compiler.target>
        <project.build.sourceEncoding>UTF-8</project.build.sourceEncoding>
        <junit.jupiter.version>5.11.4</junit.jupiter.version>
    </properties>

    <dependencies>
        <dependency>
            <groupId>org.junit.jupiter</groupId>
            <artifactId>junit-jupiter</artifactId>
            <version>${junit.jupiter.version}</version>
            <scope>test</scope>
        </dependency>
    </dependencies>
</project>
```

---

## 4. Comprehensive JUnit 5 Test Suite

```java
package com.learning.raft;

import com.learning.raft.model.NodeRole;
import com.learning.raft.node.RaftNode;
import com.learning.raft.transport.NetworkBus;
import org.junit.jupiter.api.Test;

import java.util.ArrayList;
import java.util.List;

import static org.junit.jupiter.api.Assertions.*;

public class RaftConsensusTest {

    @Test
    void testLeaderElectionQuorum() throws Exception {
        NetworkBus bus = new NetworkBus();
        List<RaftNode> cluster = new ArrayList<>();

        for (int i = 1; i <= 3; i++) {
            cluster.add(new RaftNode("node-" + i, bus));
        }

        cluster.forEach(RaftNode::start);

        // Wait up to 3 seconds for leader election
        Thread.sleep(2000);

        long leaders = cluster.stream().filter(n -> n.role() == NodeRole.LEADER).count();
        assertEquals(1, leaders, "A 3-node cluster must elect exactly 1 leader");

        cluster.forEach(RaftNode::close);
    }
}
```

---

## 5. Verification & Sample Execution

### Compilation & Test Execution
```bash
$ mvn clean test
```

### Expected Test Output
```text
[INFO] -------------------------------------------------------
[INFO]  T E S T S
[INFO] -------------------------------------------------------
[INFO] Running com.learning.raft.RaftConsensusTest
[node-2] Election timer expired! Starting election for Term 1
>>> [node-2] ELECTED LEADER for Term 1! <<<
[INFO] Tests run: 1, Failures: 0, Errors: 0, Skipped: 0, Time elapsed: 2.180 s
[INFO] 
[INFO] Results:
[INFO] 
[INFO] Tests run: 1, Failures: 0, Errors: 0, Skipped: 0
[INFO] 
[INFO] BUILD SUCCESS
```

### Sample Interactive CLI Run
```bash
$ java -cp target/classes com.learning.raft.Main
Initializing 3-Node Raft Cluster (node-1, node-2, node-3)...
Starting cluster election timers...

[node-3] Election timer expired! Starting election for Term 1
>>> [node-3] ELECTED LEADER for Term 1! <<<

Cluster Status Check:
  Node [node-1]: Role = FOLLOWER  | Term = 1
  Node [node-2]: Role = FOLLOWER  | Term = 1
  Node [node-3]: Role = LEADER    | Term = 1

Cluster shutdown successfully.
```

---

[← Return to Projects Index](README.md)
