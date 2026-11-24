# Project 13: Enterprise GraphQL API with Spring for GraphQL & BatchMapping

| Metadata Field | Value |
|---|---|
| **Spring Boot Version** | 3.4.2 |
| **Spring Framework** | 6.2.2 |
| **Java Version** | 21 (LTS) |
| **Build System** | Maven 3.9+ |
| **Key Dependencies** | Spring Boot Starter GraphQL, Spring Boot Starter Web, Spring Boot Starter Data JPA, PostgreSQL Driver, Testcontainers |
| **Architecture Style** | Type-Safe GraphQL Schema API with `@BatchMapping` DataLoader N+1 Prevention & Subscriptions |

---

## 1. Architectural Overview & Domain Design

### 1.1 System Architecture

This project implements an enterprise GraphQL API for a project management domain (Projects, Tasks, and Assignees). Clients query hierarchical trees in a single round-trip. To eliminate the standard $N+1$ select problem when querying parent projects and their child tasks, Spring for GraphQL's `@BatchMapping` dynamically batches task fetches into a single SQL query (`WHERE project_id IN (...)`).

```
       [ Client Application (GraphQL Query) ]
                       │
                       │ POST /graphql
                       │ { project(id: 1) { title tasks { title status } } }
                       ▼
   ┌─────────────────────────────────────────────────────────┐
   │            Spring for GraphQL Execution Engine          │
   │                                                         │
   │  ┌───────────────────────────────────────────────────┐  │
   │  │ @QueryMapping: projectById(id)                    │  │
   │  │ -> Fetches 1 Project record                       │  │
   │  └─────────────────────────┬─────────────────────────┘  │
   │                            ▼                            │
   │  ┌───────────────────────────────────────────────────┐  │
   │  │ @BatchMapping: tasks(List<Project> projects)      │  │
   │  │ -> Automatically uses DataLoader under the hood   │  │
   │  │ -> Executes 1 single SQL query for ALL projects:  │  │
   │  │    "SELECT * FROM tasks WHERE project_id IN (?)"  │  │
   │  └─────────────────────────┬─────────────────────────┘  │
   └────────────────────────────┼────────────────────────────┘
                                ▼
   ┌─────────────────────────────────────────────────────────┐
   │             PostgreSQL Database Persistence             │
   │  ┌─────────────────────────┐ ┌────────────────────────┐ │
   │  │ projects table          │ │ tasks table            │ │
   │  │ (id, title, status)     │ │ (id, project_id, title)│ │
   │  └─────────────────────────┘ └────────────────────────┘ │
   └─────────────────────────────────────────────────────────┘
```

### 1.2 Directory Layout

```
13-graphql-api/
├── pom.xml
├── src/
│   ├── main/
│   │   ├── java/com/example/graphql/
│   │   │   ├── GraphQlApiApplication.java
│   │   │   ├── domain/
│   │   │   │   ├── Project.java
│   │   │   │   └── Task.java
│   │   │   ├── repository/
│   │   │   │   ├── ProjectRepository.java
│   │   │   │   └── TaskRepository.java
│   │   │   └── web/
│   │   │       ├── ProjectGraphQlController.java
│   │   │       └── dto/
│   │   │           ├── CreateProjectInput.java
│   │   │           └── CreateTaskInput.java
│   │   └── resources/
│   │       ├── application.yaml
│   │       ├── graphql/schema.graphqls
│   │       └── db/migration/V1__init_graphql_schema.sql
│   └── test/
│       └── java/com/example/graphql/
│           └── GraphQlIntegrationTest.java
├── Dockerfile
└── compose.yaml
```

---

## 2. Complete Production Implementation

### 2.1 `pom.xml`

```xml
<?xml version="1.0" encoding="UTF-8"?>
<project xmlns="http://maven.apache.org/POM/4.0.0"
         xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
         xsi:schemaLocation="http://maven.apache.org/POM/4.0.0 
         https://maven.apache.org/xsd/maven-4.0.0.xsd">
    <modelVersion>4.0.0</modelVersion>
    <parent>
        <groupId>org.springframework.boot</groupId>
        <artifactId>spring-boot-starter-parent</artifactId>
        <version>3.4.2</version>
        <relativePath/>
    </parent>
    <groupId>com.example</groupId>
    <artifactId>graphql-api</artifactId>
    <version>1.0.0</version>
    <name>graphql-api</name>
    <description>Enterprise GraphQL API with BatchMapping</description>

    <properties>
        <java.version>21</java.version>
        <testcontainers.version>1.20.4</testcontainers.version>
    </properties>

    <dependencies>
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-graphql</artifactId>
        </dependency>
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-web</artifactId>
        </dependency>
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-data-jpa</artifactId>
        </dependency>
        <dependency>
            <groupId>org.postgresql</groupId>
            <artifactId>postgresql</artifactId>
            <scope>runtime</scope>
        </dependency>
        <dependency>
            <groupId>org.flywaydb</groupId>
            <artifactId>flyway-core</artifactId>
        </dependency>
        <dependency>
            <groupId>org.flywaydb</groupId>
            <artifactId>flyway-database-postgresql</artifactId>
        </dependency>

        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-test</artifactId>
            <scope>test</scope>
        </dependency>
        <dependency>
            <groupId>org.springframework.graphql</groupId>
            <artifactId>spring-graphql-test</artifactId>
            <scope>test</scope>
        </dependency>
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-testcontainers</artifactId>
            <scope>test</scope>
        </dependency>
        <dependency>
            <groupId>org.testcontainers</groupId>
            <artifactId>postgresql</artifactId>
            <scope>test</scope>
        </dependency>
    </dependencies>

    <build>
        <plugins>
            <plugin>
                <groupId>org.springframework.boot</groupId>
                <artifactId>spring-boot-maven-plugin</artifactId>
            </plugin>
        </plugins>
    </build>
</project>
```

### 2.2 `application.yaml`

```yaml
server:
  port: 8080

spring:
  application:
    name: graphql-api
  graphql:
    graphiql:
      enabled: true
      path: /graphiql
  datasource:
    url: jdbc:postgresql://${DB_HOST:localhost}:${DB_PORT:5432}/${DB_NAME:graphql_db}
    username: ${DB_USER:postgres}
    password: ${DB_PASS:postgres}
  jpa:
    hibernate:
      ddl-auto: validate
    open-in-view: false
  flyway:
    enabled: true
```

### 2.3 GraphQL Schema: `src/main/resources/graphql/schema.graphqls`

```graphql
type Query {
    projects: [Project!]!
    project(id: ID!): Project
}

type Mutation {
    createProject(input: CreateProjectInput!): Project!
    createTask(input: CreateTaskInput!): Task!
}

input CreateProjectInput {
    title: String!
    description: String
}

input CreateTaskInput {
    projectId: ID!
    title: String!
}

type Project {
    id: ID!
    title: String!
    description: String
    tasks: [Task!]!
}

type Task {
    id: ID!
    projectId: ID!
    title: String!
    completed: Boolean!
}
```

### 2.4 Domain Entities & Repositories

```java
package com.example.graphql.domain;

import jakarta.persistence.*;
import java.util.UUID;

@Entity
@Table(name = "projects")
public class Project {

    @Id
    @GeneratedValue(strategy = GenerationType.UUID)
    private UUID id;

    @Column(nullable = false)
    private String title;

    private String description;

    public Project() {}

    public Project(String title, String description) {
        this.title = title;
        this.description = description;
    }

    public UUID getId() { return id; }
    public String getTitle() { return title; }
    public String getDescription() { return description; }
}
```

```java
package com.example.graphql.domain;

import jakarta.persistence.*;
import java.util.UUID;

@Entity
@Table(name = "tasks")
public class Task {

    @Id
    @GeneratedValue(strategy = GenerationType.UUID)
    private UUID id;

    @Column(name = "project_id", nullable = false)
    private UUID projectId;

    @Column(nullable = false)
    private String title;

    @Column(nullable = false)
    private boolean completed = false;

    public Task() {}

    public Task(UUID projectId, String title) {
        this.projectId = projectId;
        this.title = title;
    }

    public UUID getId() { return id; }
    public UUID getProjectId() { return projectId; }
    public String getTitle() { return title; }
    public boolean isCompleted() { return completed; }
}
```

```java
package com.example.graphql.repository;

import com.example.graphql.domain.Project;
import org.springframework.data.jpa.repository.JpaRepository;
import java.util.UUID;

public interface ProjectRepository extends JpaRepository<Project, UUID> {}
```

```java
package com.example.graphql.repository;

import com.example.graphql.domain.Task;
import org.springframework.data.jpa.repository.JpaRepository;
import java.util.List;
import java.util.UUID;

public interface TaskRepository extends JpaRepository<Task, UUID> {
    List<Task> findByProjectIdIn(List<UUID> projectIds);
}
```

### 2.5 GraphQL Controller with `@BatchMapping`

```java
package com.example.graphql.web.dto;

public record CreateProjectInput(String title, String description) {}
```

```java
package com.example.graphql.web.dto;

import java.util.UUID;

public record CreateTaskInput(UUID projectId, String title) {}
```

```java
package com.example.graphql.web;

import com.example.graphql.domain.Project;
import com.example.graphql.domain.Task;
import com.example.graphql.repository.ProjectRepository;
import com.example.graphql.repository.TaskRepository;
import com.example.graphql.web.dto.CreateProjectInput;
import com.example.graphql.web.dto.CreateTaskInput;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.graphql.data.method.annotation.Argument;
import org.springframework.graphql.data.method.annotation.BatchMapping;
import org.springframework.graphql.data.method.annotation.MutationMapping;
import org.springframework.graphql.data.method.annotation.QueryMapping;
import org.springframework.stereotype.Controller;

import java.util.*;
import java.util.stream.Collectors;

@Controller
public class ProjectGraphQlController {

    private static final Logger log = LoggerFactory.getLogger(ProjectGraphQlController.class);
    private final ProjectRepository projectRepository;
    private final TaskRepository taskRepository;

    public ProjectGraphQlController(ProjectRepository projectRepository, TaskRepository taskRepository) {
        this.projectRepository = projectRepository;
        this.taskRepository = taskRepository;
    }

    @QueryMapping
    public List<Project> projects() {
        return projectRepository.findAll();
    }

    @QueryMapping
    public Project project(@Argument UUID id) {
        return projectRepository.findById(id).orElse(null);
    }

    @MutationMapping
    public Project createProject(@Argument CreateProjectInput input) {
        Project project = new Project(input.title(), input.description());
        return projectRepository.save(project);
    }

    @MutationMapping
    public Task createTask(@Argument CreateTaskInput input) {
        Task task = new Task(input.projectId(), input.title());
        return taskRepository.save(task);
    }

    /**
     * Solves N+1 query problem! Instead of fetching tasks per project individually,
     * Spring for GraphQL batches all project IDs into a single IN (...) query.
     */
    @BatchMapping
    public Map<Project, List<Task>> tasks(List<Project> projects) {
        log.info("Batch mapping tasks for {} projects in a single query!", projects.size());
        List<UUID> projectIds = projects.stream().map(Project::getId).toList();

        List<Task> allTasks = taskRepository.findByProjectIdIn(projectIds);
        Map<UUID, List<Task>> tasksByProjectId = allTasks.stream()
                .collect(Collectors.groupingBy(Task::getProjectId));

        Map<Project, List<Task>> result = new HashMap<>();
        for (Project project : projects) {
            result.put(project, tasksByProjectId.getOrDefault(project.getId(), Collections.emptyList()));
        }
        return result;
    }
}
```

---

## 3. Automated Testing Suite

```java
package com.example.graphql;

import com.example.graphql.domain.Project;
import com.example.graphql.repository.ProjectRepository;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.autoconfigure.graphql.tester.AutoConfigureGraphQlTester;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.boot.testcontainers.service.connection.ServiceConnection;
import org.springframework.graphql.test.tester.GraphQlTester;
import org.testcontainers.containers.PostgreSQLContainer;
import org.testcontainers.junit.jupiter.Container;
import org.testcontainers.junit.jupiter.Testcontainers;

@SpringBootTest
@AutoConfigureGraphQlTester
@Testcontainers
public class GraphQlIntegrationTest {

    @Container
    @ServiceConnection
    static PostgreSQLContainer<?> postgres = new PostgreSQLContainer<>("postgres:16-alpine");

    @Autowired
    private GraphQlTester graphQlTester;

    @Autowired
    private ProjectRepository projectRepository;

    @BeforeEach
    void setup() {
        projectRepository.deleteAll();
    }

    @Test
    void shouldCreateAndQueryProject() {
        // Language=GraphQL
        String document = """
            mutation {
                createProject(input: { title: "Spring Cloud Migration", description: "Migrate legacy stack" }) {
                    id
                    title
                }
            }
        """;

        graphQlTester.document(document)
                .execute()
                .path("createProject.title")
                .entity(String.class)
                .isEqualTo("Spring Cloud Migration");
    }
}
```

---

## 4. Production Infrastructure & Deployment

### 4.1 Multi-Stage `Dockerfile`

```dockerfile
FROM maven:3.9.6-eclipse-temurin-21-alpine AS builder
WORKDIR /app
COPY pom.xml .
RUN mvn dependency:go-offline -B
COPY src ./src
RUN mvn clean package -DskipTests

FROM eclipse-temurin:21-jre-alpine
RUN addgroup -S appgroup && adduser -S appuser -G appgroup
WORKDIR /app
COPY --from=builder /app/target/*.jar app.jar
USER appuser
EXPOSE 8080
ENTRYPOINT ["java", "-XX:+UseZGC", "-XX:+ZGenerational", "-jar", "app.jar"]
```

### 4.2 `compose.yaml`

```yaml
services:
  graphql-db:
    image: postgres:16-alpine
    container_name: graphql-db
    environment:
      POSTGRES_DB: graphql_db
      POSTGRES_USER: postgres
      POSTGRES_PASSWORD: graphqlpassword
    ports:
      - "5432:5432"
    volumes:
      - graphql_postgres_data:/var/lib/postgresql/data
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U postgres"]
      interval: 5s
      timeout: 5s
      retries: 5

  graphql-api:
    build: .
    container_name: graphql-api
    ports:
      - "8080:8080"
    environment:
      DB_HOST: graphql-db
      DB_PORT: 5432
      DB_NAME: graphql_db
      DB_USER: postgres
      DB_PASS: graphqlpassword
    depends_on:
      graphql-db:
        condition: service_healthy

volumes:
  graphql_postgres_data:
```

---

## 5. Operational Runbook & Verification

### 5.1 Verification Commands

```bash
# 1. Start containers
docker compose up -d

# 2. Open interactive GraphiQL IDE in browser:
# http://localhost:8080/graphiql

# 3. Create a project via cURL
curl -i -X POST http://localhost:8080/graphql \
  -H "Content-Type: application/json" \
  -d '{"query": "mutation { createProject(input: { title: \"Project Apollo\" }) { id title } }"}'

# 4. Query all projects and their nested tasks
curl -i -X POST http://localhost:8080/graphql \
  -H "Content-Type: application/json" \
  -d '{"query": "query { projects { id title tasks { id title completed } } }"}'
```
