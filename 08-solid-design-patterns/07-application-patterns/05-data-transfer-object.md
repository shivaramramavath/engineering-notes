# Data Transfer Object (DTO)

**Intent:** carry data between processes or layers using a simple object shaped for that transfer, so that other layers don't depend on your internal domain model.

A DTO is plain data: fields, no business behavior. It defines **what crosses a boundary** (an HTTP API, a message queue, a UI, a service call), independent of how the data is stored or modeled internally.

## The problem

If you return domain entities directly:

```java
@GetMapping("/users/{id}")
public User getUser(@PathVariable long id) {
    return userRepository.findById(id).orElseThrow();     // entity goes straight to JSON
}
```

Problems:

| Problem | Consequence |
|---|---|
| **Leaks internals and sensitive data** | Password hashes, internal flags and audit fields end up in the response |
| **Couples clients to your model** | Renaming a field breaks every API consumer |
| **Lazy-loading surprises** | Serializing triggers database queries, or throws once the session is closed |
| **Cyclic references** | `User ↔ Order` bidirectional relations break or bloat serialization |
| **One shape for everything** | Different callers need different subsets of data |
| **Over/under-fetching** | Too much data, or several calls to assemble a view |

## Structure

```text
Client ◄── JSON ──► Controller ◄── DTO ──► Service ◄──► Domain entities ◄──► Database
                              (mapping happens at the boundary)
```

## Example

Domain entity (internal):

```java
public class User {
    private Long id;
    private String email;
    private String passwordHash;
    private Role role;
    private Instant createdAt;
    private List<Order> orders;       // lazy, large
    // behavior ...
}
```

DTOs shaped for specific uses (records work well: immutable and concise):

```java
public record UserResponse(long id, String email, String displayName) {}

public record CreateUserRequest(String email, String password) {}

public record UserSummary(long id, String displayName, int orderCount) {}
```

Mapping at the boundary:

```java
public final class UserMapper {
    public static UserResponse toResponse(User u) {
        return new UserResponse(u.id(), u.email(), u.displayName());
    }
}

@GetMapping("/users/{id}")
public UserResponse getUser(@PathVariable long id) {
    return UserMapper.toResponse(userService.find(id));
}
```

The API contract is now `UserResponse`. You can refactor `User` freely, and `passwordHash` can never leak because it has no place in the DTO.

## Kinds of DTOs

| Kind | Purpose | Example |
|---|---|---|
| **Request / command** | Input from a client | `CreateUserRequest` |
| **Response / view** | Output to a client | `UserResponse` |
| **Summary / projection** | Read-optimized subset | `UserSummary` |
| **Message / event payload** | Data sent through a queue | `OrderPlacedMessage` |

Separate request and response types are usually better than a single shared one, because inputs and outputs rarely have identical fields (an `id` is returned, not submitted; a password is submitted, not returned).

## Validation

Validate **incoming** DTOs at the boundary, before they reach the domain:

```java
public record CreateUserRequest(
        @NotBlank @Email String email,
        @Size(min = 12) String password) {}
```

This uses Bean Validation annotations (Jakarta Validation). Syntax checks (format, required fields) belong on the DTO. Business rules (is this email already taken?) belong in the service and domain.

## Mapping approaches

| Approach | Notes |
|---|---|
| **Manual mapping methods** | Explicit, simple, easy to debug; verbose for large models |
| **Mapping libraries** (for example MapStruct, which generates mapping code at compile time) | Less boilerplate, compile-time checking |
| **Reflection-based mappers** | Convenient but errors surface at runtime and can hide mistakes |
| **Query projections** | Select directly into a DTO from the database, skipping the entity (efficient for read-only views) |

Whatever you choose, keep mapping **at the boundary** (controller or application service), and keep it free of business rules.

## DTO vs related concepts

| Concept | Difference |
|---|---|
| **Entity** | Has identity and behavior, lives in the domain |
| **Value object** | Domain concept defined by its values, with behavior (`Money`) |
| **DTO** | Pure data for transport, no behavior, no domain meaning |
| **Command / query object** | A DTO that represents an intent (`PlaceOrderCommand`) or a read request |
| **View model** | A DTO shaped for one UI screen |

## Versioning

Public DTOs are API contracts. Plan for change:

- Adding optional fields is usually safe.
- Removing or renaming fields breaks clients: introduce a new version (`/v2/...`) or deprecate gradually.
- Be tolerant when reading (ignore unknown fields) and conservative when writing.

## Where DTOs should and shouldn't appear

```text
Controller  ↔ DTOs        ✔ boundary
Service     ← DTO/Command ✔ acceptable as input; return a DTO or result
Domain      ✘ no DTOs      (domain objects should not know about transport shapes)
Repository  ✘ no DTOs      (except read-only projections returned to services)
```

## When to use them

- You expose an API or send messages outside your process.
- The external shape should differ from the internal model.
- You need to hide fields, flatten structures, or combine data from several sources.

For a small internal application where the entity *is* the contract and exposure isn't a concern, DTOs can be overkill. Even then, avoid serializing entities that have lazy relations or sensitive fields.

## Common mistakes

- **DTO per entity mirroring every field**, with no reason for the separation (the "pointless copy").
- **One huge DTO** reused for create, update and read, with half the fields ignored in each case.
- **Business logic in DTOs**, turning them into a second domain model.
- **Mutating DTOs** passed around layers. Prefer immutable records.
- **Mapping errors**: forgetting a new field in the mapper, or mapping null to a default silently. Cover mappers with tests.
- **Letting DTOs leak into the domain**, coupling business rules to API shapes.
- **Over-fetching**: loading full entity graphs only to copy three fields into a DTO. Use projections.
- **Exposing internal identifiers or enum names** that you can't change later without breaking clients.

## Quick Summary

- A DTO is a plain data carrier that defines what crosses a boundary.
- It protects the domain model from external coupling, and protects clients from internal change.
- Use separate request and response types, and prefer immutable records.
- Map and validate at the boundary; keep business logic out of DTOs.

## Related

[Service Layer](./04-service-layer.md) · [Repository](./01-repository.md) · [Adapter](../05-structural-patterns/01-adapter.md) (mapping is a form of adaptation) · [Layered architecture](../08-architecture/01-layered.md)

## Next

Architecture: [../08-architecture/00-overview.md](../08-architecture/00-overview.md)
