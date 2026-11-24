# Module 03: Web MVC & RESTful APIs

Welcome to **Module 03: Web MVC & RESTful APIs**. This module explores modern HTTP API development using Spring MVC: DispatcherServlet request lifecycles, REST controllers, request parameter binding, Jakarta validation, Jackson serialization, global error handling via ProblemDetail (RFC 7807), CORS, file streaming, and embedded Tomcat tuning.

---

## Module Contents

| # | Note | Description | Level | Status |
|---|---|---|---|---|
| 01 | [01-dispatcher-servlet-and-request-lifecycle.md](file:///03-backend/spring-boot/03-web-mvc/01-dispatcher-servlet-and-request-lifecycle.md) | Internals of `DispatcherServlet`, `HandlerMapping`, and `HandlerAdapter` | Advanced | Complete |
| 02 | [02-controllers-and-request-mapping.md](file:///03-backend/spring-boot/03-web-mvc/02-controllers-and-request-mapping.md) | `@RestController`, `@GetMapping`, `@PostMapping`, path matching, matrices | Beginner | Complete |
| 03 | [03-request-binding.md](file:///03-backend/spring-boot/03-web-mvc/03-request-binding.md) | `@PathVariable`, `@RequestParam`, `@RequestBody`, `@RequestHeader`, `@CookieValue` | Beginner | Complete |
| 04 | [04-validation-and-binding-errors.md](file:///03-backend/spring-boot/03-web-mvc/04-validation-and-binding-errors.md) | `@Valid`, `@Validated`, `BindingResult`, MethodArgumentNotValidException | Intermediate | Complete |
| 05 | [05-response-handling-and-content-negotiation.md](file:///03-backend/spring-boot/03-web-mvc/05-response-handling-and-content-negotiation.md) | `ResponseEntity`, `Content-Type`, `Accept` headers, XML/JSON negotiation | Intermediate | Complete |
| 06 | [06-jackson-configuration-and-custom-serialisation.md](file:///03-backend/spring-boot/03-web-mvc/06-jackson-configuration-and-custom-serialisation.md) | `ObjectMapper` tuning, JavaTimeModule, custom serializers/deserializers | Intermediate | Complete |
| 07 | [07-exception-handling.md](file:///03-backend/spring-boot/03-web-mvc/07-exception-handling.md) | `@RestControllerAdvice`, `@ExceptionHandler`, RFC 7807 `ProblemDetail` | Intermediate | Complete |
| 08 | [08-interceptors-filters-and-argument-resolvers.md](file:///03-backend/spring-boot/03-web-mvc/08-interceptors-filters-and-argument-resolvers.md) | `HandlerInterceptor` vs Servlet `Filter` vs `HandlerMethodArgumentResolver` | Advanced | Complete |
| 09 | [09-CORS.md](file:///03-backend/spring-boot/03-web-mvc/09-CORS.md) | Cross-Origin Resource Sharing via `@CrossOrigin` and `WebMvcConfigurer` | Intermediate | Complete |
| 10 | [10-file-upload-and-download-streaming.md](file:///03-backend/spring-boot/03-web-mvc/10-file-upload-and-download-streaming.md) | Multipart uploads (`MultipartFile`), streaming downloads via `StreamingResponseBody` | Intermediate | Complete |
| 11 | [11-REST-design-with-spring.md](file:///03-backend/spring-boot/03-web-mvc/11-REST-design-with-spring.md) | RESTful API design principles, semantic HTTP status codes, resource modeling | Intermediate | Complete |
| 12 | [12-hateoas-overview.md](file:///03-backend/spring-boot/03-web-mvc/12-hateoas-overview.md) | Hypermedia as the Engine of Application State using Spring HATEOAS | Advanced | Complete |
| 13 | [13-API-versioning.md](file:///03-backend/spring-boot/03-web-mvc/13-API-versioning.md) | URI path, request parameter, header, and media-type API versioning strategies | Intermediate | Complete |
| 14 | [14-pagination-sorting-and-filtering-in-web-layer.md](file:///03-backend/spring-boot/03-web-mvc/14-pagination-sorting-and-filtering-in-web-layer.md) | Spring Data `Pageable`, `Sort`, keyset pagination integration in controllers | Intermediate | Complete |
| 15 | [15-async-requests-and-sse.md](file:///03-backend/spring-boot/03-web-mvc/15-async-requests-and-sse.md) | Asynchronous requests (`DeferredResult`, `Callable`), Server-Sent Events (`SseEmitter`) | Advanced | Complete |
| 16 | [16-templates-with-thymeleaf-overview.md](file:///03-backend/spring-boot/03-web-mvc/16-templates-with-thymeleaf-overview.md) | Server-side HTML rendering with Thymeleaf expressions and layouts | Beginner | Complete |
| 17 | [17-static-resources-and-caching-headers.md](file:///03-backend/spring-boot/03-web-mvc/17-static-resources-and-caching-headers.md) | Serving static files, `Cache-Control`, ETag headers, resource versioning | Intermediate | Complete |
| 18 | [18-embedded-servers-tuning.md](file:///03-backend/spring-boot/03-web-mvc/18-embedded-servers-tuning.md) | Tuning embedded Tomcat, Jetty, and Undertow thread pools and buffer limits | Advanced | Complete |

---

## Next Steps
Proceed to **[Module 04: HTTP Clients](file:///03-backend/spring-boot/04-http-clients/)** to master synchronous and declarative outbound HTTP communication.
