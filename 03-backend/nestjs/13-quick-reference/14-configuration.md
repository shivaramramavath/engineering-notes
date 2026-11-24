# Configuration & Validation Cheat Sheet

```typescript
ConfigModule.forRoot({
  isGlobal: true,
  validate: (config) => {
    const parsed = schema.safeParse(config);
    if (!parsed.success) throw new Error(JSON.stringify(parsed.error.format()));
    return parsed.data;
  },
})
```
