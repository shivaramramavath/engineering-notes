# Prisma in NestJS Cheat Sheet

### Service Integration
```typescript
@Injectable()
export class PrismaService extends PrismaClient implements OnModuleInit, OnModuleDestroy {
  async onModuleInit() { await this.$connect(); }
  async onModuleDestroy() { await this.$disconnect(); }
}
```

### Queries & Transactions
```typescript
const user = await prisma.user.findUnique({
  where: { email },
  include: { orders: { where: { status: 'PAID' } } },
});

// Interactive Transaction
await prisma.$transaction(async (tx) => {
  await tx.user.update(...);
  await tx.audit.create(...);
});
```
