# HTTP Status Codes Reference

## 2XX - Success

| Status Code | Constructor Name            | When to Use                                                                                                                |
| ----------- | --------------------------- | -------------------------------------------------------------------------------------------------------------------------- |
| 200         | OK                          | The request succeeded; the standard response for a successful request.                                                     |
| 201         | Created                     | The request succeeded and a new resource was created as a result.                                                          |
| 202         | Accepted                    | The request has been accepted for processing, but processing is not yet complete.                                          |
| 203         | NonAuthoritativeInformation | The returned metadata is from a local or third-party copy, not the origin server.                                          |
| 204         | NoContent                   | The request succeeded but there is no content to return (e.g., after a delete).                                            |
| 205         | ResetContent                | The request succeeded; the client should reset the document view that sent the request.                                    |
| 206         | PartialContent              | The server is delivering only part of the resource due to a range header sent by the client.                               |
| 207         | MultiStatus                 | Conveys information about multiple resources in situations where multiple status codes may be appropriate (WebDAV).        |
| 208         | AlreadyReported             | The members of a DAV binding have already been enumerated in a previous reply (WebDAV).                                    |
| 226         | IMUsed                      | The server fulfilled the request and the response is a representation of the result of one or more instance manipulations. |

## 3XX - Redirection

| Status Code | Constructor Name  | When to Use                                                                                    |
| ----------- | ----------------- | ---------------------------------------------------------------------------------------------- |
| 300         | MultipleChoices   | The request has more than one possible response; the client should choose one.                 |
| 301         | MovedPermanently  | The resource has been permanently moved to a new URL.                                          |
| 302         | Found             | The resource temporarily resides at a different URL; use the original URL for future requests. |
| 303         | SeeOther          | The client should retrieve the resource from a different URL using a GET request.              |
| 304         | NotModified       | The resource has not changed since the last request; the client can use its cached version.    |
| 305         | UseProxy          | The requested resource must be accessed through the proxy given in the response (deprecated).  |
| 306         | Unused            | Reserved; no longer used (originally "Switch Proxy").                                          |
| 307         | TemporaryRedirect | The resource temporarily resides at a different URL; the original HTTP method must be reused.  |
| 308         | PermanentRedirect | The resource has permanently moved to a new URL; the original HTTP method must be reused.      |

## 4XX - Client Errors

| Status Code | Constructor Name            | When to Use                                                                                            |
| ----------- | --------------------------- | ------------------------------------------------------------------------------------------------------ |
| 400         | BadRequest                  | The request is malformed, has invalid syntax, or contains invalid parameters.                          |
| 401         | Unauthorized                | The client must authenticate itself to get the requested response (missing/invalid credentials).       |
| 402         | PaymentRequired             | Reserved for future use; some APIs use it to indicate payment is required to access the resource.      |
| 403         | Forbidden                   | The client is authenticated but does not have permission to access the resource.                       |
| 404         | NotFound                    | The requested resource could not be found on the server.                                               |
| 405         | MethodNotAllowed            | The HTTP method used is not supported for the requested resource.                                      |
| 406         | NotAcceptable               | The server cannot produce a response matching the list of acceptable values in the request headers.    |
| 407         | ProxyAuthenticationRequired | The client must authenticate with a proxy before the request can proceed.                              |
| 408         | RequestTimeout              | The server timed out waiting for the request from the client.                                          |
| 409         | Conflict                    | The request conflicts with the current state of the target resource (e.g., duplicate entry).           |
| 410         | Gone                        | The requested resource is no longer available and has been permanently removed.                        |
| 411         | LengthRequired              | The server requires a `Content-Length` header that was not provided.                                   |
| 412         | PreconditionFailed          | One or more conditions in the request headers were not met by the server.                              |
| 413         | PayloadTooLarge             | The request payload is larger than the server is willing or able to process.                           |
| 414         | URITooLong                  | The URI provided in the request is too long for the server to process.                                 |
| 415         | UnsupportedMediaType        | The media format of the requested data is not supported by the server.                                 |
| 416         | RangeNotSatisfiable         | The range specified in the `Range` header cannot be fulfilled.                                         |
| 417         | ExpectationFailed           | The server cannot meet the requirements of the `Expect` request header.                                |
| 418         | ImATeapot                   | A joke status code from RFC 2324 (April Fools); sometimes used for easter eggs or to signal a refusal. |
| 421         | MisdirectedRequest          | The request was directed at a server unable to produce a response.                                     |
| 422         | UnprocessableEntity         | The request is well-formed but contains semantic errors (e.g., failed validation).                     |
| 423         | Locked                      | The resource being accessed is locked.                                                                 |
| 424         | FailedDependency            | The request failed because it depended on another request that failed.                                 |
| 425         | TooEarly                    | The server is unwilling to risk processing a request that might be replayed.                           |
| 426         | UpgradeRequired             | The client should switch to a different protocol (e.g., upgrade to HTTPS/WebSocket).                   |
| 428         | PreconditionRequired        | The origin server requires the request to be conditional to prevent lost updates.                      |
| 429         | TooManyRequests             | The client has sent too many requests in a given time (rate limiting).                                 |
| 431         | RequestHeaderFieldsTooLarge | The request's header fields are too large for the server to process.                                   |
| 451         | UnavailableForLegalReasons  | The resource is unavailable due to legal restrictions (e.g., censorship).                              |

## 5XX - Server Errors

| Status Code | Constructor Name              | When to Use                                                                                     |
| ----------- | ----------------------------- | ----------------------------------------------------------------------------------------------- |
| 500         | InternalServerError           | A generic error indicating something went wrong on the server.                                  |
| 501         | NotImplemented                | The server does not support the functionality required to fulfill the request.                  |
| 502         | BadGateway                    | The server, acting as a gateway/proxy, received an invalid response from the upstream server.   |
| 503         | ServiceUnavailable            | The server is temporarily unable to handle the request (overload or maintenance).               |
| 504         | GatewayTimeout                | The server, acting as a gateway/proxy, did not get a response in time from the upstream server. |
| 505         | HTTPVersionNotSupported       | The server does not support the HTTP protocol version used in the request.                      |
| 506         | VariantAlsoNegotiates         | The server has an internal configuration error in content negotiation.                          |
| 507         | InsufficientStorage           | The server cannot store the representation needed to complete the request.                      |
| 508         | LoopDetected                  | The server detected an infinite loop while processing the request.                              |
| 509         | BandwidthLimitExceeded        | The server has exceeded its allocated bandwidth limit (non-standard, used by some hosts).       |
| 510         | NotExtended                   | Further extensions to the request are required for the server to fulfill it.                    |
| 511         | NetworkAuthenticationRequired | The client needs to authenticate to gain network access (e.g., captive portal).                 |
