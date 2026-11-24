# Observability Cheat Sheet

- **JSON Logger**: `new ConsoleLogger({ json: true })` in NestJS 11
- **Pino**: `LoggerModule.forRoot({ pinoHttp: { redact: ['req.headers.authorization'] } })`
- **Correlation ID**: `AsyncLocalStorage<RequestStore>()`
- **Health**: `@nestjs/terminus` (`checkHeap`, `pingCheck`)
