# Queue Scaling, Performance, and Monitoring

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 05-advanced / 02-background-processing
- **Prerequisites**: [02-bullmq](./02-bullmq.md), [03-producers-and-consumers](./03-producers-and-consumers.md)

---

## 1. Context & Purpose

As application traffic grows from thousands to millions of background jobs per day, queue infrastructure faces critical performance hurdles: Redis CPU saturation, worker thread starvation, unmonitored queue lag, and inefficient horizontal autoscaling.

Scaling background workers requires understanding how to balance **intra-process concurrency** (running multiple asynchronous jobs per Node process), **inter-process sandboxing** (isolating CPU-heavy tasks into separate child processes), **queue metrics instrumentation** (Prometheus / Grafana), and **Kubernetes autoscaling** (KEDA based on queue depth).

---

## 2. Theoretical Foundations

### 2.1 Concurrency vs Sandboxed Workers
- **Standard Concurrency (`concurrency: N`)**: Executes $N$ jobs concurrently inside the same Node.js event loop. Highly efficient for I/O-bound jobs (database queries, external HTTP calls). Ineffective for CPU-heavy tasks because a single synchronous computation blocks all $N$ jobs.
- **Sandboxed Workers (`WorkerHost` with Child Processes)**: Spawns independent Node.js processes for worker execution. Crashes or heavy computations in one worker cannot block or crash the parent process.

### 2.2 Queue Lag Metric
The critical metric for autoscaling workers is **Queue Lag** (or Latency):
$$\text{Lag} = \text{Time Job Picked Up} - \text{Time Job Enqueued}$$
If Lag is increasing, worker capacity is insufficient and new worker pods must be spawned.

---

## 3. Architecture & Internal Mechanics

```
┌────────────────────────────────────────────────────────┐
│                   Redis 7.x Cluster                    │
│                 Queue: `transcode-video`               │
└───────────────────────────┬────────────────────────────┘
                            │
               Queue Depth Metric Reported
                            │
                            ▼
┌────────────────────────────────────────────────────────┐
│               KEDA (Kubernetes Autoscaler)             │
│   Target: 50 waiting jobs per worker pod               │
└───────────────────────────┬────────────────────────────┘
                            │
              Scales Deployments (Pods 1 ... N)
                            │
 ┌──────────────────────────┼───────────────────────────┐
 │                          │                           │
 ▼                          ▼                           ▼
Worker Pod 1               Worker Pod 2                Worker Pod N
(4 Child Processes)        (4 Child Processes)         (4 Child Processes)
```

---

## 4. Concrete Implementation

### 4.1 Sandboxed Child-Process Worker Setup

For CPU-heavy tasks (image resizing, video transcoding, crypto hashing), run the worker inside an isolated sandboxed file:

#### Step 1: Create the Sandboxed Processor Script

```typescript
// src/media/processors/transcode.sandbox.ts
import { SandboxedJob } from 'bullmq';

export default async function (job: SandboxedJob<{ videoPath: string; resolution: string }>) {
  console.log(`[Child Process PID ${process.pid}] Transcoding video: ${job.data.videoPath}`);

  // Heavy CPU work executes in this child process without blocking the main event loop
  await new Promise((resolve) => setTimeout(resolve, 3000));

  return {
    outputUrl: `https://s3.amazonaws.com/cdn/${job.id}.mp4`,
    processedByPid: process.pid,
  };
}
```

#### Step 2: Register in Processor Class

```typescript
// src/media/media.processor.ts
import { Processor, WorkerHost } from '@nestjs/bullmq';
import { join } from 'path';

@Processor('transcode-video', {
  concurrency: 4, // 4 isolated child processes running simultaneously
  useWorkerThreads: true, // Uses Worker Threads for lower memory overhead
})
export class MediaProcessor extends WorkerHost {
  // Point to the sandboxed file path
  process = join(__dirname, 'processors', 'transcode.sandbox.js');
}
```

---

### 4.2 Prometheus Metrics Exporter for Queue Health

Export queue depth, active jobs, and completion rates to Prometheus:

```typescript
// src/metrics/queue-metrics.service.ts
import { Injectable, OnModuleInit, Logger } from '@nestjs/common';
import { InjectQueue } from '@nestjs/bullmq';
import { Queue } from 'bullmq';

@Injectable()
export class QueueMetricsService implements OnModuleInit {
  private readonly logger = new Logger(QueueMetricsService.name);

  constructor(@InjectQueue('transcode-video') private readonly queue: Queue) {}

  onModuleInit() {
    // Poll queue metrics every 15 seconds
    setInterval(async () => {
      await this.collectMetrics();
    }, 15000);
  }

  async collectMetrics() {
    const counts = await this.queue.getJobCounts(
      'waiting',
      'active',
      'completed',
      'failed',
      'delayed',
    );

    this.logger.log(
      `[Queue Metrics] Waiting: ${counts.waiting} | Active: ${counts.active} | Failed: ${counts.failed} | Delayed: ${counts.delayed}`,
    );

    // Export to Prometheus gauge:
    // waitingGauge.set(counts.waiting);
    // activeGauge.set(counts.active);
  }
}
```

---

### 4.3 Kubernetes Autoscaling Configuration (KEDA)

Autoscale worker pods based directly on Redis queue length using KEDA (Kubernetes Event-driven Autoscaling):

```yaml
# deploy/keda-scaledobject.yaml
apiVersion: keda.sh/v1alpha1
kind: ScaledObject
metadata:
  name: video-worker-scaler
  namespace: default
spec:
  scaleTargetRef:
    name: video-worker-deployment
  minReplicaCount: 2
  maxReplicaCount: 20
  cooldownPeriod: 300
  triggers:
    - type: redis
      metadata:
        address: redis-service:6379
        listName: bull:transcode-video:wait
        listLength: "50" # Scale up 1 pod for every 50 waiting jobs
        enableTLS: "false"
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Memory Leaks in Sandboxed Workers
Child processes can accumulate memory leaks if native C++ bindings (e.g. Sharp or FFmpeg) fail to release memory buffers.
- **Handling**: In BullMQ, configure `maxJobsPerWorker: 100`. BullMQ automatically restarts the child process after 100 jobs, flushing all V8 heap memory cleanly.

### 5.2 Redis Connection Exhaustion on High Concurrency
If 50 worker pods each set `concurrency: 50`, thousands of active connections attempt to read from Redis.
- **Handling**: Use connection pooling and configure `max_connections` on the Redis server appropriately.

---

## 6. Performance & Memory Implications

| Architecture | Throughput (I/O Tasks) | CPU Saturation (CPU Tasks) | Memory Per Worker |
|---|---|---|---|
| **Standard Async Worker** | High (5,000+ jobs/s) | ⚠️ Blocks entire event loop | ~60 MB |
| **Worker Threads** | Moderate (1,000 jobs/s) | ✅ Runs on separate V8 thread | ~90 MB |
| **Child Process Sandbox** | Moderate (800 jobs/s) | ✅ Isolated OS process | ~120 MB |

---

## 7. Anti-Patterns & Common Mistakes

- **Autoscaling Workers Based on CPU Utilization**: Using standard Kubernetes HPA with CPU triggers. When queue depth spikes with I/O jobs, CPU remains low, so pods never scale up! Always scale on **Queue Depth** via KEDA.
- **Running CPU Transcoding in the Main Thread**: Resizing 4K images on the main NestJS process thread, causing all HTTP endpoints on that pod to drop incoming TCP connections.
- **Omitting `removeOnComplete`**: Leaving millions of completed job records in Redis until Redis runs out of memory.

---

## 8. Production Checklist & Operational Readiness

- [ ] CPU-heavy workloads run in sandboxed child processes or Worker Threads.
- [ ] KEDA ScaledObject configured for queue-depth autoscaling.
- [ ] Queue metrics (`waiting`, `active`, `failed`) exported to Prometheus.
- [ ] Max memory eviction policies configured on Redis cluster.
- [ ] Graceful termination period (`terminationGracePeriodSeconds: 60`) configured in Kubernetes to permit jobs to finish cleanly.

---

## 9. Migration & Version History

- **BullMQ 4.x/5.x**: Added native `useWorkerThreads: true` option for lower memory overhead in sandboxed execution.

---

## 10. Verification & Testing

Testing queue metrics collection:

```typescript
import { Test, TestingModule } from '@nestjs/testing';
import { getQueueToken } from '@nestjs/bullmq';
import { QueueMetricsService } from './queue-metrics.service';

describe('QueueMetricsService', () => {
  let service: QueueMetricsService;
  let mockQueue: any;

  beforeEach(async () => {
    mockQueue = {
      getJobCounts: vi.fn().mockResolvedValue({
        waiting: 15,
        active: 3,
        completed: 120,
        failed: 1,
        delayed: 0,
      }),
    };

    const module: TestingModule = await Test.createTestingModule({
      providers: [
        QueueMetricsService,
        { provide: getQueueToken('transcode-video'), useValue: mockQueue },
      ],
    }).compile();

    service = module.get<QueueMetricsService>(QueueMetricsService);
  });

  it('collects accurate job counts from queue', async () => {
    await service.collectMetrics();
    expect(mockQueue.getJobCounts).toHaveBeenCalledWith(
      'waiting',
      'active',
      'completed',
      'failed',
      'delayed',
    );
  });
});
```

---

## 11. Comprehensive Examples

### Queue Pausing and Resuming for Maintenance

```typescript
// src/admin/queue-controller.service.ts
import { Injectable, Logger } from '@nestjs/common';
import { InjectQueue } from '@nestjs/bullmq';
import { Queue } from 'bullmq';

@Injectable()
export class QueueControllerService {
  private readonly logger = new Logger(QueueControllerService.name);

  constructor(@InjectQueue('transcode-video') private readonly queue: Queue) {}

  async pauseQueue() {
    await this.queue.pause();
    this.logger.warn('Queue [transcode-video] PAUSED. No new jobs will be picked up.');
  }

  async resumeQueue() {
    await this.queue.resume();
    this.logger.log('Queue [transcode-video] RESUMED.');
  }

  async drainQueue() {
    await this.queue.drain();
    this.logger.warn('Queue [transcode-video] DRAINING: all waiting jobs discarded.');
  }
}
```

---

## 12. Cross-References & Further Reading

- [02-bullmq.md](./02-bullmq.md) - BullMQ setup
- [03-producers-and-consumers.md](./03-producers-and-consumers.md) - Consumer design
- [07-production/02-performance/01-benchmarking-and-profiling.md](../../07-production/02-performance/01-benchmarking-and-profiling.md) - Performance profiling
- [KEDA Redis Scaler Documentation](https://keda.sh/docs/scalers/redis-lists/)
