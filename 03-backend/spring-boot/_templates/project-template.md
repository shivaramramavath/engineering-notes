# Production Reference Architecture: [Project Name]

## Metadata & Production Baseline

| Attribute | Specification |
|:---|:---|
| **Project Code** | `SPRING-PROJ-[XX]` |
| **Spring Boot Version** | `3.4.x` / `4.x` |
| **Spring Framework Version** | `6.2.x` / `7.0.x` |
| **Java Baseline** | `Java 21` / `Java 25 LTS` |
| **Build Tool** | `Maven 3.9+` (with Gradle equivalent) |
| **Database & ORM** | `PostgreSQL 16+`, `Spring Data JPA (Hibernate 6.6+)`, `Flyway` |
| **Messaging & Cache** | `Apache Kafka` / `Redis 7.2+` |
| **Testing** | `JUnit 5.11+`, `AssertJ`, `Testcontainers 1.20+`, `MockMvc` |
| **Complexity Level** | [Intermediate / Advanced / Enterprise] |

---

## 1. Architectural Overview & Domain Design

### 1.1 System Architecture & Data Flow

```
                      +---------------------------------------+
                      |         Web / Mobile Client           |
                      +-------------------+-------------------+
                                          |
                                          v
                      +-------------------+-------------------+
                      |      Spring Boot API Gateway          |
                      +-------------------+-------------------+
                                          |
                                          v
                      +-------------------+-------------------+
                      |     Domain Core (Hexagonal / DDD)     |
                      +-------------------+-------------------+
                                          |
                        +-----------------+-----------------+
                        |                                   |
                        v                                   v
             +----------+----------+             +----------+----------+
             | PostgreSQL Database |             | Apache Kafka / Redis|
             +---------------------+             +---------------------+
```

### 1.2 Directory Layout

```
[project-name]/
|-- pom.xml
|-- Dockerfile
|-- compose.yaml
`-- src/
    |-- main/
    |   |-- java/com/example/app/
    |   |   |-- Application.java
    |   |   |-- config/
    |   |   |-- domain/
    |   |   |-- repository/
    |   |   |-- service/
    |   |   `-- web/
    |   `-- resources/
    |       |-- application.yaml
    |       `-- db/migration/
    `-- test/
        `-- java/com/example/app/
```

---

## 2. Complete Production Implementation

### 2.1 Build Configuration (`pom.xml`)
```xml
<!-- Complete compilable pom.xml -->
```

### 2.2 Application Configuration (`src/main/resources/application.yaml`)
```yaml
# Complete application.yaml
```

### 2.3 Domain Entities & DTO Records (`src/main/java/.../domain/`)
```java
// Complete domain code
```

### 2.4 Repositories & Data Access (`src/main/java/.../repository/`)
```java
// Complete repository code
```

### 2.5 Business Services (`src/main/java/.../service/`)
```java
// Complete service code
```

### 2.6 REST Controllers & Error Handling (`src/main/java/.../web/`)
```java
// Complete controller code
```

---

## 3. Automated Testing Suite

### 3.1 Integration Test with Testcontainers (`src/test/java/.../IntegrationTest.java`)
```java
// Complete integration test code
```

### 3.2 Unit & Slice Tests (`src/test/java/.../WebLayerTest.java`)
```java
// Complete slice test code
```

---

## 4. Production Infrastructure & Deployment

### 4.1 Multi-Stage `Dockerfile`
```dockerfile
# Multi-stage Dockerfile
```

### 4.2 `compose.yaml` (Docker Compose)
```yaml
# Docker compose configuration
```

---

## 5. Operational Runbook & Verification

### 5.1 Verification Commands
```bash
# cURL and execution commands
```
