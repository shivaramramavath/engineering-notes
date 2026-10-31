# Chain of Responsibility

**Intent:** avoid coupling the sender of a request to its receiver by giving more than one object a chance to handle it. Chain the receiving objects and pass the request along until one handles it (or all have had a look).

## The problem

A request may need to be checked or handled by several parties, and which one acts depends on the request. Hard-coding that logic creates a long conditional:

```java
void handle(Request r) {
    if (!authenticated(r)) { reject(r); return; }
    if (!rateLimitOk(r))   { reject(r); return; }
    if (!validPayload(r))  { reject(r); return; }
    process(r);                      // adding a step edits this method
}
```

The order, the steps and the stopping rules are tangled in one place.

## Structure

```text
Client ──► Handler1 ──► Handler2 ──► Handler3 ──► (end)
              │             │            │
         handle or     handle or     handle or
         pass on       pass on       pass on
```

- **Handler**: common interface with a method to handle the request and a link to the next handler.
- **Concrete handlers**: each decides to process the request, stop it, or pass it on.

## Example: request pipeline

```java
public abstract class RequestHandler {
    private RequestHandler next;

    public RequestHandler linkWith(RequestHandler next) {
        this.next = next;
        return next;                               // allows fluent chaining
    }

    public abstract boolean handle(Request request);

    protected boolean passOn(Request request) {
        return next == null || next.handle(request);
    }
}

public class AuthHandler extends RequestHandler {
    public boolean handle(Request r) {
        if (!r.hasValidToken()) { System.out.println("Unauthenticated"); return false; }   // stop the chain
        return passOn(r);
    }
}

public class RateLimitHandler extends RequestHandler {
    public boolean handle(Request r) {
        if (tooManyRequests(r.clientId())) { System.out.println("Rate limited"); return false; }
        return passOn(r);
    }
    private boolean tooManyRequests(String clientId) { /* ... */ return false; }
}

public class ValidationHandler extends RequestHandler {
    public boolean handle(Request r) {
        if (r.body() == null) { System.out.println("Invalid body"); return false; }
        return passOn(r);
    }
}

// Build the chain
RequestHandler chain = new AuthHandler();
chain.linkWith(new RateLimitHandler()).linkWith(new ValidationHandler());

boolean ok = chain.handle(request);
```

Reordering or adding a step is a wiring change. Each handler has one job ([SRP](../02-solid-principles/01-single-responsibility.md)).

## Two styles

| Style | Behavior | Example |
|---|---|---|
| **Single handler** | The first handler that can deal with it handles it, and the chain stops | Support escalation (agent → manager → director), exception handling |
| **Pipeline** | Every handler does its part and passes it on, any can abort | Servlet filters, middleware, validation steps |

Most real-world uses are the pipeline style.

## Compact form with a list

For simple cases, skip the linked handlers and use a list:

```java
public interface Check { boolean passes(Request r); }

public class Pipeline {
    private final List<Check> checks;
    public Pipeline(List<Check> checks) { this.checks = List.copyOf(checks); }

    public boolean run(Request r) {
        return checks.stream().allMatch(c -> c.passes(r));    // stops at the first failure
    }
}
```

This is easier to build and test, and it's often all you need.

## Where you've seen it

- **Servlet filters** and **web middleware** (authentication, logging, compression, CORS).
- Spring Security's filter chain.
- Logging frameworks (a message passes through appenders and filters).
- UI event bubbling (an event goes up the component tree until someone handles it).
- Exception handling: each `catch` clause is a handler in a chain.

## Chain of Responsibility vs Decorator

Both chain objects that share an interface.

| | Chain of Responsibility | [Decorator](../05-structural-patterns/04-decorator.md) |
|---|---|---|
| Can stop the flow? | **Yes**, a handler may refuse to pass on | No, always delegates |
| Purpose | Decide who handles / whether to continue | Add behavior around a call |
| Result | Possibly handled by exactly one | All layers participate |

## Benefits and costs

| Benefits | Costs |
|---|---|
| Decouples sender from receivers | A request may end up **unhandled** |
| Add, remove or reorder handlers easily | Harder to debug: flow is spread over many objects |
| Each handler has a single responsibility | Behavior depends on **chain order** |
| Open/Closed friendly | Long chains add overhead |

## Common mistakes

- **Forgetting to pass the request on**, silently stopping the chain.
- **No default handler** for requests nobody handles, so failures go unnoticed.
- **Order-dependent logic left undocumented** (for example authentication must come before authorization).
- **Handlers sharing mutable state** unsafely.
- **Cycles** in the chain, causing infinite loops.
- **Handlers that know too much about others**, which defeats the decoupling.
- **Using it where a simple ordered list** or direct call would do.

## Debugging

- Log at each handler (entry, decision, exit) to trace the path of a request.
- Name handlers clearly so stack traces and logs are readable.
- Log the full chain order at startup.

## Quick Summary

- Chain of Responsibility passes a request along handlers, each of which may act, stop or pass on.
- Pipelines (filters, middleware) are the most common form.
- Order matters, so document it and test it.
- It differs from Decorator because handlers may stop the flow.

## Related

[Decorator](../05-structural-patterns/04-decorator.md) · [Command](./03-command.md) · [Mediator](./08-mediator.md)

## Next

[07-iterator.md](./07-iterator.md)
