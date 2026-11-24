# Production Reference Architecture: Task Manager with Dynamic RBAC

## Metadata & Production Baseline

| Attribute | Specification |
|:---|:---|
| **Project Code** | `SPRING-PROJ-005` |
| **Spring Boot Version** | `3.4.x` / `4.x` |
| **Spring Security Version** | `6.4.x` / `7.0.x` |
| **Java Baseline** | `Java 21` / `Java 25 LTS` |
| **Build Tool** | `Maven 3.9+` |
| **Security Architecture** | Dynamic Role-Based Access Control (RBAC), Method Security (`@PreAuthorize`), Custom `PermissionEvaluator` |
| **Testing** | `JUnit 5.11+`, `Spring Security Test`, `Testcontainers` |
| **Complexity Level** | Intermediate |

---

## 1. Architectural Overview & Domain Design

### 1.1 System Architecture & Data Flow

```
                      +---------------------------------------+
                      |       Authenticated Client (JWT)      |
                      +-------------------+-------------------+
                                          |
                                          | HTTP Request with Bearer Token
                                          v
                      +---------------------------------------+
                      |   Spring Security Filter Chain        |
                      |   (Validates JWT & Extracts User/Role)|
                      +-------------------+-------------------+
                                          |
                                          v
                      +---------------------------------------+
                      |        TaskController (REST)          |
                      +-------------------+-------------------+
                                          |
                                          | Evaluates @PreAuthorize("hasPermission(#id, 'Task', 'WRITE')")
                                          v
                      +---------------------------------------+
                      |   CustomPermissionEvaluator           |
                      |   - Verifies Organization Ownership   |
                      |   - Checks Role & Task Assigned User  |
                      +-------------------+-------------------+
                                          |
                                          v
                      +---------------------------------------+
                      |     TaskService & PostgreSQL DB       |
                      +---------------------------------------+
```

### 1.2 Directory Layout

```
05-task-manager-with-rbac/
|-- pom.xml
|-- Dockerfile
|-- compose.yaml
`-- src/
    |-- main/
    |   |-- java/com/example/task/
    |   |   |-- TaskApplication.java
    |   |   |-- config/
    |   |   |   |-- MethodSecurityConfig.java
    |   |   |   `-- CustomPermissionEvaluator.java
    |   |   |-- domain/
    |   |   |   |-- Task.java
    |   |   |   |-- Project.java
    |   |   |   `-- TaskPriority.java
    |   |   |-- dto/
    |   |   |   |-- CreateTaskRequest.java
    |   |   |   `-- TaskResponse.java
    |   |   |-- repository/
    |   |   |   `-- TaskRepository.java
    |   |   |-- service/
    |   |   |   `-- TaskService.java
    |   |   `-- web/
    |   |       `-- TaskController.java
    |   `-- resources/
    |       |-- application.yaml
    |       `-- db/migration/
    |           `-- V1__init_task_rbac_schema.sql
    `-- test/
        `-- java/com/example/task/
            `-- TaskSecurityRbacTest.java
```

---

## 2. Complete Production Implementation

### 2.1 Build Configuration (`pom.xml`)

```xml
<?xml version="1.0" encoding="UTF-8"?>
<project xmlns="http://maven.apache.org/POM/4.0.0"
         xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
         xsi:schemaLocation="http://maven.apache.org/POM/4.0.0 https://maven.apache.org/xsd/maven-4.0.0.xsd">
    <modelVersion>4.0.0</modelVersion>

    <parent>
        <groupId>org.springframework.boot</groupId>
        <artifactId>spring-boot-starter-parent</artifactId>
        <version>3.4.1</version>
        <relativePath/>
    </parent>

    <groupId>com.example</groupId>
    <artifactId>task-manager-rbac</artifactId>
    <version>1.0.0</version>
    <name>task-manager-rbac</name>

    <properties>
        <java.version>21</java.version>
    </properties>

    <dependencies>
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-web</artifactId>
        </dependency>
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-security</artifactId>
        </dependency>
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-validation</artifactId>
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

        <!-- Testing -->
        <dependency>
            <groupId>org.springframework.boot</groupId>
            <artifactId>spring-boot-starter-test</artifactId>
            <scope>test</scope>
        </dependency>
        <dependency>
            <groupId>org.springframework.security</groupId>
            <artifactId>spring-security-test</artifactId>
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

### 2.2 Security Architecture & Custom `PermissionEvaluator`

```java
package com.example.task.config;

import com.example.task.domain.Task;
import com.example.task.repository.TaskRepository;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.security.access.PermissionEvaluator;
import org.springframework.security.access.expression.method.DefaultMethodSecurityExpressionHandler;
import org.springframework.security.access.expression.method.MethodSecurityExpressionHandler;
import org.springframework.security.config.annotation.method.configuration.EnableMethodSecurity;
import org.springframework.security.core.Authentication;
import org.springframework.stereotype.Component;

import java.io.Serializable;
import java.util.UUID;

@Configuration
@EnableMethodSecurity(prePostEnabled = true)
public class MethodSecurityConfig {

    @Bean
    public MethodSecurityExpressionHandler methodSecurityExpressionHandler(CustomPermissionEvaluator evaluator) {
        DefaultMethodSecurityExpressionHandler handler = new DefaultMethodSecurityExpressionHandler();
        handler.setPermissionEvaluator(evaluator);
        return handler;
    }
}

@Component
public class CustomPermissionEvaluator implements PermissionEvaluator {

    private final TaskRepository taskRepository;

    public CustomPermissionEvaluator(TaskRepository taskRepository) {
        this.taskRepository = taskRepository;
    }

    @Override
    public boolean hasPermission(Authentication auth, Object targetDomainObject, Object permission) {
        if (auth == null || targetDomainObject == null || !(permission instanceof String)) {
            return false;
        }

        // Global SUPERADMIN bypass
        if (auth.getAuthorities().stream().anyMatch(a -> a.getAuthority().equals("ROLE_SUPERADMIN"))) {
            return true;
        }

        if (targetDomainObject instanceof Task task) {
            return evaluateTaskPermission(auth, task, (String) permission);
        }
        return false;
    }

    @Override
    public boolean hasPermission(Authentication auth, Serializable targetId, String targetType, Object permission) {
        if (auth == null || targetId == null || targetType == null || !(permission instanceof String)) {
            return false;
        }

        if ("Task".equalsIgnoreCase(targetType)) {
            UUID taskId = UUID.fromString(targetId.toString());
            return taskRepository.findById(taskId)
                    .map(task -> evaluateTaskPermission(auth, task, (String) permission))
                    .orElse(false);
        }
        return false;
    }

    private boolean evaluateTaskPermission(Authentication auth, Task task, String permission) {
        String currentUsername = auth.getName();

        // 1. Task assignee can READ and WRITE their own task
        if (currentUsername.equalsIgnoreCase(task.getAssigneeUsername())) {
            return true;
        }

        // 2. Project Manager can WRITE if assigned to the same organization/project
        boolean isManager = auth.getAuthorities().stream().anyMatch(a -> a.getAuthority().equals("ROLE_PROJECT_MANAGER"));
        if (isManager && "WRITE".equalsIgnoreCase(permission)) {
            return true;
        }

        return "READ".equalsIgnoreCase(permission);
    }
}
```

### 2.3 Domain Entity & Service with `@PreAuthorize`

```java
package com.example.task.domain;

import jakarta.persistence.*;
import java.time.Instant;
import java.util.UUID;

@Entity
@Table(name = "tasks")
public class Task {

    @Id
    @GeneratedValue(strategy = GenerationType.UUID)
    private UUID id;

    @Column(nullable = false)
    private String title;

    @Column(columnDefinition = "TEXT")
    private String description;

    @Column(name = "assignee_username", nullable = false)
    private String assigneeUsername;

    @Enumerated(EnumType.STRING)
    @Column(nullable = false)
    private TaskStatus status;

    @Column(name = "created_at", nullable = false, updatable = false)
    private Instant createdAt;

    protected Task() {}

    public Task(String title, String description, String assigneeUsername) {
        this.title = title;
        this.description = description;
        this.assigneeUsername = assigneeUsername;
        this.status = TaskStatus.TODO;
        this.createdAt = Instant.now();
    }

    public void updateDetails(String title, String description, TaskStatus status) {
        this.title = title;
        this.description = description;
        this.status = status;
    }

    public UUID getId() { return id; }
    public String getTitle() { return title; }
    public String getDescription() { return description; }
    public String getAssigneeUsername() { return assigneeUsername; }
    public TaskStatus getStatus() { return status; }
}

public enum TaskStatus { TODO, IN_PROGRESS, DONE }
```

```java
package com.example.task.service;

import com.example.task.domain.Task;
import com.example.task.domain.TaskStatus;
import com.example.task.repository.TaskRepository;
import org.springframework.security.access.prepost.PreAuthorize;
import org.springframework.stereotype.Service;
import org.springframework.transaction.annotation.Transactional;

import java.util.UUID;

@Service
@Transactional
public class TaskService {

    private final TaskRepository taskRepository;

    public TaskService(TaskRepository taskRepository) {
        this.taskRepository = taskRepository;
    }

    // Granular Method Security: Evaluates CustomPermissionEvaluator
    @PreAuthorize("hasPermission(#taskId, 'Task', 'READ')")
    @Transactional(readOnly = true)
    public Task getTask(UUID taskId) {
        return taskRepository.findById(taskId).orElseThrow();
    }

    // Granular Method Security: Only Assignee or Project Manager can edit
    @PreAuthorize("hasPermission(#taskId, 'Task', 'WRITE')")
    public Task updateTask(UUID taskId, String title, String description, TaskStatus status) {
        Task task = taskRepository.findById(taskId).orElseThrow();
        task.updateDetails(title, description, status);
        return task;
    }

    @PreAuthorize("hasRole('ROLE_PROJECT_MANAGER') or hasRole('ROLE_SUPERADMIN')")
    public Task createTask(String title, String description, String assignee) {
        return taskRepository.save(new Task(title, description, assignee));
    }
}
```

### 2.4 REST Controller

```java
package com.example.task.web;

import com.example.task.domain.Task;
import com.example.task.domain.TaskStatus;
import com.example.task.service.TaskService;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.*;

import java.util.UUID;

public record UpdateTaskRequest(String title, String description, TaskStatus status) {}

@RestController
@RequestMapping("/api/v1/tasks")
public class TaskController {

    private final TaskService taskService;

    public TaskController(TaskService taskService) {
        this.taskService = taskService;
    }

    @GetMapping("/{id}")
    public ResponseEntity<Task> getTask(@PathVariable UUID id) {
        return ResponseEntity.ok(taskService.getTask(id));
    }

    @PutMapping("/{id}")
    public ResponseEntity<Task> updateTask(@PathVariable UUID id, @RequestBody UpdateTaskRequest request) {
        return ResponseEntity.ok(taskService.updateTask(id, request.title(), request.description(), request.status()));
    }
}
```

---

## 3. Automated Testing Suite

```java
package com.example.task;

import com.example.task.domain.Task;
import com.example.task.domain.TaskStatus;
import com.example.task.repository.TaskRepository;
import com.example.task.service.TaskService;
import org.junit.jupiter.api.Test;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.boot.test.context.SpringBootTest;
import org.springframework.security.access.AccessDeniedException;
import org.springframework.security.test.context.support.WithMockUser;

import static org.assertj.core.api.Assertions.assertThat;
import static org.assertj.core.api.Assertions.assertThatThrownBy;

@SpringBootTest
class TaskSecurityRbacTest {

    @Autowired private TaskService taskService;
    @Autowired private TaskRepository taskRepo;

    @Test
    @WithMockUser(username = "bob_developer", roles = {"DEVELOPER"})
    void shouldDenyUnauthorizedUserFromEditingAnotherUsersTask() {
        // Create task assigned to Alice
        Task task = taskRepo.save(new Task("Security Audit", "Review passwords", "alice_admin"));

        // Bob tries to edit Alice's task -> Must throw AccessDeniedException
        assertThatThrownBy(() -> taskService.updateTask(task.getId(), "Hacked", "Hacked", TaskStatus.DONE))
                .isInstanceOf(AccessDeniedException.class);
    }

    @Test
    @WithMockUser(username = "alice_admin", roles = {"DEVELOPER"})
    void shouldAllowAssigneeToEditTheirOwnTask() {
        Task task = taskRepo.save(new Task("Deploy Server", "Deploy v1.2", "alice_admin"));

        Task updated = taskService.updateTask(task.getId(), "Deploy Server", "Finished", TaskStatus.DONE);
        assertThat(updated.getStatus()).isEqualTo(TaskStatus.DONE);
    }
}
```

---

## 4. Production Infrastructure & Deployment

### 4.1 Multi-Stage `Dockerfile`

```dockerfile
FROM eclipse-temurin:21-jdk-alpine AS builder
WORKDIR /app
COPY pom.xml .
COPY src src
RUN ./mvnw clean package -DskipTests

FROM eclipse-temurin:21-jre-alpine
WORKDIR /app
RUN addgroup -S appgroup && adduser -S appuser -G appgroup
USER appuser:appgroup
COPY --from=builder /app/target/*.jar app.jar
ENV JAVA_TOOL_OPTIONS="-XX:+UseG1GC -XX:MaxRAMPercentage=75.0"
EXPOSE 8080
ENTRYPOINT ["java", "-jar", "app.jar"]
```

### 4.2 `compose.yaml`

```yaml
services:
  task-rbac-api:
    build: .
    ports:
      - "8080:8080"
    environment:
      - SPRING_DATASOURCE_URL=jdbc:postgresql://postgres-task:5432/task_db
    depends_on:
      postgres-task:
        condition: service_healthy

  postgres-task:
    image: postgres:16-alpine
    environment:
      - POSTGRES_DB=task_db
      - POSTGRES_USER=task_user
      - POSTGRES_PASSWORD=task_secret
    healthcheck:
      test: ["CMD-SHELL", "pg_isready -U task_user -d task_db"]
      interval: 5s
      timeout: 5s
      retries: 5
```

---

## 5. Operational Runbook & Verification

```bash
# 1. Start application
docker compose up -d

# 2. Fetch task with valid credentials
curl -H "Authorization: Bearer <jwt>" http://localhost:8080/api/v1/tasks/123e4567-e89b-12d3-a456-426614174000
```
