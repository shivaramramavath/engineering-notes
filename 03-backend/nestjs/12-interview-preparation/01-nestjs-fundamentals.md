---
title: "NestJS Interview: Framework Fundamentals and Core Architecture"
description: "High-frequency interview questions on NestJS core concepts, comparison with Express and Spring Boot, and lifecycle"
tags: [nestjs, interview, fundamentals, architecture]
---

# NestJS Interview: Framework Fundamentals and Core Architecture

### Q1: What is NestJS and what problems does it solve compared to vanilla Express?
**Answer**: While Express provides minimal, unopinionated HTTP routing, it lacks conventions for architecture, dependency injection, and project organization. Large Express applications frequently degrade into unstructured spaghetti code. NestJS provides an enterprise-grade abstraction layer built on TypeScript and Inversion of Control (IoC), offering a standardized modular architecture inspired by Angular and Spring Boot, while maintaining underlying engine flexibility (Express or Fastify).

### Q2: How does NestJS support both Express and Fastify under the hood?
**Answer**: NestJS decouples HTTP engine operations behind the `AbstractHttpAdapter` interface. By default, `@nestjs/platform-express` implements this interface using Express. By substituting `FastifyAdapter` from `@nestjs/platform-fastify`, developers can switch engines without modifying controllers, guards, or interceptors.
