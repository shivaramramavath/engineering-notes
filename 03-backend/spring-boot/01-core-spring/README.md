# Module 01: Core Spring Framework & IoC Container

Welcome to **Module 01: Core Spring Framework & IoC Container**. This module delves into the heart of Spring: the Inversion of Control container, bean creation, dependency injection mechanisms, bean post-processors, CGLIB vs JDK dynamic proxies, SpEL, and ApplicationContext refresh internals.

---

## Module Contents

| # | Note | Description | Level | Status |
|---|---|---|---|---|
| 01 | [01-inversion-of-control-and-the-container.md](file:///03-backend/spring-boot/01-core-spring/01-inversion-of-control-and-the-container.md) | IoC principles, `BeanFactory` vs `ApplicationContext`, container hierarchy | Beginner | Complete |
| 02 | [02-beans-and-bean-definitions.md](file:///03-backend/spring-boot/01-core-spring/02-beans-and-bean-definitions.md) | `BeanDefinition` metadata, role, lazy initialization, primary, and depends-on | Intermediate | Complete |
| 03 | [03-dependency-injection-styles.md](file:///03-backend/spring-boot/01-core-spring/03-dependency-injection-styles.md) | Constructor injection vs Setter vs Field injection (`@Autowired`), immutability | Beginner | Complete |
| 04 | [04-component-scanning-and-stereotypes.md](file:///03-backend/spring-boot/01-core-spring/04-component-scanning-and-stereotypes.md) | `@Component`, `@Service`, `@Repository`, `@Controller`, filter patterns | Beginner | Complete |
| 05 | [05-java-config-vs-component-config.md](file:///03-backend/spring-boot/01-core-spring/05-java-config-vs-component-config.md) | `@Configuration` and `@Bean` methods, inter-bean references, `@Import` | Intermediate | Complete |
| 06 | [06-bean-scopes-and-lifecycle.md](file:///03-backend/spring-boot/01-core-spring/06-bean-scopes-and-lifecycle.md) | Singleton, Prototype, Request, Session scopes, `@PostConstruct`, `@PreDestroy` | Intermediate | Complete |
| 07 | [07-bean-post-processors-and-factory-post-processors.md](file:///03-backend/spring-boot/01-core-spring/07-bean-post-processors-and-factory-post-processors.md) | `BeanPostProcessor` vs `BeanFactoryPostProcessor`, proxy injection hooks | Advanced | Complete |
| 08 | [08-qualifiers-primary-and-optional-injection.md](file:///03-backend/spring-boot/01-core-spring/08-qualifiers-primary-and-optional-injection.md) | Disambiguation via `@Qualifier`, `@Primary`, custom qualifier annotations, `Optional` | Intermediate | Complete |
| 09 | [09-profiles-and-environment-abstraction.md](file:///03-backend/spring-boot/01-core-spring/09-profiles-and-environment-abstraction.md) | `@Profile`, Environment property sources, profile activation priority | Intermediate | Complete |
| 10 | [10-application-events.md](file:///03-backend/spring-boot/01-core-spring/10-application-events.md) | In-process event publishing with `ApplicationEventPublisher`, `@EventListener` | Intermediate | Complete |
| 11 | [11-resources-and-resource-loading.md](file:///03-backend/spring-boot/01-core-spring/11-resources-and-resource-loading.md) | `ResourceLoader`, `classpath:`, `file:`, protocol resolvers | Beginner | Complete |
| 12 | [12-SpEL-expression-language.md](file:///03-backend/spring-boot/01-core-spring/12-SpEL-expression-language.md) | Spring Expression Language syntax, parser context, security, evaluation | Advanced | Complete |
| 13 | [13-AOP-concepts-and-proxies.md](file:///03-backend/spring-boot/01-core-spring/13-AOP-concepts-and-proxies.md) | Aspect-Oriented Programming theory, JDK Dynamic vs CGLIB proxy mechanics | Advanced | Complete |
| 14 | [14-aspects-in-practice.md](file:///03-backend/spring-boot/01-core-spring/14-aspects-in-practice.md) | Custom annotations, `@Around`, `@Before`, JoinPoints, audit logging aspects | Advanced | Complete |
| 15 | [15-validation-with-jakarta-bean-validation.md](file:///03-backend/spring-boot/01-core-spring/15-validation-with-jakarta-bean-validation.md) | Jakarta Bean Validation, `@Valid`, `@Validated`, custom constraint validators | Intermediate | Complete |
| 16 | [16-type-conversion-and-formatting.md](file:///03-backend/spring-boot/01-core-spring/16-type-conversion-and-formatting.md) | `ConversionService`, `Converter<S, T>`, `Formatter<T>`, custom binders | Intermediate | Complete |
| 17 | [17-application-context-internals.md](file:///03-backend/spring-boot/01-core-spring/17-application-context-internals.md) | Step-by-step walkthrough of `AbstractApplicationContext.refresh()` phases | Expert | Complete |
| 18 | [18-circular-dependencies.md](file:///03-backend/spring-boot/01-core-spring/18-circular-dependencies.md) | Three-level cache resolution, Boot 2.6+ prohibition, `@Lazy` trade-offs | Advanced | Complete |
| 19 | [19-lazy-beans-and-startup-cost.md](file:///03-backend/spring-boot/01-core-spring/19-lazy-beans-and-startup-cost.md) | Lazy initialization (`spring.main.lazy-initialization=true`), trade-offs | Intermediate | Complete |

---

## Next Steps
Proceed to **[Module 02: Spring Boot Fundamentals](file:///03-backend/spring-boot/02-boot-fundamentals/)** to learn how auto-configuration builds on the core container.
