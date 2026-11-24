---
title: "NestJS Interview: Testing Strategies"
description: "Unit testing, integration testing, Testcontainers, and mocking dependencies with @nestjs/testing"
tags: [nestjs, interview, testing, unit-tests, e2e, mocks]
---

# NestJS Interview: Testing Strategies

### Q1: How do you mock a service dependency in a NestJS unit test?
**Answer**: Use `Test.createTestingModule({ providers: [TargetService, { provide: DependencyService, useValue: mockInstance }] }).compile()`.
