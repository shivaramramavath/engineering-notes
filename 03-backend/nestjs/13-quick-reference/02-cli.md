# Nest CLI Cheat Sheet

```bash
# Project & Monorepo Creation
nest new <project-name>           # Scaffolds new NestJS project
nest new <monorepo> --monorepo    # Scaffolds monorepo workspace

# Code Generation (nest g <schematic> <name>)
nest g module users               # Generates users.module.ts
nest g controller users           # Generates users.controller.ts
nest g service users              # Generates users.service.ts
nest g resource products          # Scaffolds complete CRUD REST/GraphQL/Microservice
nest g guard auth                 # Generates auth.guard.ts
nest g interceptor logging        # Generates logging.interceptor.ts
nest g pipe validation            # Generates validation.pipe.ts
nest g filter http-exception      # Generates http-exception.filter.ts

# Build & Execution
nest start                        # Runs development server
nest start --watch                # Hot-reload development server
nest build                        # Compiles TypeScript to dist/
nest info                         # Prints platform, CLI, and package versions
```
