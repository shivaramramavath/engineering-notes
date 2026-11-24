# DevOps Curriculum Architecture: Master Progress Tracker

## 1. Global Track Status Overview

| Track | Target Directory | Module Count | Existing Notes | Planned Final Notes | Track Status |
|---|---|:---:|:---:|:---:|:---:|
| **Shared Foundation** | `04-devops/` (Root & Templates) | 1 | 0 | 11 Governance Files | **100% Done** |
| **Linux Systems** | `04-devops/linux/` | 9 | 24 | 28 Notes | Foundation Ready (Prompt Pending) |
| **Git & GitHub Actions**| `04-devops/git-github/` | 7 | 33 | 36 Notes | Foundation Ready (Prompt Pending) |
| **Docker Containers** | `04-devops/docker/` | 9 | 22 | 26 Notes | Foundation Ready (Prompt Pending) |
| **Nginx Web Gateway** | `04-devops/nginx/` | 9 | 24 | 27 Notes | Foundation Ready (Prompt Pending) |
| **Amazon Web Services** | `04-devops/aws/` | 9 | 25 | 30 Notes | Foundation Ready (Prompt Pending) |
| **Total DevOps Area** | `04-devops/` | **44 Modules** | **128 Files** | **158 Notes** | **Phase 1 In Progress** |

---

## 2. Shared Foundation Deliverables Checklist

- [x] **Phase 0 Technical Facts**: [`DEVOPS_FACTS.md`](file:///04-devops/DEVOPS_FACTS.md) (Done)
- [x] **Phase 1 Baseline Audit**: [`DEVOPS_AUDIT.md`](file:///04-devops/DEVOPS_AUDIT.md) (Done)
- [x] **Phase 2 Curriculum & Rename Plan**: [`DEVOPS_COMPLETION_PLAN.md`](file:///04-devops/DEVOPS_COMPLETION_PLAN.md) (Done)
- [x] **Master Conventions & Standard**: [`CONVENTIONS.md`](file:///04-devops/CONVENTIONS.md) (Done)
- [x] **Authoritative Concept Template**: [`_templates/concept-template.md`](file:///04-devops/_templates/concept-template.md) (Done)
- [x] **Authoritative Project Template**: [`_templates/project-template.md`](file:///04-devops/_templates/project-template.md) (Done)
- [x] **Curriculum Root README**: [`README.md`](file:///04-devops/README.md) (Done)
- [x] **Curriculum Root Master Index**: [`INDEX.md`](file:///04-devops/INDEX.md) (Done)
- [x] **Curriculum Master Glossary**: [`GLOSSARY.md`](file:///04-devops/GLOSSARY.md) (Done)
- [x] **Curriculum Master Roadmap**: [`ROADMAP.md`](file:///04-devops/ROADMAP.md) (Done)
- [x] **Authoring Guidelines**: [`CONTRIBUTING.md`](file:///04-devops/CONTRIBUTING.md) (Done)
- [x] **Architecture Decision Records**: [`DEVOPS_DECISIONS.md`](file:///04-devops/DEVOPS_DECISIONS.md) (Done)
- [x] **Changelog & Audit Log**: [`DEVOPS_CHANGELOG.md`](file:///04-devops/DEVOPS_CHANGELOG.md) (Done)
- [x] **Final Delivery Report**: [`DEVOPS_FINAL_REPORT.md`](file:///04-devops/DEVOPS_FINAL_REPORT.md) (Done)

---

## 3. Per-Track Execution Checklist (For Track Prompts)

### 3.1 Track: `linux/`
- [ ] Rename `reference/cheatsheet.md` $\rightarrow$ `07-cheatsheets/01-linux-cheatsheet.md`
- [ ] Rename `reference/interview.md` $\rightarrow$ `08-interview/01-linux-interview-qa.md`
- [ ] Create `09-projects/` module with reference system automation projects
- [ ] Create per-module `README.md` files for modules 01 through 09
- [ ] Update root `linux/README.md` from 0-byte stub to full track portal
- [ ] Standardize all 24 concept notes to 13-section template

### 3.2 Track: `git-github/`
- [ ] Rename snake folders `00_setup` ... `04_github_actions` to kebab `00-setup` ... `04-github-actions`
- [ ] Rename files `01_name.md` to `01-name.md`
- [ ] Rename `getting-started.md` $\rightarrow$ `01-getting-started.md`
- [ ] Ingest 5 raw YAML files in `examples/*.yml` into fenced code blocks; delete `examples/`
- [ ] Move and rename `CHEATSHEET.md` $\rightarrow$ `06-cheatsheets/01-git-and-actions-cheatsheet.md`
- [ ] Create `05-projects/` with production CI/CD automation projects
- [ ] Create `07-interview/` with Git internals and GitHub Actions interview Q&A
- [ ] Create per-module `README.md` files

### 3.3 Track: `docker/`
- [ ] Rename snake folders `00_setup` ... `06_production` to kebab `00-setup` ... `06-production`
- [ ] Rename files `01_name.md` to `01-name.md`
- [ ] Rename `getting-started.md` $\rightarrow$ `01-getting-started.md`
- [ ] Create `07-projects/` with reference multi-tier container stacks
- [ ] Create `08-interview/` with Docker container runtime interview Q&A
- [ ] Create `09-cheatsheets/` with Docker CLI & Compose syntax reference
- [ ] Standardize per-module `README.md` files

### 3.4 Track: `nginx/`
- [ ] Add numeric prefixes to all unnumbered notes (`01-config-structure.md`, `02-installation.md`, etc.)
- [ ] Split `08-reference/` into `08-interview/` and `09-cheatsheets/`
- [ ] Verify HTTP/3 QUIC syntax and TLS 1.3 hardening recipes
- [ ] Standardize all 24 concept notes to 13-section template
- [ ] Create per-module `README.md` files

### 3.5 Track: `aws/`
- [ ] Rename `reference/cheatsheet.md` $\rightarrow$ `07-cheatsheets/01-aws-cheatsheet.md`
- [ ] Rename `reference/interview.md` $\rightarrow$ `08-interview/01-aws-interview-qa.md`
- [ ] Create `09-projects/` with end-to-end cloud reference architectures
- [ ] Create per-module `README.md` files for modules 01 through 09
- [ ] Update root `aws/README.md` from 0-byte stub to full track portal
- [ ] Standardize all 25 concept notes to 13-section template
