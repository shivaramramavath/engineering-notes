---
title: "NestJS Interview: Deep Dive into Scopes and Lifecycle Hooks"
description: "Advanced questions on DEFAULT, REQUEST, and TRANSIENT scopes, and exact startup/shutdown lifecycle hooks"
tags: [nestjs, interview, scopes, lifecycle-hooks]
---

# NestJS Interview: Deep Dive into Scopes and Lifecycle Hooks

### Startup Lifecycle:
`onModuleInit` $	o$ `onApplicationBootstrap`

### Shutdown Lifecycle (NestJS 11):
`onModuleDestroy` $	o$ `beforeApplicationShutdown` $	o$ `onApplicationShutdown`
