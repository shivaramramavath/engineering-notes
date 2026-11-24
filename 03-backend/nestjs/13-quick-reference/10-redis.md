# Redis & Caching Cheat Sheet

### NestJS 11 Cache-Manager v6 Configuration
```typescript
CacheModule.registerAsync({
  useFactory: () => ({
    stores: [
      new Keyv({
        store: new CacheableMemory({ ttl: 60000, lruSize: 5000 }),
      }),
      createKeyv(process.env.REDIS_URL),
    ],
  }),
})
```
