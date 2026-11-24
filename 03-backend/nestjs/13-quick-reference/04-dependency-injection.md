# Dependency Injection Cheat Sheet

### Custom Provider Syntaxes
```typescript
// 1. Class Provider
{ provide: UsersService, useClass: UsersService }

// 2. Value Provider (Static value or mock)
{ provide: 'API_KEY', useValue: 'secret_123' }

// 3. Factory Provider (Dynamic / Async)
{
  provide: 'DB_CONNECTION',
  useFactory: async (config: ConfigService) => createConnection(config.get('DB_URL')),
  inject: [ConfigService],
}

// 4. Existing Provider (Alias)
{ provide: 'IUsersRepo', useExisting: PostgresUsersRepo }
```

### Breaking Circular Dependencies
```typescript
constructor(
  @Inject(forwardRef(() => OrdersService))
  private ordersService: OrdersService,
) {}
```
