# NestJS Core Cheat Sheet

| Feature | Syntax / Example | Purpose |
| :--- | :--- | :--- |
| **Bootstrap** | `NestFactory.create(AppModule)` | Initializes IoC container and binds HTTP server |
| **Module** | `@Module({ imports, controllers, providers, exports })` | Declares cohesive domain boundary |
| **Controller** | `@Controller('path')` | Handles incoming HTTP requests |
| **Injectable** | `@Injectable()` | Marks class for IoC container instantiation |
| **Shutdown** | `app.enableShutdownHooks()` | Listens for SIGTERM/SIGINT for clean termination |
