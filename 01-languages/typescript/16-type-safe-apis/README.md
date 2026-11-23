# Module 16: Type-Safe APIs

> Comprehensive guide and curriculum for building end-to-end type-safe APIs, transport contracts, and client architectures in TypeScript.

## Overview
This module explores how to establish compile-time contracts between clients and servers, eliminating API drift, unhandled runtime errors, and unauthorized field exposure.

## Module Structure

| Note | Title | Description | Baseline |
| :--- | :--- | :--- | :--- |
| **[00-api-contracts.md](./00-api-contracts.md)** | End-to-End API Contracts | Core architecture of API contracts, monorepo type sharing, and drift prevention. | TS 4.5+ |
| **[01-request-response-types.md](./01-request-response-types.md)** | Request & Response Types | Modeling HTTP headers, query strings, bodies, and status-coded response shapes. | TS 4.5+ |
| **[02-dto-pattern.md](./02-dto-pattern.md)** | Data Transfer Objects (DTO) | Decoupling domain entities from transport representations, projection, and stripping. | TS 4.5+ |
| **[03-pagination-types.md](./03-pagination-types.md)** | Pagination Types | Offset, page-based, and cursor-based pagination contracts, and Relay Connection specs. | TS 4.5+ |
| **[04-error-response-types.md](./04-error-response-types.md)** | Error Response Types | RFC 7807/9457 Problem Details, discriminated domain errors, and exhaustive handling. | TS 4.5+ |
| **[05-typed-fetch-and-api-client.md](./05-typed-fetch-and-api-client.md)** | Typed Fetch & API Clients | Type-safe HTTP clients, template literal route parsing, and interceptor pipelines. | TS 4.5+ |
| **[06-contract-first-apis.md](./06-contract-first-apis.md)** | Contract-First APIs | OpenAPI 3.1 codegen, tRPC monorepo type sharing, and GraphQL typed documents. | TS 4.5+ |

## Learning Path & Prerequisites
- Complete **[15-runtime-validation](../15-runtime-validation/README.md)** before starting this module.
- After completing this module, proceed to **[17-design-patterns](../17-design-patterns/README.md)** and **[18-testing-and-debugging](../18-testing-and-debugging/README.md)**.
