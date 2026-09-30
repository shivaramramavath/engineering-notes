# 06 — Express

Express is the framework most Node APIs are built with — a thin, well-designed layer directly over the `http` module and HTTP concepts covered in `02-core-modules/03-http.md` and `05-http-web/`. This section covers building a real Express application, piece by piece.

## In this section

| File                           | Covers                                                                                                       |
| ------------------------------ | ------------------------------------------------------------------------------------------------------------ |
| `01-setup-and-routing.md`      | Installing Express, the basic app structure, and defining routes (including params, routers)                 |
| `02-middleware.md`             | What middleware actually is, the request/response/next model, and common built-in and third-party middleware |
| `03-controllers.md`            | Organizing route logic into controllers, separate from routing itself                                        |
| `04-error-handling.md`         | Express-specific error handling — centralized error middleware, async error handling                         |
| `05-validation.md`             | Validating request input before it reaches your business logic                                               |
| `06-auth-and-authorization.md` | Authenticating requests and authorizing what an authenticated user can do                                    |
| `07-file-upload.md`            | Handling file uploads with `multer`                                                                          |
| `08_http-errors.md`            | Build in methods from standard errors with `http-errors`                                                     |

## Why Express, and why now

Everything Express provides — routing, middleware, `res.json()`, error handling — is a convenience layer over concepts already covered: HTTP methods and status codes (`05-http-web/01-...`), headers (`05-http-web/02-...`), and the raw `http` module (`02-core-modules/03-http.md`). Nothing here is magic; it's the same ideas, packaged for productivity.

## What you should be able to do after this section

- Set up an Express app with organized, parameterized routes
- Write and correctly order middleware, including error-handling middleware
- Separate routing from business logic using controllers
- Handle both synchronous and asynchronous errors correctly, with one centralized handler
- Validate incoming request data before it reaches your application logic
- Protect routes with authentication, and restrict specific actions with authorization
- Accept file uploads safely

## Next

**`07-databases`** covers connecting an Express app to a real data store — MongoDB, PostgreSQL, and Redis.
