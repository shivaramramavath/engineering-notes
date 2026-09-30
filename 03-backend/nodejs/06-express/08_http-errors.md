# http-errors

**Version:** 2.0.1 · **License:** MIT · **Repo:** github.com/jshttp/http-errors

A small Node.js utility for creating HTTP error objects, commonly used with Express, Koa, Connect, and similar frameworks.

## Install

```bash
npm install http-errors
```

## Basic Usage

```js
var createError = require("http-errors");
var express = require("express");
var app = express();

app.use(function (req, res, next) {
  if (!req.user)
    return next(createError(401, "Please login to view this page."));
  next();
});
```

## Error Properties

| Property     | Description                                                                                                    |
| ------------ | -------------------------------------------------------------------------------------------------------------- |
| `expose`     | Signals whether the error message should be sent to the client. Defaults to `false` for status codes `>= 500`. |
| `headers`    | An object of header names to values sent to the client. Keys should be lower-cased. Defaults to `undefined`.   |
| `message`    | The error message text; should be short and single-line.                                                       |
| `status`     | The numeric status code (mirrors `statusCode` for compatibility).                                              |
| `statusCode` | The numeric status code. Defaults to `500`.                                                                    |

## API

### `createError([status], [message], [properties])`

Creates a new error object with the given message. The object inherits from `createError.HttpError`.

```js
var err = createError(404, "This video does not exist!");
```

- `status` (number, default `500`) — the status code
- `message` — defaults to the standard text for that status code if omitted
- `properties` — custom properties to attach to the error

### `createError([status], [error], [properties])`

Wraps and extends an existing error object with `HttpError` properties, without changing its prototype chain. Returns the modified object.

```js
fs.readFile("foo.txt", function (err, buf) {
  if (err) {
    if (err.code === "ENOENT") {
      var httpError = createError(404, err, { expose: false });
    } else {
      var httpError = createError(500, err);
    }
  }
});
```

- `status` — the status code
- `error` — the error object being extended
- `properties` — custom properties to attach

### `createError.isHttpError(val)`

Checks whether `val` is an `HttpError` — either because it inherits from this module's `HttpError` constructor, or because it structurally matches ("duck types") an error this module produces. Any error created via the factory returns `true`, even if a non-`HttpError` was passed in.

### `new createError[code || name]([msg])`

Creates a new error via a named constructor rather than the factory function.

```js
var err = new createError.NotFound();
```

- `code` — the numeric status code
- `name` — the "bumpy case" name of the error, e.g. `NotFound` or `InternalServerError`

### Named Constructors with Custom Messages

Each status code constructor can also take a custom message, and the `new` keyword is optional:

```js
throw new createError.NotFound("User not found");
throw createError.NotFound("User not found");
throw createError.BadRequest("Invalid email address");
throw createError.Unauthorized("Please log in first");
throw createError.Forbidden("You do not have access to this resource");
throw createError.Conflict("Username already taken");
throw createError.TooManyRequests("Rate limit exceeded, try again later");
throw createError.InternalServerError("Something went wrong");
```

Example inside an Express route:

```js
app.get("/users/:id", function (req, res, next) {
  var user = findUserById(req.params.id);
  if (!user) {
    return next(createError.NotFound("User not found"));
  }
  res.json(user);
});
```

## Status Code Constructors

| Status Code | Constructor Name              |
| ----------- | ----------------------------- |
| 400         | BadRequest                    |
| 401         | Unauthorized                  |
| 402         | PaymentRequired               |
| 403         | Forbidden                     |
| 404         | NotFound                      |
| 405         | MethodNotAllowed              |
| 406         | NotAcceptable                 |
| 407         | ProxyAuthenticationRequired   |
| 408         | RequestTimeout                |
| 409         | Conflict                      |
| 410         | Gone                          |
| 411         | LengthRequired                |
| 412         | PreconditionFailed            |
| 413         | PayloadTooLarge               |
| 414         | URITooLong                    |
| 415         | UnsupportedMediaType          |
| 416         | RangeNotSatisfiable           |
| 417         | ExpectationFailed             |
| 418         | ImATeapot                     |
| 421         | MisdirectedRequest            |
| 422         | UnprocessableEntity           |
| 423         | Locked                        |
| 424         | FailedDependency              |
| 425         | TooEarly                      |
| 426         | UpgradeRequired               |
| 428         | PreconditionRequired          |
| 429         | TooManyRequests               |
| 431         | RequestHeaderFieldsTooLarge   |
| 451         | UnavailableForLegalReasons    |
| 500         | InternalServerError           |
| 501         | NotImplemented                |
| 502         | BadGateway                    |
| 503         | ServiceUnavailable            |
| 504         | GatewayTimeout                |
| 505         | HTTPVersionNotSupported       |
| 506         | VariantAlsoNegotiates         |
| 507         | InsufficientStorage           |
| 508         | LoopDetected                  |
| 509         | BandwidthLimitExceeded        |
| 510         | NotExtended                   |
| 511         | NetworkAuthenticationRequired |

## Links

- npm: https://www.npmjs.com/package/http-errors
- Repository: https://github.com/jshttp/http-errors
