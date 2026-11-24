# Fastify 5 Adapter Cheat Sheet

```typescript
const app = await NestFactory.create<NestFastifyApplication>(
  AppModule,
  new FastifyAdapter({ bodyLimit: 1048576 }),
);
await app.listen(3000, '0.0.0.0'); // Mandatory 0.0.0.0 for Docker!
```
