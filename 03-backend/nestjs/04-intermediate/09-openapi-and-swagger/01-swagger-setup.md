# Swagger Setup

Setting up Swagger in Nest takes a few lines, but three decisions deserve thought: **where the docs live** (and who can see them in production), **how much documentation you write by hand vs generate** (the CLI plugin), and **how the document is built** (one spec or one per version). This note covers the setup and the traps.

Prerequisites: [REST and resource design](../08-api-design/01-rest-and-resource-design.md), [DTOs](../../03-core-concepts/02-validation-and-serialization/01-dto.md).

## Install

```bash
npm i @nestjs/swagger
# Fastify adapter only:
npm i @fastify/static
```

`@nestjs/swagger` bundles Swagger UI assets. Make sure its version is compatible with your `@nestjs/common` version (check the package's peer dependencies).

## Minimal setup

```ts
// main.ts
import { DocumentBuilder, SwaggerModule } from '@nestjs/swagger';

async function bootstrap() {
  const app = await NestFactory.create(AppModule);

  const config = new DocumentBuilder()
    .setTitle('Orders API')
    .setDescription('Public API for managing orders')
    .setVersion('1.0')
    .addBearerAuth()                    // adds a "bearer" security scheme (see "Documenting authentication")
    .build();

  const document = SwaggerModule.createDocument(app, config);
  SwaggerModule.setup('docs', app, document);

  await app.listen(3000);
}
```

Result:

| URL | Content |
|-----|---------|
| `/docs` | Swagger UI |
| `/docs-json` | The OpenAPI document as JSON (the default pattern is `<path>-json`) |
| `/docs-yaml` | The same as YAML |

Call `SwaggerModule.createDocument` **after** global configuration (prefix, versioning) and before `listen`, so the document reflects the final routes.

## `DocumentBuilder`

```ts
new DocumentBuilder()
  .setTitle('Orders API')
  .setDescription('...')                           // Markdown is supported in many UIs
  .setVersion('1.0')
  .setContact('API Team', 'https://example.com', 'api@example.com')
  .setLicense('MIT', 'https://opensource.org/licenses/MIT')
  .addServer('https://api.example.com', 'Production')
  .addServer('http://localhost:3000', 'Local')
  .addTag('orders', 'Order management')
  .addBearerAuth()
  .build();
```

- **`addServer`** controls the base URLs shown in the spec and used by "Try it out" and generated SDKs. Wrong server entries are a common cause of "Try it out" hitting the wrong host.
- The `info.version` is **your API version/doc version**, not the OpenAPI spec version.
- Tags declared with `addTag` provide descriptions and **ordering**; `@ApiTags` on controllers assigns endpoints to tags ([documenting endpoints](./02-documenting-endpoints.md)).

## Setup options

```ts
SwaggerModule.setup('docs', app, document, {
  jsonDocumentUrl: 'docs/openapi.json',            // custom URLs for the raw spec
  yamlDocumentUrl: 'docs/openapi.yaml',
  customSiteTitle: 'Orders API Docs',
  useGlobalPrefix: true,                           // put the UI under the global prefix: /api/docs
  swaggerOptions: {
    persistAuthorization: true,                    // keep the token across page reloads
    tagsSorter: 'alpha',
    operationsSorter: 'alpha',
    docExpansion: 'none',
  },
});
```

Notes:

- With `app.setGlobalPrefix('api')`, the docs are **not** under the prefix unless you set `useGlobalPrefix: true` (option names and behavior can vary; check your version's docs). The **routes inside** the document do include the prefix.
- The first positional argument is the UI path; it must not collide with an application route.
- Swagger UI is a **client-side app** calling your API: with CORS or cookies in play, "Try it out" follows the same browser rules as any frontend ([CORS](../06-authentication/06-cookie-authentication.md)).
- `swaggerOptions` are passed to Swagger UI itself (see its docs for the full list).

## Document only what you want

```ts
const document = SwaggerModule.createDocument(app, config, {
  include: [OrdersModule, UsersModule],            // restrict to certain modules
  operationIdFactory: (controllerKey: string, methodKey: string) => methodKey,   // cleaner operationIds
  deepScanRoutes: true,                            // also scan modules imported by the included ones
});
```

- **`include`** is how you produce separate documents (public vs internal, per module, per version).
- **`operationIdFactory`** matters for generated SDKs: the default ids look like `OrdersController_findOne`, which produces ugly client method names ([client SDK generation](./06-client-sdk-generation.md)). If you shorten them to just the method name, make sure they're unique across the document.
- Hide things from the docs with `@ApiExcludeController()`, `@ApiExcludeEndpoint()`, and `@ApiHideProperty()`.

### Versioned APIs

With [Nest versioning](../08-api-design/02-api-versioning.md), generate **one document per version** so each spec is accurate:

```ts
const v1 = SwaggerModule.createDocument(app, configV1, { include: [OrdersV1Module] });
SwaggerModule.setup('docs/v1', app, v1);

const v2 = SwaggerModule.createDocument(app, configV2, { include: [OrdersV2Module] });
SwaggerModule.setup('docs/v2', app, v2);
```

Mixing every version into one document produces duplicate/confusing paths and unusable SDKs.

## The CLI plugin: less boilerplate

Without help, you'd annotate every DTO property with `@ApiProperty()`. The **Nest CLI plugin** analyzes your code at build time and adds much of that metadata automatically (property types, optionality from `?`, enums, defaults, validation constraints from class-validator, and descriptions from comments).

```json
// nest-cli.json
{
  "compilerOptions": {
    "plugins": [
      {
        "name": "@nestjs/swagger",
        "options": {
          "classValidatorShim": true,
          "introspectComments": true
        }
      }
    ]
  }
}
```

What it does:

| Option | Effect |
|--------|--------|
| (default) | Adds `@ApiProperty` metadata to DTO/entity class properties and infers response types for controller methods |
| `classValidatorShim` | Reads class-validator decorators (`@IsEmail`, `@MinLength`, `@IsOptional`) to set formats, limits, `required` |
| `introspectComments` | Uses JSDoc comments as property/operation descriptions (and `@example`) |
| `dtoFileNameSuffix` / `controllerFileNameSuffix` | Which files are processed (defaults cover `.dto.ts`, `.entity.ts`, `.controller.ts`; add suffixes for other naming) |

```ts
export class CreateUserDto {
  /** The user's email address */          // ← becomes the description (introspectComments)
  @IsEmail()
  email: string;                            // ← type, required, format: email inferred (no @ApiProperty needed)

  @IsOptional() @IsInt() @Min(18)
  age?: number;                             // ← optional, integer, minimum: 18
}
```

Plugin caveats:

- It runs as a **TypeScript transformer during compilation** (`nest build` / `nest start`). Other pipelines need explicit setup: **Jest** needs the plugin wired into the `ts-jest` config (otherwise your specs/E2E tests see undecorated DTOs and the generated document is incomplete in tests), and **SWC/webpack/esbuild** builds need the SWC-compatible plugin or the equivalent integration. Check the docs for your toolchain.
- It **only processes files matching the configured suffixes**; a DTO in `user.request.ts` is silently skipped.
- **Explicit decorators still win and are sometimes required**: `@ApiProperty({ description, example })` for examples, `enumName`, `oneOf`, circular references, generics, and anything the plugin can't infer (unions, complex types).
- Commit to one approach per project: plugin plus selective explicit decorators, or all explicit. Mixed or partial coverage leads to "why is this property missing from the docs?" confusion.

## Don't expose your docs carelessly in production

An OpenAPI document is a **map of your attack surface** (every endpoint, parameter, and error). Public APIs publish it deliberately; internal or private APIs often shouldn't.

```ts
if (process.env.NODE_ENV !== 'production') {
  SwaggerModule.setup('docs', app, document);
}
```

Options for production:

- **Don't serve it** and publish the generated spec through your documentation site or portal instead (build step exports the JSON: [SDK generation](./06-client-sdk-generation.md)).
- **Protect it**: HTTP basic auth or your normal authentication in front of `/docs*`, a network restriction (VPN/IP allow-list), or serve it only on an internal port/host.
- Make sure documented **examples** don't contain real data or secrets, and that internal-only endpoints are excluded (`@ApiExcludeEndpoint`, `include`).
- Remember the UI uses inline scripts; if you've configured a strict Content Security Policy (for example via Helmet), the Swagger UI route may need a relaxed policy or exclusion ([security headers](../../07-production/01-security/02-http-security-headers-and-cors.md)).

Documentation is not access control: hiding an endpoint from Swagger doesn't protect it ([authorization](../07-authorization/README.md)).

## Verifying the setup

- Open `/docs` and `/docs-json`; check endpoints, tags, servers, and security schemes appear.
- Validate the JSON with an OpenAPI linter/validator (for example Spectral or the Swagger Editor) in CI.
- Compare against a few real endpoints: required fields, enums, array types, response codes.
- Use "Try it out" against local to catch wrong server URLs and CORS issues.

## Common mistakes

- **Creating the document before** applying global prefix/versioning/pipes, so the spec is missing or misrouted paths.
- **The plugin not applied** (build pipeline or Jest config) and wondering why DTO properties are empty.
- **DTOs not matching the plugin's file suffixes**, so they're skipped.
- **Wrong `addServer`** entries, so "Try it out" and SDKs call the wrong host.
- **Exposing docs publicly** when the API is private.
- **One mixed document for all versions.**
- **Duplicate/ugly `operationId`s** that produce bad SDK method names.
- **Treating hidden endpoints as protected.**
- **Docs path colliding with a route**, or blocked by a strict CSP.
- **Relying on hand-maintained docs** that drift instead of generating from code and checking in CI.

## Debugging

- Empty schemas in the UI: the plugin isn't running (check `nest-cli.json`, the build tool, and Jest transformer config) or the DTO file name doesn't match the suffix list; add `@ApiProperty()` explicitly to confirm.
- Endpoint missing from the document: the module isn't included (`include`/`deepScanRoutes`), the controller is excluded, or the route is registered after you created the document.
- `404` on `/docs` with a global prefix: the UI is not under the prefix unless configured; use `/docs` or set `useGlobalPrefix`.
- "Try it out" fails with CORS errors: CORS isn't enabled for the docs origin, or the server URL points at a different origin.
- Spec rejected by tooling: validate the JSON; common culprits are circular references without lazy types and duplicate schema names from identically named DTO classes.
- Duplicate or odd schema names (`CreateUserDto` vs `CreateUserDto_1`): two classes share a name; rename or set explicit `@ApiExtraModels`/schema names.

## Quick Summary

- `DocumentBuilder` + `SwaggerModule.createDocument` + `SwaggerModule.setup('docs', app, document)` gives you the UI and `docs-json`/`docs-yaml`.
- Create the document **after** global configuration; use `include` for per-module/version documents and `operationIdFactory` for clean operation ids.
- The **CLI plugin** (with `classValidatorShim` and `introspectComments`) removes most `@ApiProperty` boilerplate, but needs wiring for Jest/SWC and file-name suffixes; explicit decorators still cover the edge cases.
- Set accurate `addServer` entries; treat the document as sensitive: disable or protect it in production, or publish it through your docs site.
- Validate the generated spec in CI; docs hiding isn't security.

## Next

[Documenting endpoints →](./02-documenting-endpoints.md)
