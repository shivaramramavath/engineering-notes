# gRPC Client Implementation and Dual Server Architecture

- **NestJS Version**: 11.x
- **Node.js Baseline**: >= 20.0.0
- **Module**: 05-advanced / 06-microservices
- **Prerequisites**: [06-grpc-fundamentals-and-protobuf](./06-grpc-fundamentals-and-protobuf.md), [03-core-concepts/04-modules-and-di/01-dependency-injection](../../03-core-concepts/04-modules-and-di/01-dependency-injection.md)

---

## 1. Context & Purpose

Once a gRPC microservice is implemented, client microservices (API Gateways, sibling domain services) need a type-safe way to call its remote RPC methods. NestJS provides the `ClientGrpc` abstraction via `ClientsModule.register()`.

With `ClientGrpc`, NestJS loads the `.proto` file, reflects its service definition, and generates a strictly typed client stub (`client.getService<HeroService>('HeroService')`). Calling remote methods behaves identically to invoking local TypeScript service methods, returning reactive RxJS Observables that easily convert to Promises.

---

## 2. Theoretical Foundations

### 2.1 The Client Stub Pattern
A client stub simulates the remote interface locally:
1. **Invocation**: Application calls `heroService.findOne({ id: 1 })`.
2. **Marshaling**: The stub serializes `{ id: 1 }` into binary Protobuf bytes.
3. **Transport**: Streams the bytes over an active HTTP/2 connection to the remote port.
4. **Unmarshaling**: Receives binary return bytes, decodes them to a JavaScript object, and completes the RxJS Observable.

---

## 3. Architecture & Internal Mechanics

```
┌────────────────────────────────────────────────────────┐
│             API Gateway Service (Consumer)             │
│    const heroService = client.getService<HeroService>  │
│    await firstValueFrom(heroService.findOne({ id: 1 }))│
└───────────────────────────┬────────────────────────────┘
                            │
              Binary Protocol Buffers over HTTP/2
                            │
                            ▼
┌────────────────────────────────────────────────────────┐
│             Hero Microservice (gRPC Server)            │
│               Port 50051 (hero.proto)                  │
│       @GrpcMethod('HeroService', 'FindOne')            │
└────────────────────────────────────────────────────────┘
```

---

## 4. Concrete Implementation

### 4.1 Configuring the Client with `ClientsModule`

```typescript
// src/hero/hero-client.module.ts
import { Module } from '@nestjs/common';
import { ClientsModule, Transport } from '@nestjs/microservices';
import { join } from 'path';
import { HeroClientService } from './hero-client.service';

export const HERO_PACKAGE = 'HERO_PACKAGE';

@Module({
  imports: [
    ClientsModule.register([
      {
        name: HERO_PACKAGE,
        transport: Transport.GRPC,
        options: {
          package: 'hero',
          protoPath: join(__dirname, '../proto/hero.proto'),
          url: process.env.HERO_SERVICE_URL || 'localhost:50051',
        },
      },
    ]),
  ],
  providers: [HeroClientService],
  exports: [HeroClientService],
})
export class HeroClientModule {}
```

---

### 4.2 Consuming the gRPC Service via `ClientGrpc`

```typescript
// src/hero/hero-client.service.ts
import { Injectable, Inject, OnModuleInit, Logger } from '@nestjs/common';
import { ClientGrpc } from '@nestjs/microservices';
import { firstValueFrom } from 'rxjs';
import { HeroService, Hero } from './interfaces/hero.interface';
import { HERO_PACKAGE } from './hero-client.module';

@Injectable()
export class HeroClientService implements OnModuleInit {
  private heroService!: HeroService;
  private readonly logger = new Logger(HeroClientService.name);

  constructor(@Inject(HERO_PACKAGE) private readonly client: ClientGrpc) {}

  onModuleInit() {
    // Obtain typed service stub dynamically generated from hero.proto
    this.heroService = this.client.getService<HeroService>('HeroService');
  }

  async getHero(id: number): Promise<Hero> {
    this.logger.log(`Calling remote gRPC HeroService.FindOne(${id})...`);

    // heroService.findOne() returns an RxJS Observable<Hero>
    const hero$ = this.heroService.findOne({ id });
    return firstValueFrom(hero$);
  }
}
```

---

### 4.3 Exposing gRPC via Public REST Controller (Gateway Pattern)

```typescript
// src/hero/hero-gateway.controller.ts
import { Controller, Get, Param, ParseIntPipe } from '@nestjs/common';
import { HeroClientService } from './hero-client.service';

@Controller('heroes')
export class HeroGatewayController {
  constructor(private readonly heroClient: HeroClientService) {}

  @Get(':id')
  async getHero(@Param('id', ParseIntPipe) id: number) {
    return this.heroClient.getHero(id);
  }
}
```

---

## 5. Edge Cases & Failure Modes

### 5.1 Service Unavailable at Bootstrap
If the remote gRPC server is offline when the client application boots, `getService()` still succeeds because gRPC HTTP/2 connections establish lazily on the first request. The first call to `firstValueFrom()` will reject with `14 UNAVAILABLE: No connection established`.
- **Handling**: Apply retry logic with exponential backoff on gRPC client calls.

### 5.2 Proto Method Case Sensitivity
In `.proto`, RPC methods are conventionally PascalCase (`rpc FindOne(...)`). In TypeScript, calling `heroService.findOne()` or `heroService.FindOne()` depends on `proto-loader` configuration. By default, NestJS normalizes method names to camelCase (`findOne`).

---

## 6. Performance & Memory Implications

- **Connection Reuse**: All calls across the lifetime of the application share a single multiplexed HTTP/2 TCP connection, eliminating TCP connection overhead.
- **Microsecond Latency**: Internal gRPC roundtrip times typically complete in $< 1\text{ms}$ inside the same Kubernetes cluster.

---

## 7. Anti-Patterns & Common Mistakes

- **Calling `getService()` on Every Request**: Calling `this.client.getService()` inside the route handler instead of caching it once in `onModuleInit()`.
- **Forgetting `protoPath` in Deployment Containers**: Deploying Docker images without including `.proto` files in the container build.

---

## 8. Production Checklist & Operational Readiness

- [ ] `.proto` files synchronized across repositories using Git submodules or an npm package.
- [ ] Service stubs initialized once inside `onModuleInit()`.
- [ ] gRPC status codes (e.g. `NOT_FOUND`, `UNAVAILABLE`) mapped to HTTP exceptions at Gateway.
- [ ] Channel keep-alive pings enabled to prevent load balancers from killing idle TCP connections.

---

## 9. Migration & Version History

- **NestJS 8/9/10**: Replaced old `@grpc/proto-loader` callbacks with strict Promise/Observable pipelines.
- **NestJS 11.x**: Support for modern TypeScript 5 interfaces and Protobuf 3 syntax.

---

## 10. Verification & Testing

Unit testing gRPC client consumption:

```typescript
import { HeroClientService } from './hero-client.service';
import { of } from 'rxjs';

describe('HeroClientService', () => {
  let service: HeroClientService;
  let mockGrpcService: any;

  beforeEach(() => {
    mockGrpcService = {
      findOne: vi.fn().mockReturnValue(of({ id: 1, name: 'Flash' })),
    };

    const mockClient: any = {
      getService: vi.fn().mockReturnValue(mockGrpcService),
    };

    service = new HeroClientService(mockClient);
    service.onModuleInit();
  });

  it('fetches hero via gRPC stub and converts to promise', async () => {
    const hero = await service.getHero(1);
    expect(hero.name).toBe('Flash');
    expect(mockGrpcService.findOne).toHaveBeenCalledWith({ id: 1 });
  });
});
```

---

## 11. Comprehensive Examples

### Mapping gRPC Error Codes to NestJS HTTP Exceptions

```typescript
// src/common/filters/grpc-to-http.filter.ts
import { ExceptionFilter, Catch, ArgumentsHost, HttpStatus } from '@nestjs/common';
import { Response } from 'express';
import { status as GrpcStatus } from '@grpc/grpc-js';

@Catch()
export class GrpcToHttpExceptionFilter implements ExceptionFilter {
  catch(exception: any, host: ArgumentsHost) {
    const ctx = host.switchToHttp();
    const response = ctx.getResponse<Response>();

    let httpStatus = HttpStatus.INTERNAL_SERVER_ERROR;

    switch (exception.code) {
      case GrpcStatus.NOT_FOUND:
        httpStatus = HttpStatus.NOT_FOUND;
        break;
      case GrpcStatus.INVALID_ARGUMENT:
        httpStatus = HttpStatus.BAD_REQUEST;
        break;
      case GrpcStatus.UNAUTHENTICATED:
        httpStatus = HttpStatus.UNAUTHORIZED;
        break;
      case GrpcStatus.UNAVAILABLE:
        httpStatus = HttpStatus.SERVICE_UNAVAILABLE;
        break;
    }

    response.status(httpStatus).json({
      statusCode: httpStatus,
      message: exception.details || exception.message,
      grpcCode: exception.code,
    });
  }
}
```

---

## 12. Cross-References & Further Reading

- [06-grpc-fundamentals-and-protobuf.md](./06-grpc-fundamentals-and-protobuf.md) - Protobuf contracts
- [08-grpc-streaming.md](./08-grpc-streaming.md) - gRPC streaming
- [09-grpc-authentication.md](./09-grpc-authentication.md) - gRPC metadata authentication
- [gRPC Official Node Guide](https://grpc.io/docs/languages/node/)
