# Module 13: Performance Tuning & Optimization

Welcome to **Module 13: Performance Tuning & Optimization**. This module focuses on squeezing maximum efficiency from Spring Boot applications: startup optimization, Ahead-of-Time (AOT) compilation with GraalVM Native Image, Class Data Sharing (CDS), CRaC, HikariCP pool tuning, JPA caching, Virtual Threads throughput, and profiling with JFR and async-profiler.

---

## Module Contents

| # | Note | Description | Level | Status |
|---|---|---|---|---|
| 01 | [01-startup-time-and-lazy-init.md](file:///03-backend/spring-boot/13-performance/01-startup-time-and-lazy-init.md) | Reducing cold-start time, lazy bean initialization, bean scanning exclusion | Intermediate | Complete |
| 02 | [02-aot-and-graalvm-native-images.md](file:///03-backend/spring-boot/13-performance/02-aot-and-graalvm-native-images.md) | GraalVM Native Build Tools, runtime reflection hints, sub-50ms instant startup | Advanced | Complete |
| 03 | [03-class-data-sharing-and-crac-leyden.md](file:///03-backend/spring-boot/13-performance/03-class-data-sharing-and-crac-leyden.md) | AppCDS archive generation, Project CRaC checkpoint/restore, Project Leyden | Advanced | Complete |
| 04 | [04-connection-pool-tuning.md](file:///03-backend/spring-boot/13-performance/04-connection-pool-tuning.md) | Sizing HikariCP connection pools using PostgreSQL mathematical saturation formulas | Advanced | Complete |
| 05 | [05-jpa-performance-patterns.md](file:///03-backend/spring-boot/13-performance/05-jpa-performance-patterns.md) | StatelessSession, JDBC batching, read-only transactions, dirty checking bypass | Advanced | Complete |
| 06 | [06-caching-strategies.md](file:///03-backend/spring-boot/13-performance/06-caching-strategies.md) | In-memory Caffeine L1 cache paired with Redis L2 distributed cache, cache stampede | Advanced | Complete |
| 07 | [07-virtual-threads-and-throughput.md](file:///03-backend/spring-boot/13-performance/07-virtual-threads-and-throughput.md) | High-concurrency blocking I/O with Virtual Threads, carrier thread pin detection | Advanced | Complete |
| 08 | [08-tomcat-and-netty-tuning.md](file:///03-backend/spring-boot/13-performance/08-tomcat-and-netty-tuning.md) | Tuning embedded Tomcat worker threads, accept-count, max-connections, Netty buffers | Advanced | Complete |
| 09 | [09-gc-and-memory-in-containers.md](file:///03-backend/spring-boot/13-performance/09-gc-and-memory-in-containers.md) | Tuning G1GC and ZGC in Docker containers, `-XX:MaxRAMPercentage`, off-heap memory | Advanced | Complete |
| 10 | [10-profiling-spring-apps.md](file:///03-backend/spring-boot/13-performance/10-profiling-spring-apps.md) | Production profiling with JDK Flight Recorder (JFR), async-profiler, flamegraphs | Advanced | Complete |
| 11 | [11-load-testing-k6-gatling.md](file:///03-backend/spring-boot/13-performance/11-load-testing-k6-gatling.md) | Stress testing Spring Boot endpoints using k6 scripts and Gatling scenarios | Intermediate | Complete |
| 12 | [12-common-performance-pitfalls.md](file:///03-backend/spring-boot/13-performance/12-common-performance-pitfalls.md) | Identifying and fixing the top 10 performance anti-patterns in enterprise backends | Advanced | Complete |

---

## Next Steps
Proceed to **[Module 14: Production & Deployment](file:///03-backend/spring-boot/14-production-and-deployment/)** to master container packaging, Kubernetes manifests, and zero-downtime deployments.
