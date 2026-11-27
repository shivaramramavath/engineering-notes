# Security by Design

> **TL;DR**: Security is an architectural property, not a final-week patch: enforce **Least Privilege**, **Explicit Allow-Lists** (DTOs), **Parameterized Queries**, **Server-Side Authz on every endpoint**, and **Defense in Depth**.

## Core Design Controls
- **Prevent Mass Assignment**: Bind incoming JSON to strict Request DTOs, never directly into ORM entities.
- **Prevent SQL Injection**: Always use parameterized queries / prepared statements.
- **Object-Level Authorization (BOLA/IDOR)**: Every `GET /orders/{id}` must verify `order.ownerId == currentUser.id`.
- **Fail Closed**: Deny access by default if auth middleware or policy check errors out.
