# Module 02: Spring Boot Fundamentals & Auto-Configuration

Welcome to **Module 02: Spring Boot Fundamentals & Auto-Configuration**. This module deconstructs how Spring Boot achieves convention over configuration: auto-configuration conditions (`@ConditionalOnClass`, `@ConditionalOnMissingBean`), starter dependencies, `@ConfigurationProperties` binding, native structured logging, Actuator, and custom starter creation.

---

## Module Contents

| # | Note | Description | Level | Status |
|---|---|---|---|---|
| 01 | [01-how-auto-configuration-works.md](file:///03-backend/spring-boot/02-boot-fundamentals/01-how-auto-configuration-works.md) | Auto-configuration discovery, `AutoConfiguration.imports`, conditions evaluation | Advanced | Complete |
| 02 | [02-starters-and-dependency-management.md](file:///03-backend/spring-boot/02-boot-fundamentals/02-starters-and-dependency-management.md) | Spring Boot Starter BOM, transitive dependency management, exclusions | Beginner | Complete |
| 03 | [03-externalized-configuration-and-property-binding.md](file:///03-backend/spring-boot/02-boot-fundamentals/03-externalized-configuration-and-property-binding.md) | `@ConfigurationProperties`, immutable records binding, relaxed binding | Intermediate | Complete |
| 04 | [04-profiles-and-config-files-yaml-properties.md](file:///03-backend/spring-boot/02-boot-fundamentals/04-profiles-and-config-files-yaml-properties.md) | Profile-specific config (`application-{profile}.yaml`), precedence rules | Intermediate | Complete |
| 05 | [05-config-data-import-and-config-trees.md](file:///03-backend/spring-boot/02-boot-fundamentals/05-config-data-import-and-config-trees.md) | `spring.config.import`, volume mounted config trees in Kubernetes, cloud config | Advanced | Complete |
| 06 | [06-logging-with-logback-and-structured-logging.md](file:///03-backend/spring-boot/02-boot-fundamentals/06-logging-with-logback-and-structured-logging.md) | Logback tuning, Boot 3.4+ native structured logging (ECS/Logstash/GELF) | Intermediate | Complete |
| 07 | [07-application-lifecycle-and-runners.md](file:///03-backend/spring-boot/02-boot-fundamentals/07-application-lifecycle-and-runners.md) | `CommandLineRunner`, `ApplicationRunner`, startup hooks, exit codes | Beginner | Complete |
| 08 | [08-custom-auto-configuration-and-starters.md](file:///03-backend/spring-boot/02-boot-fundamentals/08-custom-auto-configuration-and-starters.md) | Creating production-grade custom auto-configurations and starter libraries | Advanced | Complete |
| 09 | [09-banner-and-spring-application-customisation.md](file:///03-backend/spring-boot/02-boot-fundamentals/09-banner-and-spring-application-customisation.md) | Customizing `SpringApplicationBuilder`, programmatic banners, headless modes | Beginner | Complete |
| 10 | [10-graceful-shutdown.md](file:///03-backend/spring-boot/02-boot-fundamentals/10-graceful-shutdown.md) | Configuring `server.shutdown=graceful`, in-flight request draining on SIGTERM | Intermediate | Complete |
| 11 | [11-actuator-fundamentals.md](file:///03-backend/spring-boot/02-boot-fundamentals/11-actuator-fundamentals.md) | Actuator endpoints (`/health`, `/info`, `/env`, `/metrics`), security exposure | Intermediate | Complete |
| 12 | [12-devtools-and-hot-reload.md](file:///03-backend/spring-boot/02-boot-fundamentals/12-devtools-and-hot-reload.md) | How DevTools manages dual classloaders (base vs restart) and cache clearing | Intermediate | Complete |
| 13 | [13-configuration-metadata.md](file:///03-backend/spring-boot/02-boot-fundamentals/13-configuration-metadata.md) | Generating `additional-spring-configuration-metadata.json` for IDE autocomplete | Intermediate | Complete |
| 14 | [14-conditional-beans-recipes.md](file:///03-backend/spring-boot/02-boot-fundamentals/14-conditional-beans-recipes.md) | `@ConditionalOnProperty`, `@ConditionalOnExpression`, custom condition classes | Advanced | Complete |

---

## Next Steps
Proceed to **[Module 03: Web MVC & RESTful APIs](file:///03-backend/spring-boot/03-web-mvc/)** to master HTTP routing, validation, content negotiation, and error handling.
