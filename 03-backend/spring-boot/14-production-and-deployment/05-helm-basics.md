# Helm Charts for Spring Boot Microservices

| Metadata | Value |
|---|---|
| **Spring Boot Version** | 3.4.x |
| **Spring Framework Version** | 6.2.x |
| **Java Version** | 21 LTS |
| **Module** | 14-production-and-deployment |
| **Status** | Complete |

---

## Table of Contents
- [1. What & Why](#1-what--why)
- [2. Core Concepts](#2-core-concepts)
- [3. How It Works Internally](#3-how-it-works-internally)
- [4. Syntax & Template Architecture Reference](#4-syntax--template-architecture-reference)
- [5. Practical Examples](#5-practical-examples)
- [6. Best Practices & Production Guidelines](#6-best-practices--production-guidelines)
- [7. Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
- [8. Performance & Security Notes](#8-performance--security-notes)
- [9. Comparison / When to Use What](#9-comparison--when-to-use-what)
- [10. Interview Questions & Answers](#10-interview-questions--answers)
- [11. Practice Exercises](#11-practice-exercises)
- [12. Summary / Cheat Sheet](#12-summary--cheat-sheet)
- [13. Further Reading](#13-further-reading)

---

## 1. What & Why

### Problem Statement
Managing raw Kubernetes YAML manifests across multiple microservices and environments (Development, Staging, Production) produces massive configuration duplication:
- A change to a common probe path, logging pattern, or security context requires copy-pasting edits across dozens of separate YAML files.
- Environment differences (e.g. 1 replica in Dev vs. 10 replicas in Prod) result in fragile, fragmented manifest copies.
- Rollbacks and release tracking require manual scripting without unified versioning.

### Historical Evolution
Originally created by Deis and incubated by the CNCF, **Helm** has become the de-facto package manager for Kubernetes. Helm allows developers to package, parameterize, and version Kubernetes resources into **Charts**.

Rather than maintaining hardcoded YAML files, Spring Boot applications define dynamic Go templates (`templates/deployment.yaml`) parameterized by environment values files (`values-dev.yaml`, `values-prod.yaml`).

### Production Relevance
Packaging Spring Boot microservices with Helm enables:
- **Parameterized Deployments**: Single template suite parameterized across all cloud environments.
- **Atomic Upgrades & Rollbacks**: Automated releases with instant rollback (`helm rollback my-service 2`) if health checks fail.
- **Standardized Enterprise Base Charts**: Organizations can maintain a shared "library chart" enforcing company-wide security standards, probe paths, and logging configurations.

---

## 2. Core Concepts

### Helm Architecture Components
1. **Chart**: The package containing all resource definitions, metadata, and default values.
2. **`Chart.yaml`**: Metadata file declaring the chart name, description, API version, and semantic version (`version` and `appVersion`).
3. **`values.yaml`**: Default configuration values that can be overridden per environment (`values-prod.yaml`).
4. **`templates/`**: Directory of Go-templated Kubernetes manifests rendered by the Helm engine.
5. **`_helpers.tpl`**: Reusable template partials and naming conventions (e.g. `order-service.fullname`).
6. **Release**: A running instance of a chart deployed into a Kubernetes cluster (`helm install my-release ./order-service`).
7. **Helm Hooks**: Annotations that trigger specific jobs before or after an installation or upgrade (e.g. running a Flyway database migration job before rolling the deployment).

---

## 3. How It Works Internally

### Helm Rendering and Release Management Pipeline

```
DEVELOPER / CI RUNNER
  |
  |  helm upgrade --install order-service ./chart -f values-prod.yaml
  v
+------------------------------------------------------------------------------------+
| 1. TEMPLATE ENGINE (Go Templates & Sprig Functions)                                |
|    - Merges default values.yaml with override values-prod.yaml                     |
|    - Evaluates templates/ (resolves if/else, range, include, nindent)              |
|    - Produces valid, fully resolved Kubernetes YAML stream                         |
+------------------------------------------------------------------------------------+
  |
  v
+------------------------------------------------------------------------------------+
| 2. PRE-INSTALL / PRE-UPGRADE HOOKS EXECUTION                                       |
|    - Detects annotations: "helm.sh/hook": pre-upgrade                              |
|    - Executes Flyway / Liquibase DB migration Kubernetes Job                       |
|    - Waits for migration job completion                                           |
+------------------------------------------------------------------------------------+
  |
  v
+------------------------------------------------------------------------------------+
| 3. KUBERNETES API SERVER SUBMISSION & RELEASE VERSIONING                           |
|    - Applies manifests to Kubernetes API (kubectl apply equivalent)                |
|    - Creates a versioned Secret/ConfigMap storing the release metadata             |
|    - Enables instant rollback via: helm rollback order-service <revision>          |
+------------------------------------------------------------------------------------+
```

---

## 4. Syntax & Template Architecture Reference

### Standard Chart Directory Layout
```text
order-service/
  ├── Chart.yaml                  # Chart metadata and versions
  ├── values.yaml                 # Default values (Local/Dev)
  ├── values-prod.yaml            # Production override values
  ├── templates/
  │   ├── _helpers.tpl            # Template partials and naming helpers
  │   ├── deployment.yaml         # Spring Boot Deployment template
  │   ├── service.yaml            # ClusterIP Service template
  │   ├── ingress.yaml            # Ingress route template
  │   ├── hpa.yaml                # Autoscaler template
  │   ├── pdb.yaml                # PodDisruptionBudget template
  │   └── migration-job.yaml      # Pre-upgrade DB migration hook
  └── .helmignore                 # Patterns to exclude from packaging
```

### 1. `Chart.yaml`
```yaml
apiVersion: v2
name: order-service
description: A production-ready Helm chart for Order Service Spring Boot Application
type: application
version: 1.2.0      # Chart semantic version
appVersion: "2.4.1" # Spring Boot application version
```

### 2. Default `values.yaml`
```yaml
replicaCount: 2

image:
  repository: registry.internal/apps/order-service
  pullPolicy: IfNotPresent
  tag: "" # Defaults to .Chart.AppVersion if empty

service:
  type: ClusterIP
  port: 80
  targetPort: 8080
  managementPort: 9090

ingress:
  enabled: true
  className: nginx
  annotations:
    cert-manager.io/cluster-issuer: letsencrypt-prod
  hosts:
    - host: api.company.com
      paths:
        - path: /api/orders
          pathType: Prefix
  tls:
    - secretName: order-api-tls
      hosts:
        - api.company.com

resources:
  requests:
    cpu: 500m
    memory: 1024Mi
  limits:
    cpu: 1000m
    memory: 1024Mi

probes:
  startup:
    failureThreshold: 30
    periodSeconds: 2
  liveness:
    periodSeconds: 10
    timeoutSeconds: 3
  readiness:
    periodSeconds: 5
    timeoutSeconds: 2

autoscaling:
  enabled: false
  minReplicas: 2
  maxReplicas: 10
  targetCPUUtilizationPercentage: 75
```

---

## 5. Practical Examples

### Example 1: Parameterized `templates/deployment.yaml`
```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: {{ include "order-service.fullname" . }}
  labels:
    {{- include "order-service.labels" . | nindent 4 }}
spec:
  {{- if not .Values.autoscaling.enabled }}
  replicas: {{ .Values.replicaCount }}
  {{- end }}
  strategy:
    type: RollingUpdate
    rollingUpdate:
      maxSurge: 25%
      maxUnavailable: 0
  selector:
    matchLabels:
      {{- include "order-service.selectorLabels" . | nindent 6 }}
  template:
    metadata:
      labels:
        {{- include "order-service.selectorLabels" . | nindent 8 }}
    spec:
      terminationGracePeriodSeconds: 60
      containers:
        - name: {{ .Chart.Name }}
          image: "{{ .Values.image.repository }}:{{ .Values.image.tag | default .Chart.AppVersion }}"
          imagePullPolicy: {{ .Values.image.pullPolicy }}
          ports:
            - name: http
              containerPort: {{ .Values.service.targetPort }}
            - name: management
              containerPort: {{ .Values.service.managementPort }}
          startupProbe:
            httpGet:
              path: /actuator/health/liveness
              port: management
            failureThreshold: {{ .Values.probes.startup.failureThreshold }}
            periodSeconds: {{ .Values.probes.startup.periodSeconds }}
          livenessProbe:
            httpGet:
              path: /actuator/health/liveness
              port: management
            periodSeconds: {{ .Values.probes.liveness.periodSeconds }}
            timeoutSeconds: {{ .Values.probes.liveness.timeoutSeconds }}
          readinessProbe:
            httpGet:
              path: /actuator/health/readiness
              port: management
            periodSeconds: {{ .Values.probes.readiness.periodSeconds }}
            timeoutSeconds: {{ .Values.probes.readiness.timeoutSeconds }}
          resources:
            {{- toYaml .Values.resources | nindent 12 }}
          lifecycle:
            preStop:
              exec:
                command: ["sh", "-c", "sleep 15"]
```

### Example 2: Reusable Helpers in `templates/_helpers.tpl`
```yaml
{{/*
Expand the name of the chart.
*/}}
{{- define "order-service.name" -}}
{{- default .Chart.Name .Values.nameOverride | trunc 63 | trimSuffix "-" }}
{{- end }}

{{/*
Create a default fully qualified app name.
*/}}
{{- define "order-service.fullname" -}}
{{- if .Values.fullnameOverride }}
{{- .Values.fullnameOverride | trunc 63 | trimSuffix "-" }}
{{- else }}
{{- $name := default .Chart.Name .Values.nameOverride }}
{{- printf "%s-%s" .Release.Name $name | trunc 63 | trimSuffix "-" }}
{{- end }}
{{- end }}

{{/*
Common labels attached to all resources
*/}}
{{- define "order-service.labels" -}}
helm.sh/chart: {{ include "order-service.chart" . }}
{{ include "order-service.selectorLabels" . }}
app.kubernetes.io/version: {{ .Values.image.tag | default .Chart.AppVersion | quote }}
app.kubernetes.io/managed-by: {{ .Release.Service }}
{{- end }}

{{/*
Selector labels for deployment and service
*/}}
{{- define "order-service.selectorLabels" -}}
app.kubernetes.io/name: {{ include "order-service.name" . }}
app.kubernetes.io/instance: {{ .Release.Name }}
{{- end }}
```

### Example 3: Database Migration Pre-Upgrade Hook (`templates/migration-job.yaml`)
Executes Flyway database migrations before updating pod replicas:

```yaml
apiVersion: batch/v1
kind: Job
metadata:
  name: "{{ include "order-service.fullname" . }}-migration-{{ .Release.Revision }}"
  annotations:
    # Triggers before the deployment upgrade begins
    "helm.sh/hook": pre-install,pre-upgrade
    "helm.sh/hook-weight": "1"
    "helm.sh/hook-delete-policy": before-hook-creation,hook-succeeded
spec:
  template:
    spec:
      restartPolicy: Never
      containers:
        - name: migration
          image: "{{ .Values.image.repository }}:{{ .Values.image.tag | default .Chart.AppVersion }}"
          command: ["java", "-cp", "app.jar", "org.springframework.boot.loader.JarLauncher", "--spring.profiles.active=migration"]
          env:
            - name: DATABASE_PASSWORD
              valueFrom:
                secretKeyRef:
                  name: db-credentials
                  key: password
```

### Example 4: Anti-Pattern vs. Production Fix (Hardcoded Image Tags in Templates)
```yaml
# ANTI-PATTERN: Hardcoding tags or repo inside the template file!
image: "mycompany/orders:v1.0" # Defeats CI/CD parameterization completely!

# PRODUCTION FIX: Fallback chaining using default and AppVersion
image: "{{ .Values.image.repository }}:{{ .Values.image.tag | default .Chart.AppVersion }}"
```

### Example 5: Linting and Testing Templates Locally
```bash
# 1. Validate syntax and values validity
helm lint ./order-service

# 2. Render templates locally to inspect generated YAML without deploying
helm template my-release ./order-service -f values-prod.yaml > rendered-output.yaml

# 3. Dry-run installation against a live Kubernetes cluster
helm install my-release ./order-service -f values-prod.yaml --dry-run
```

---

## 6. Best Practices & Production Guidelines

1. **Always Use `nindent` Instead of `indent`**: `nindent` automatically prepends a newline before indenting, preventing formatting syntax errors when embedding multi-line blocks (e.g. `{{- toYaml .Values.resources | nindent 12 }}`).
2. **Never Store Cleartext Passwords in `values.yaml`**: Store secrets in external secret stores (Vault, AWS Secrets Manager) and inject them via Kubernetes Secret references or External Secrets Operator.
3. **Always Run `helm lint` in CI Pipelines**: Ensure charts are syntactically validated before merging code.
4. **Parameterize Probes and Resources**: Default values must cater to development, while `values-prod.yaml` should enforce production sizing (e.g. Guaranteed QoS with matching requests and limits).
5. **Use Semantic Versioning for Charts**: Bump `version` whenever templates or values change; bump `appVersion` when the underlying Spring Boot binary changes.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Practical Remedy |
|---|---|---|
| Whitespace syntax errors in YAML | Missing `-` in `{{-` or incorrect indentation corrupts generated YAML files. | Use `helm template` to inspect whitespace rendering; use `nindent`. |
| Committing database passwords in `values.yaml` | Plaintext credentials leak into Git history. | Reference Kubernetes Secret keys: `valueFrom.secretKeyRef`. |
| Omitting `nameOverride` / `fullnameOverride` | Helm chart fails when multiple releases are deployed into the same namespace due to name collisions. | Standardize on `_helpers.tpl` naming macros. |
| Forgetting `hook-delete-policy` on migration jobs | Previous failed or successful migration Jobs block subsequent Helm upgrades with naming conflicts. | Add `"helm.sh/hook-delete-policy": before-hook-creation`. |
| Inconsistent label selectors | Changing `selectorLabels` between releases causes deployment updates to fail with immutable field errors. | Keep `selectorLabels` minimal, static, and stable. |
| Missing `.helmignore` | Helm packages `.git` directories and local build logs into chart archives, inflating package size. | Include a comprehensive `.helmignore` file. |

---

## 8. Performance & Security Notes

### Helm Release Storage Overhead
- By default, Helm stores release history in Kubernetes Secrets within the target namespace.
- In active CI/CD pipelines deploying 50 times a day, secrets can accumulate rapidly.
- Limit release history retention: `helm upgrade --history-max 10`.

### Security Hardening of Helm Charts
- Verify chart signatures using Helm provenance files (`helm verify`).
- Scan Helm charts for misconfigurations using static linters like **Checkov** or **Kube-linter** to ensure `securityContext` best practices are satisfied.

---

## 9. Comparison / When to Use What

| Feature | Helm | Kustomize | Raw Manifests |
|---|---|---|---|
| **Parameterization Model** | Go Templates + `values.yaml` | Patch Overlays (Overlay YAMLs) | None (Hardcoded) |
| **Package Management** | **Yes (Versioned artifacts)** | No | No |
| **Lifecycle Hooks (Migrations)**| **Yes (`helm.sh/hook`)** | No | No |
| **Rollback Tracking** | **Yes (`helm rollback`)** | Via Git / ArgoCD only | Manual |
| **Learning Curve** | Medium (Template syntax) | Low | Low |

---

## 10. Interview Questions & Answers

### Q1: What is the purpose of `_helpers.tpl` in a Helm chart?
<details>
<summary>Click to view answer</summary>

`_helpers.tpl` is a dedicated file where shared template definitions (partials/macros) are declared using the `{{- define "macro.name" -}}` syntax. These helpers encapsulate common logic: generating fully qualified resource names, constructing standard Kubernetes metadata labels (`app.kubernetes.io/name`, `managed-by`), and enforcing length limits (truncating names to 63 characters to satisfy DNS label requirements).
</details>

### Q2: What is the difference between `version` and `appVersion` in `Chart.yaml`?
<details>
<summary>Click to view answer</summary>

- `version`: The semantic version (SemVer 2) of the **Helm chart itself** (e.g. `1.2.0`). This version increments when templates, helpers, or default values change.
- `appVersion`: The version of the **underlying application binary** being deployed (e.g. Spring Boot version `2.4.1` or Git SHA). Incrementing `appVersion` does not necessarily alter chart structure.
</details>

### Q3: How do Helm lifecycle hooks work and what is a common use case for Spring Boot?
<details>
<summary>Click to view answer</summary>

Helm hooks allow operations to be executed at specific points during a release lifecycle. They are declared via annotations:
`"helm.sh/hook": pre-install,pre-upgrade`.  
A common Spring Boot use case is running a standalone **Flyway database migration Job** before rolling out the new application deployment. Helm runs the migration job, waits for it to succeed, and only then proceeds with updating the application pod deployment.
</details>

### Q4: What does the `nindent` function do and why is it preferred over `indent`?
<details>
<summary>Click to view answer</summary>

`indent` prepends spaces to each line of a multi-line string. However, if invoked on an existing line, the first line does not receive a preceding newline, often causing YAML syntax errors. `nindent` (new-line indent) prepends a newline character *before* applying the specified indentation, ensuring that injected multi-line blocks (like `resources:` or `env:`) align with YAML structure.
</details>

### Q5: How do you perform a rollback with Helm if a Spring Boot deployment fails?
<details>
<summary>Click to view answer</summary>

View previous releases:
```bash
helm history order-service
```
Roll back to a specific revision (e.g. revision 2):
```bash
helm rollback order-service 2
```
Helm automatically reapplies the exact Kubernetes manifests from revision 2, triggering a rolling update back to the previous stable pod deployment.
</details>

### Q6: What is a Library Chart in Helm?
<details>
<summary>Click to view answer</summary>

A Library Chart (`type: library` in `Chart.yaml`) is a chart that provides common templates and helpers to other charts, without deploying any resources on its own. Organizations maintain a central library chart containing standard Spring Boot deployment templates, probes, and security contexts. Microservice application charts simply declare the library chart as a dependency and supply their custom `values.yaml`.
</details>

### Q7: How does Helm merge `values.yaml` files when multiple files are passed?
<details>
<summary>Click to view answer</summary>

Values are merged in the order they are passed on the command line:
`helm install ... -f values.yaml -f values-prod.yaml --set replicaCount=5`.  
Files evaluated later override values from earlier files. Explicit `--set` flags have the highest precedence, overriding all file-based values.
</details>

### Q8: What does `helm template` do?
<details>
<summary>Click to view answer</summary>

`helm template` renders all chart templates locally using the provided values and prints the resulting raw Kubernetes YAML manifests directly to standard output. It does not contact the Kubernetes cluster, making it the primary tool for testing template syntax and debugging rendering logic in CI/CD pipelines.
</details>

---

## 11. Practice Exercises

### Exercise 1: Render Helm Chart Locally with Custom Values (Easy)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Write the Helm command to render a local chart at `./order-chart` using override values file `values-prod.yaml` without contacting a cluster.

**Solution**:
```bash
helm template test-release ./order-chart -f values-prod.yaml
```
</details>

### Exercise 2: Parameterize Service Port in Helm Template (Medium)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Write a `templates/service.yaml` template that parameterizes the service port and targetPort, defaulting the targetPort to 8080 if not set.

**Solution**:
```yaml
apiVersion: v1
kind: Service
metadata:
  name: {{ include "order-service.fullname" . }}
  labels:
    {{- include "order-service.labels" . | nindent 4 }}
spec:
  type: {{ .Values.service.type | default "ClusterIP" }}
  ports:
    - name: http
      port: {{ .Values.service.port }}
      targetPort: {{ .Values.service.targetPort | default 8080 }}
  selector:
    {{- include "order-service.selectorLabels" . | nindent 4 }}
```
</details>

### Exercise 3: Complete Pre-Upgrade Migration Hook with Cleanup Policy (Hard)
<details>
<summary>Click to view challenge & solution</summary>

**Challenge**: Create a complete `templates/migration-job.yaml` that runs before an upgrade, mounts a secret for database password, and automatically deletes the job before the next upgrade run.

**Solution**:
```yaml
apiVersion: batch/v1
kind: Job
metadata:
  name: {{ include "order-service.fullname" . }}-migrate-{{ .Release.Revision }}
  annotations:
    "helm.sh/hook": pre-upgrade,pre-install
    "helm.sh/hook-weight": "-5"
    "helm.sh/hook-delete-policy": before-hook-creation,hook-succeeded
spec:
  backoffLimit: 1
  template:
    spec:
      restartPolicy: Never
      containers:
        - name: flyway-migration
          image: "{{ .Values.image.repository }}:{{ .Values.image.tag | default .Chart.AppVersion }}"
          command: ["java", "-cp", "app.jar", "org.springframework.boot.loader.launch.JarLauncher", "--spring.profiles.active=migration"]
          env:
            - name: DATABASE_PASSWORD
              valueFrom:
                secretKeyRef:
                  name: {{ include "order-service.fullname" . }}-db-secret
                  key: password
```
</details>

---

## 12. Summary / Cheat Sheet

```
+---------------------------------------------+-------------------------------------------------------------+
| Command / Directive                         | Functionality & Best Practice                               |
+---------------------------------------------+-------------------------------------------------------------+
| helm lint <path>                            | Validates chart template syntax and values structure        |
| helm template <path> -f values.yaml         | Locally renders manifests for inspection and CI checks      |
| helm upgrade --install <rel> <path>         | Idempotently creates or updates a release                   |
| helm rollback <rel> <rev>                   | Instantly reverts cluster to a previous working revision    |
| nindent <n>                                 | Prepends newline and indents block by n spaces              |
| helm.sh/hook: pre-upgrade                   | Hook executing DB migration Jobs prior to deployment rollout|
| helm.sh/hook-delete-policy                  | Cleans up completed hook pods to prevent naming collisions  |
+---------------------------------------------+-------------------------------------------------------------+
```

---

## 13. Further Reading
- Helm Official Documentation: Chart Template Developer's Guide
- CNCF Artifact Hub: Best Practices for Kubernetes Packaging
- Sprig Template Function Documentation
- SRE Deployment Strategies: Automated Database Migrations with Helm Hooks