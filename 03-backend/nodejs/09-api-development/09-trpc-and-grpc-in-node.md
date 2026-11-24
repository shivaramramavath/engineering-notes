# tRPC & gRPC in Node.js: RPC Architectures

> High-velocity RPC communication: End-to-end type safety without codegen via tRPC, and high-performance binary microservice communication via gRPC (`@grpc/grpc-js`).

| Metadata | Details |
| :--- | :--- |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Protocols** | tRPC v11+, gRPC (`@grpc/grpc-js`, Protocol Buffers v3) |
| **Use Cases** | Fullstack TypeScript Monorepos (tRPC), Microservice East-West Traffic (gRPC) |

---

## 1. tRPC: Zero-Codegen Type Safety

tRPC allows you to build type-safe APIs without schemas or code generators. The client simply imports the backend's TypeScript type definitions:

```typescript
// server/trpc.ts
import { initTRPC } from '@trpc/server';
import { z } from 'zod';

const t = initTRPC.create();

export const appRouter = t.router({
  getUser: t.procedure
    .input(z.object({ id: z.string() }))
    .query(async ({ input }) => {
      return { id: input.id, name: 'Alice', role: 'admin' };
    }),
});

export type AppRouter = typeof appRouter;
```

Client usage (in frontend SPA or mobile):
```typescript
// Client automatically infers parameter types and return object types!
const user = await trpc.getUser.query({ id: 'usr_123' });
console.log(user.name); // Full autocomplete, zero build step!
```

---

## 2. gRPC for High-Throughput Microservice Communication

When communicating between backend services across the data center, JSON over HTTP/1.1 is inefficient.
**gRPC** uses binary serialization (Protocol Buffers) over multiplexed HTTP/2 streams:

```protobuf
// proto/users.proto
syntax = "proto3";
package users;

service UserService {
  rpc GetUser (UserRequest) returns (UserResponse);
}

message UserRequest { string id = 1; }
message UserResponse { string id = 1; string name = 2; }
```

Server Implementation (`@grpc/grpc-js`):
```javascript
import grpc from '@grpc/grpc-js';
import protoLoader from '@grpc/proto-loader';

const packageDef = protoLoader.loadSync('proto/users.proto');
const proto = grpc.loadPackageDefinition(packageDef).users;

const server = new grpc.Server();
server.addService(proto.UserService.service, {
  getUser: (call, callback) => {
    callback(null, { id: call.request.id, name: 'Alice' });
  },
});

server.bindAsync('0.0.0.0:50051', grpc.ServerCredentials.createInsecure(), () => {
  console.log('gRPC service listening on port 50051');
});
```
