---
title: "System Design: Hotel and Event Seat Reservation Engine"
description: "Designing a high-contention booking engine: Redlock distributed locking, temporary holds, and timeout reclamation"
tags: [nestjs, system-design, booking, redlock, temporary-holds]
---

# System Design: Hotel and Event Seat Reservation Engine

## Architecture
- **Two-Phase Reservation**:
  1. Temporary Seat Hold (Redis key with 10-minute TTL).
  2. Permanent Payment Confirmation (writes to PostgreSQL and clears hold).
