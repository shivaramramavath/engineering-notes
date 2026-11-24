# gRPC Authentication and Metadata Handling

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 05-advanced / 06-microservices
- **Prerequisites**: [06-grpc-fundamentals-and-protobuf](./06-grpc-fundamentals-and-protobuf.md), [07-grpc-server-and-client](./07-grpc-server-and-client.md), [04-intermediate/06-authentication/02-jwt-authentication](../../04-intermediate/06-authentication/02-jwt-authentication.md)

---

## 1. Context & Purpose

While HTTP uses standard headers (`Authorization: Bearer <token>`) and WebSockets use handshake auth, gRPC relies on **gRPC Metadata** (key-value pairs of binary or ASCII strings encoded into HTTP/2 headers).

When microservices communicate internally over gRPC, zero-trust architectures require every RPC call to be authenticated and authorized. Microservices must extract authorization tokens from incoming gRPC metadata, validate identities via custom guards, forward distributed tracing IDs, and propagate tenant contexts.

---

## 2. Theoretical Foundations

### 2.1 gRPC Metadata (RFC 7540)
Metadata in gRPC is represented as an instance of `Metadata` from `@grpc/grpc-js`:
- Keys are ASCII strings (case-insensitive).
- Binary metadata keys must end with `-bin` (e.g. `trace-context-bin`).
- Handled in NestJS handlers via the `@GrpcMethod()` metadata argument or `RpcContext`.

### 2.2 Security Models in gRPC
1. **Transport Security (Channel Credentials)**: Mutual TLS (mTLS) encrypting the TCP connection and verifying service certificates.
2. **Call Credentials (Per-RPC Metadata)**: Attaching bearer tokens (JWT or API keys) to each individual RPC invocation.

---

## 3. Architecture & Internal Mechanics

```
gRPC Client (Caller)
const metadata = new Metadata();
metadata.set('authorization', 'Bearer token_123');
heroService.findOne({ id: 1 }, metadata);
                 │
                 ▼
     HTTP/2 Frame with Metadata Headers
                 │
                 ▼
gRPC Server (Callee)
                 │
                 ▼
          GrpcAuthGuard
Extracts metadata via context.switchToRpc().getContext()
                 │
         ┌───────┴───────┐
         ▼               ▼
     [Valid]         [Invalid]
         │               │
         ▼               ▼
  Execute Handler    Throw RpcException(16 UNAUTHENTICATED)
```

---

## 4. Concrete Implementation

### 4.1 Attaching Metadata on the gRPC Client

```typescript
// src/hero/hero-client-caller.service.ts
import { Injectable, OnModuleInit } from '@nestjs/common';
import { ClientGrpc } from '@nestjs/microservices';
import { Metadata } from '@grpc/grpc-js';
import { firstValueFrom } from 'rxjs';
import { HeroService } from './interfaces/hero.interface';

@Injectable()
export class HeroClientCallerService implements OnModuleInit {
  private heroService!: HeroService;

  constructor(private readonly client: ClientGrpc) {}

  onModuleInit() {
    this.heroService = this.client.getService<HeroService>('HeroService');
  }

  async callHeroWithAuth(heroId: number, token: string) {
    // 1. Construct gRPC Metadata object
    const metadata = new Metadata();
    metadata.set('authorization', `Bearer ${token}`);
    metadata.set('x-tenant-id', 'tenant_enterprise_01');

    // 2. Pass metadata as the second argument to the RPC call
    const hero$ = this.heroService.findOne({ id: heroId }, metadata);
    return firstValueFrom(hero$);
  }
}
```

---

### 4.2 Building a `GrpcAuthGuard`

```typescript
// src/common/guards/grpc-auth.guard.ts
import { CanActivate, ExecutionContext, Injectable } from '@nestjs/common';
import { RpcException } from '@nestjs/microservices';
import { status as GrpcStatus, Metadata } from '@grpc/grpc-js';
import { JwtService } from '@nestjs/jwt';

@Injectable()
export class GrpcAuthGuard implements CanActivate {
  constructor(private readonly jwtService: JwtService) {}

  async canActivate(context: ExecutionContext): Promise<boolean> {
    // Switch context to RPC
    const rpcContext = context.switchToRpc();

    // In gRPC, getContext() returns the Metadata instance!
    const metadata: Metadata = rpcContext.getContext();

    const authHeaders = metadata.get('authorization');
    if (!authHeaders || authHeaders.length === 0) {
      throw new RpcException({
        code: GrpcStatus.UNAUTHENTICATED,
        message: 'Missing authorization metadata',
      });
    }

    const authHeader = authHeaders[0].toString();
    const token = authHeader.replace(/^Bearer\s+/i, '');

    try {
      const decoded = await this.jwtService.verifyAsync(token, {
        secret: process.env.JWT_SECRET || 'secret',
      });

      // Attach user to data payload or request context
      const data = rpcContext.getData();
      data.user = decoded;

      return true;
    } catch (err: any) {
      throw new RpcException({
        code: GrpcStatus.UNAUTHENTICATED,
        message: `Invalid token: ${err.message}`,
      });
    }
  }
}
```

---

### 4.3 Applying the Guard in gRPC Controller

```typescript
// src/hero/hero-secure.controller.ts
import { Controller, UseGuards } from '@nestjs/common';
import { GrpcMethod, Payload } from '@nestjs/microservices';
import { GrpcAuthGuard } from '../common/guards/grpc-auth.guard';
import { HeroById, Hero } from './interfaces/hero.interface';

@Controller()
export class HeroSecureController {
  @UseGuards(GrpcAuthGuard)
  @GrpcMethod('HeroService', 'FindOne')
  findOne(@Payload() data: HeroById & { user?: any }): Hero {
    console.log(`Authenticated RPC request by user: ${data.user?.email}`);

    return {
      id: data.id,
      name: 'Secure Superman',
      power: 'Flight',
    };
  }
}
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Throwing Standard `UnauthorizedException`
Throwing HTTP `UnauthorizedException` (401) inside a gRPC handler does not translate to standard gRPC unauthenticated errors. The client receives code `2 UNKNOWN`.
- **Rule**: Always throw `RpcException` with explicit `@grpc/grpc-js` status code `GrpcStatus.UNAUTHENTICATED` (code 16).

### 5.2 Metadata Array Multi-Values
Calling `metadata.get('key')` always returns an **array** of values (`MetadataValue[]`), because HTTP/2 permits duplicate header keys.
- **Handling**: Always check `headers.length > 0` and extract `headers[0]`.

---

## 6. Performance & Memory Implications

- **HPACK Compression**: Because HTTP/2 uses HPACK compression, repeated transmission of the same Authorization header across thousands of multiplexed RPC calls consumes almost zero bandwidth.
- **Binary Metadata**: Binary metadata keys (`-bin`) pass raw Buffers without base64 text conversion overhead.

---

## 7. Anti-Patterns & Common Mistakes

- **Passing Auth Inside the Payload Body**: Defining `message Request { string token = 1; int32 id = 2; }`. Auth tokens belong in metadata headers, not polluting domain data models.
- **Ignoring mTLS in Production**: Running plaintext gRPC without TLS across public cloud networks. Always configure channel credentials (`grpc.credentials.createSsl()`).

---

## 8. Production Checklist & Operational Readiness

- [ ] Mutual TLS (mTLS) configured for transport-layer security.
- [ ] Calls protected by `GrpcAuthGuard` reading metadata.
- [ ] `RpcException` used with explicit `GrpcStatus` codes.
- [ ] Distributed tracing headers (`traceparent`) propagated in metadata.

---

## 9. Migration & Version History

- **NestJS 8/9/10**: Standardized `Metadata` access via `context.switchToRpc().getContext()`.

---

## 10. Verification & Testing

Unit testing `GrpcAuthGuard`:

```typescript
import { GrpcAuthGuard } from './grpc-auth.guard';
import { Metadata, status as GrpcStatus } from '@grpc/grpc-js';

describe('GrpcAuthGuard', () => {
  let guard: GrpcAuthGuard;
  let mockJwt: any;

  beforeEach(() => {
    mockJwt = { verifyAsync: vi.fn().mockResolvedValue({ sub: 'u1' }) };
    guard = new GrpcAuthGuard(mockJwt);
  });

  it('permits valid bearer token in metadata', async () => {
    const metadata = new Metadata();
    metadata.set('authorization', 'Bearer valid_token');

    const mockContext: any = {
      switchToRpc: () => ({
        getContext: () => metadata,
        getData: () => ({}),
      }),
    };

    const allowed = await guard.canActivate(mockContext);
    expect(allowed).toBe(true);
  });
});
```

---

## 11. Comprehensive Examples

### Mutual TLS (mTLS) Channel Credentials Setup

```typescript
// src/main.ts (Secure gRPC Server with mTLS)
import { NestFactory } from '@nestjs/core';
import { Transport, MicroserviceOptions } from '@nestjs/microservices';
import * as grpc from '@grpc/grpc-js';
import * as fs from 'fs';
import { join } from 'path';
import { AppModule } from './app.module';

async function bootstrap() {
  const rootCert = fs.readFileSync(join(__dirname, '../certs/ca.crt'));
  const serverCert = fs.readFileSync(join(__dirname, '../certs/server.crt'));
  const serverKey = fs.readFileSync(join(__dirname, '../certs/server.key'));

  // Create mutual TLS credentials
  const credentials = grpc.ServerCredentials.createSsl(
    rootCert,
    [{ cert_chain: serverCert, private_key: serverKey }],
    true, // checkClientCertificate: true (mTLS)
  );

  const app = await NestFactory.createMicroservice<MicroserviceOptions>(AppModule, {
    transport: Transport.GRPC,
    options: {
      package: 'hero',
      protoPath: join(__dirname, 'proto/hero.proto'),
      url: '0.0.0.0:50051',
      credentials,
    },
  });

  await app.listen();
}
bootstrap();
```

---

## 12. Cross-References & Further Reading

- [06-grpc-fundamentals-and-protobuf.md](./06-grpc-fundamentals-and-protobuf.md) - Protobuf contracts
- [07-grpc-server-and-client.md](./07-grpc-server-and-client.md) - Client stubs
- [04-intermediate/06-authentication/02-jwt-authentication.md](../../04-intermediate/06-authentication/02-jwt-authentication.md) - JWT verification
- [gRPC Authentication Guide](https://grpc.io/docs/guides/auth/)
