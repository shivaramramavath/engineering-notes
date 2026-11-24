# Production Hardening Cheat Sheet

- **Security**: `app.use(helmet())`, `app.enableCors({ origin: [...] })`
- **Validation**: `app.useGlobalPipes(new ValidationPipe({ whitelist: true, forbidNonWhitelisted: true }))`
- **Proxy**: `app.set('trust proxy', 1)`
- **Shutdown**: `app.enableShutdownHooks()`
- **Docker**: `USER node`, multi-stage build, dumb-init entrypoint
