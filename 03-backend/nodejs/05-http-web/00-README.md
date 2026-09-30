# 05 — HTTP & the Web

Every Node backend, whatever framework sits on top, is ultimately speaking HTTP. This section covers the protocol itself — methods, status codes, headers, cookies, CORS, caching, and compression — independent of any specific framework, since `06-express` builds directly on top of these concepts.

## In this section

| File                                    | Covers                                                                                             |
| --------------------------------------- | -------------------------------------------------------------------------------------------------- |
| `01-http-methods-and-status-codes.md`   | GET/POST/PUT/PATCH/DELETE and what they mean, and the status code ranges that communicate outcomes |
| `02-headers-and-content-negotiation.md` | Request/response headers, and how a client and server agree on response format/language/encoding   |
| `03-cookies.md`                         | How cookies work, their attributes, and local vs cross-origin/production considerations            |
| `04-cors.md`                            | Why cross-origin requests are blocked by default, and how to correctly allow them                  |
| `05-caching-and-compression.md`         | HTTP caching headers, and compressing responses over the wire                                      |

## Why this comes before Express

Express's API (`res.status()`, `res.json()`, `req.headers`, cookie middleware, CORS middleware) is a thin, convenient layer directly over the HTTP concepts in this section. Understanding what a 404 actually means, what `Cache-Control` does, or why CORS exists in the first place makes Express's API feel like a natural shorthand rather than a black box of methods to memorize.

## What you should be able to do after this section

- Choose the correct HTTP method and status code for a given API action
- Read and set the headers that control content type, language, and encoding negotiation
- Understand cookie attributes (`httpOnly`, `secure`, `sameSite`) well enough to configure them correctly for local dev and production
- Explain why a browser blocks a cross-origin request by default, and configure CORS correctly rather than by trial and error
- Apply the right HTTP caching headers, and know when/how to compress a response

## Next

**`06-express`** puts all of this to work through the framework most Node APIs are actually built with.
