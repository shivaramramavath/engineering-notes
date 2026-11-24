# gRPC Microservices Cheat Sheet

```typescript
// main.ts
app.connectMicroservice<MicroserviceOptions>({
  transport: Transport.GRPC,
  options: {
    package: 'hero',
    protoPath: join(__dirname, 'hero/hero.proto'),
    url: '0.0.0.0:5000',
  },
});
```
