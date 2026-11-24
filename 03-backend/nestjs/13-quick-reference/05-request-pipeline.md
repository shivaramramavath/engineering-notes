# Request Pipeline Cheat Sheet

### Pipeline Execution Order
```
1. Global Middleware -> Module Middleware
2. Global Guards -> Controller Guards -> Route Guards
3. Global Pre-Interceptors -> Controller Pre-Interceptors -> Route Pre-Interceptors
4. Global Pipes -> Controller Pipes -> Route Pipes -> Param Pipes
5. Route Handler (Controller Action)
6. Route Post-Interceptors -> Controller Post-Interceptors -> Global Post-Interceptors
7. Exception Filters (Route -> Controller -> Global)
```

### Registration Tokens
- Guard: `{ provide: APP_GUARD, useClass: JwtAuthGuard }`
- Interceptor: `{ provide: APP_INTERCEPTOR, useClass: LoggingInterceptor }`
- Pipe: `{ provide: APP_PIPE, useClass: ValidationPipe }`
- Filter: `{ provide: APP_FILTER, useClass: GlobalExceptionFilter }`
