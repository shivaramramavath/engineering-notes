# NestJS Decorators Cheat Sheet

### Class Decorators
- `@Module({ imports, controllers, providers, exports })`
- `@Controller('prefix')`
- `@Injectable({ scope?: Scope })`
- `@Catch(...exceptions)`
- `@WebSocketGateway(port?, options?)`

### Route Method Decorators
- `@Get(path?)`, `@Post(path?)`, `@Put(path?)`, `@Patch(path?)`, `@Delete(path?)`, `@Options()`, `@Head()`, `@All()`
- `@HttpCode(200)`
- `@Header('Cache-Control', 'none')`
- `@Redirect('https://nestjs.com', 301)`
- `@Sse('stream')`

### Route Parameter Decorators
- `@Req()`, `@Request()` $	o$ Express Request object
- `@Res({ passthrough: true })` $	o$ Express Response object
- `@Next()` $	o$ Express NextFunction
- `@Param('id', ParseUUIDPipe)` $	o$ Route parameter
- `@Query('page', new DefaultValuePipe(1), ParseIntPipe)` $	o$ Query parameter
- `@Body(ValidationPipe)` $	o$ Request body payload
- `@Headers('authorization')` $	o$ Inbound HTTP header
- `@Ip()` $	o$ Client IP address
- `@Session()` $	o$ Session object
