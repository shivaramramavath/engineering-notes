# API Design & Versioning

> **TL;DR**: Design APIs **contract-first** (OpenAPI / Protobuf), prefer **additive evolution** over breaking versions, paginate every collection endpoint (cursor pagination for scale), and require `Idempotency-Key` on non-idempotent mutations.

## Versioning Strategies
1. **URI Path (`/v1/orders`, `/v2/orders`)**: Explicit, easy to route at API Gateways.
2. **Header / Media-Type (`Accept: application/vnd.api+json;version=2`)**: Cleaner URLs, slightly harder to test in browsers.
3. **Best Strategy**: Avoid breaking changes in the first place—add new optional fields rather than renaming/removing existing ones!
