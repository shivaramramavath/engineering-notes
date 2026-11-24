# Module 04: Routing & Application Architecture

Welcome to **Module 04: Routing & Application Architecture**. This module explores scalable modularization, APIRouter composition, Lifespan context managers, and custom route handlers.

---

## Module Overview

| File | Title | Description |
| :--- | :--- | :--- |
| [01-apirouter-and-modular-apps.md](./01-apirouter-and-modular-apps.md) | **APIRouter & Modularity** | Structuring large projects with decomposed routers. |
| [02-router-prefixes-tags-and-deps.md](./02-router-prefixes-tags-and-deps.md) | **Prefixes, Tags & Deps** | Grouping routes, assigning OpenAPI tags, and bulk guards. |
| [03-large-application-layout.md](./03-large-application-layout.md) | **Large Application Layout** | Enterprise folder structure (Domain/Feature vs Layered). |
| [04-mounting-subapps-and-static-files.md](./04-mounting-subapps-and-static-files.md) | **Mounting Sub-Apps & Static** | Serving static assets and mounting isolated WSGI/ASGI sub-apps. |
| [05-versioning-strategies.md](./05-versioning-strategies.md) | **API Versioning Strategies** | URI path, header, and query parameter versioning schemes. |
| [06-lifespan-events.md](./06-lifespan-events.md) | **Lifespan Events Architecture** | Modern `@asynccontextmanager` startup and teardown handling. |
| [07-app-state-and-singletons.md](./07-app-state-and-singletons.md) | **App State & Singletons** | Safely attaching shared connection pools to `app.state`. |
| [08-settings-and-config-management.md](./08-settings-and-config-management.md) | **Configuration Management** | Multi-environment staging/production settings patterns. |
| [09-custom-response-classes-and-orjson.md](./09-custom-response-classes-and-orjson.md) | **Custom Response & orjson** | Accelerating JSON serialization with ORJSONResponse. |
| [10-custom-routes-and-route-classes.md](./10-custom-routes-and-route-classes.md) | **Custom Route Classes** | Overriding `APIRoute` for automated timing, logging, and error handling. |
