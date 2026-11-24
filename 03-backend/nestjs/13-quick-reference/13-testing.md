# NestJS Testing Cheat Sheet

```typescript
const module: TestingModule = await Test.createTestingModule({
  controllers: [UsersController],
  providers: [
    UsersService,
    {
      provide: UsersRepository,
      useValue: {
        findById: jest.fn().mockResolvedValue({ id: '1', name: 'Alice' }),
      },
    },
  ],
}).compile();

const service = module.get<UsersService>(UsersService);
```
