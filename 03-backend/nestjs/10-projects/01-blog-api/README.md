# Project 01: Headless CMS and Blog REST API (`01-blog-api`)

> Production-grade RESTful content management and blogging API built with NestJS 11, Express 5, TypeORM, PostgreSQL, Passport JWT authentication, Role-Based Access Control (RBAC), and OpenAPI/Swagger documentation.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-nest-01-blog-api` |
| **NestJS Version** | NestJS 11+ |
| **Node.js Baseline** | Node.js >= 20.0.0 (Node.js 22 LTS) |
| **HTTP Platform** | Express 5 |
| **Database & ORM** | PostgreSQL 16 + TypeORM 0.3+ |
| **Auth & Security** | Passport JWT, Argon2id, Role Guards (`Admin`, `Author`, `Reader`) |
| **Testing** | Jest + Supertest (Unit & E2E) |
| **Complexity Level** | Intermediate Production Reference |

---

## 1. Architectural Overview & Domain Design

### 1.1 System Architecture
The application follows a clean layered domain architecture separating Presentation (Controllers, DTOs), Domain (Entities, Invariants), and Infrastructure (TypeORM Repositories, Migrations).

```
+----------------------------------------------------------------------------------------------------+
|                                      01-blog-api Architecture                                       |
|                                                                                                    |
|  [ Client / Mobile / SPA ] ---> [ Nginx Reverse Proxy / Port 80/443 ]                              |
|                                             |                                                      |
|                                             v                                                      |
|                             [ NestJS 11 App / Port 3000 ]                                          |
|                                             |                                                      |
|                  +--------------------------+--------------------------+                           |
|                  |                          |                          |                           |
|                  v                          v                          v                           |
|           [ AuthModule ]             [ PostsModule ]            [ CommentsModule ]                 |
|           - JwtStrategy              - SlugGenerator            - Nested Replies                   |
|           - RolesGuard               - Markdown Sanitizer       - Moderation Flagging              |
|                  |                          |                          |                           |
|                  +--------------------------+--------------------------+                           |
|                                             |                                                      |
|                                             v (TypeORM Repository Pool)                            |
|                                  [ PostgreSQL 16 Database ]                                        |
+----------------------------------------------------------------------------------------------------+
```

### 1.2 Directory Structure
```
01-blog-api/
├── README.md
├── package.json
├── tsconfig.json
├── docker-compose.yml
├── Dockerfile
└── src/
    ├── main.ts
    ├── app.module.ts
    ├── auth/
    │   ├── auth.module.ts
    │   ├── auth.service.ts
    │   ├── auth.controller.ts
    │   ├── guards/jwt-auth.guard.ts
    │   └── guards/roles.guard.ts
    ├── posts/
    │   ├── posts.module.ts
    │   ├── posts.controller.ts
    │   ├── posts.service.ts
    │   ├── entities/post.entity.ts
    │   └── dtos/create-post.dto.ts
    └── comments/
        ├── comments.module.ts
        ├── comments.controller.ts
        └── entities/comment.entity.ts
```

---

## 2. Complete Production Implementation

### 2.1 Configuration & Bootstrap

```json
// package.json
{
  "name": "blog-api",
  "version": "1.0.0",
  "scripts": {
    "build": "nest build",
    "start": "nest start",
    "start:prod": "node dist/main",
    "test": "jest",
    "test:e2e": "jest --config ./test/jest-e2e.json"
  },
  "dependencies": {
    "@nestjs/common": "^11.0.0",
    "@nestjs/core": "^11.0.0",
    "@nestjs/platform-express": "^11.0.0",
    "@nestjs/typeorm": "^11.0.0",
    "@nestjs/jwt": "^11.0.0",
    "@nestjs/passport": "^11.0.0",
    "@nestjs/swagger": "^11.0.0",
    "typeorm": "^0.3.20",
    "pg": "^8.13.0",
    "passport-jwt": "^4.0.1",
    "argon2": "^0.41.0",
    "class-validator": "^0.14.1",
    "class-transformer": "^0.5.1",
    "slugify": "^1.6.6"
  }
}
```

```typescript
// src/main.ts
import { NestFactory } from '@nestjs/core';
import { AppModule } from './app.module';
import { ValidationPipe } from '@nestjs/common';
import { DocumentBuilder, SwaggerModule } from '@nestjs/swagger';

async function bootstrap() {
  const app = await NestFactory.create(AppModule);

  app.useGlobalPipes(
    new ValidationPipe({
      whitelist: true,
      forbidNonWhitelisted: true,
      transform: true,
    }),
  );

  const config = new DocumentBuilder()
    .setTitle('Headless Blog API')
    .setDescription('Enterprise Blog and Content API with JWT and RBAC')
    .setVersion('1.0')
    .addBearerAuth()
    .build();
  const document = SwaggerModule.createDocument(app, config);
  SwaggerModule.setup('docs', app, document);

  app.enableShutdownHooks();
  await app.listen(3000);
}
bootstrap();
```

### 2.2 Core Domain Entity & DTO

```typescript
// src/posts/entities/post.entity.ts
import {
  Entity,
  PrimaryGeneratedColumn,
  Column,
  CreateDateColumn,
  UpdateDateColumn,
  Index,
} from 'typeorm';

export enum PostStatus {
  DRAFT = 'DRAFT',
  PUBLISHED = 'PUBLISHED',
  ARCHIVED = 'ARCHIVED',
}

@Entity('posts')
export class PostEntity {
  @PrimaryGeneratedColumn('uuid')
  id: string;

  @Column({ length: 255 })
  title: string;

  @Index({ unique: true })
  @Column({ length: 255 })
  slug: string;

  @Column({ type: 'text' })
  content: string;

  @Column({
    type: 'enum',
    enum: PostStatus,
    default: PostStatus.DRAFT,
  })
  status: PostStatus;

  @Column({ name: 'author_id' })
  authorId: string;

  @CreateDateColumn({ name: 'created_at' })
  createdAt: Date;

  @UpdateDateColumn({ name: 'updated_at' })
  updatedAt: Date;
}
```

```typescript
// src/posts/dtos/create-post.dto.ts
import { IsString, IsNotEmpty, MinLength, MaxLength, IsEnum, IsOptional } from 'class-validator';
import { ApiProperty } from '@nestjs/swagger';
import { PostStatus } from '../entities/post.entity';

export class CreatePostDto {
  @ApiProperty({ example: 'Architecting NestJS 11 Systems' })
  @IsString()
  @IsNotEmpty()
  @MinLength(5)
  @MaxLength(255)
  title: string;

  @ApiProperty({ example: 'Deep dive into NestJS 11 enterprise patterns...' })
  @IsString()
  @IsNotEmpty()
  content: string;

  @ApiProperty({ enum: PostStatus, default: PostStatus.DRAFT })
  @IsEnum(PostStatus)
  @IsOptional()
  status?: PostStatus;
}
```

### 2.3 Posts Service & Controller

```typescript
// src/posts/posts.service.ts
import { Injectable, ConflictException, NotFoundException } from '@nestjs/common';
import { InjectRepository } from '@nestjs/typeorm';
import { Repository } from 'typeorm';
import { PostEntity, PostStatus } from './entities/post.entity';
import { CreatePostDto } from './dtos/create-post.dto';
import slugify from 'slugify';

@Injectable()
export class PostsService {
  constructor(
    @InjectRepository(PostEntity)
    private readonly postRepo: Repository<PostEntity>,
  ) {}

  async create(dto: CreatePostDto, authorId: string): Promise<PostEntity> {
    const slug = slugify(dto.title, { lower: true, strict: true });
    const existing = await this.postRepo.findOne({ where: { slug } });
    if (existing) {
      throw new ConflictException(`A post with slug "${slug}" already exists`);
    }

    const post = this.postRepo.create({
      ...dto,
      slug,
      authorId,
    });
    return this.postRepo.save(post);
  }

  async findBySlug(slug: string): Promise<PostEntity> {
    const post = await this.postRepo.findOne({ where: { slug, status: PostStatus.PUBLISHED } });
    if (!post) {
      throw new NotFoundException(`Post "${slug}" not found`);
    }
    return post;
  }
}
```

---

## 3. Automated Testing Suite

```typescript
// test/posts.e2e-spec.ts
import { Test, TestingModule } from '@nestjs/testing';
import { INestApplication, ValidationPipe } from '@nestjs/common';
import * as request from 'supertest';
import { AppModule } from '../src/app.module';

describe('Posts API (E2E)', () => {
  let app: INestApplication;

  beforeAll(async () => {
    const moduleFixture: TestingModule = await Test.createTestingModule({
      imports: [AppModule],
    }).compile();

    app = moduleFixture.createNestApplication();
    app.useGlobalPipes(new ValidationPipe({ whitelist: true }));
    await app.init();
  });

  afterAll(async () => {
    await app.close();
  });

  it('POST /posts - rejects short titles with 400 Bad Request', () => {
    return request(app.getHttpServer())
      .post('/posts')
      .send({ title: 'Hi', content: 'Valid content' })
      .expect(400);
  });
});
```

---

## 4. Production Deployment & Verification

```yaml
# docker-compose.yml
version: '3.8'
services:
  postgres:
    image: postgres:16-alpine
    environment:
      POSTGRES_DB: blog_db
      POSTGRES_USER: nest
      POSTGRES_PASSWORD: secretpassword
    ports:
      - "5432:5432"
    volumes:
      - pgdata:/var/lib/postgresql/data

  api:
    build: .
    ports:
      - "3000:3000"
    environment:
      DATABASE_URL: postgres://nest:secretpassword@postgres:5432/blog_db
      JWT_SECRET: supersecretjwtkeymustbe32charslong!
    depends_on:
      - postgres

volumes:
  pgdata:
```

### Verification Commands
```bash
# Start infrastructure
docker-compose up -d

# Verify Swagger API Docs
curl -i http://localhost:3000/docs
```
