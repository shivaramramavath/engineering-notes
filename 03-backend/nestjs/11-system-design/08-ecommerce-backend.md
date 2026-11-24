---
title: "System Design: High-Concurrency E-Commerce Flash Sale System"
description: "Handling inventory flash sales, pessimistic database locking, Redis pre-allocation, and Stripe checkout synchronization"
tags: [nestjs, system-design, ecommerce, flash-sale, inventory]
---

# System Design: High-Concurrency E-Commerce Flash Sale System

## Architecture
- **Inventory Pre-allocation**: Redis atomic `DECR` commands reserve stock in memory ($< 1\text{ms}$) to prevent database thrashing.
- **Order Placement**: Asynchronous BullMQ worker persists orders to PostgreSQL via ACID transactions.
