---
title: "NestJS Interview: Request Lifecycle and Enhancers"
description: "Mastering the request pipeline: exact execution order of Middleware, Guards, Interceptors, Pipes, and Filters"
tags: [nestjs, interview, request-lifecycle, guards, interceptors, pipes]
---

# NestJS Interview: Request Lifecycle and Enhancers

### Q1: What is the exact execution order of the NestJS request lifecycle?
**Answer**:
1. Global Middleware $	o$ Module Middleware
2. Global Guards $	o$ Controller Guards $	o$ Route Guards
3. Global Pre-Interceptors $	o$ Controller Pre-Interceptors $	o$ Route Pre-Interceptors
4. Global Pipes $	o$ Controller Pipes $	o$ Route Pipes $	o$ Route Param Pipes
5. Route Handler (Controller method)
6. Route Post-Interceptors $	o$ Controller Post-Interceptors $	o$ Global Post-Interceptors (RxJS)
7. Exception Filters (Route $	o$ Controller $	o$ Global) if any step throws.

### Q2: What is the difference between a Guard and an Interceptor?
**Answer**:
- **Guard**: Implements `CanActivate` and has a single responsibility: returning boolean `true` or `false` to permit or deny access. Executes before interceptors and pipes.
- **Interceptor**: Implements `NestInterceptor` and wraps the entire execution using RxJS `Observable`. Can mutate inbound requests, transform output values, handle timeouts, and manage database transactions.
