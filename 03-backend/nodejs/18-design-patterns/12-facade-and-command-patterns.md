# Facade & Command Patterns in Node.js

> Simplifying complex subsystems with the Facade pattern and encapsulating requests as standalone invokable objects with the Command pattern.

| Metadata | Details |
| :--- | :--- |
| **Node.js Baseline** | Node.js 22 LTS / 24+ |
| **Patterns** | Structural (Facade), Behavioral (Command) |

---

## 1. The Facade Pattern

Provides a unified, simplified interface to a set of interfaces in a complex subsystem (e.g. video processing, multi-step cloud uploads):

```typescript
export class VideoProcessingFacade {
  constructor(
    private readonly transcoder: FFmpegTranscoder,
    private readonly s3Storage: S3Uploader,
    private readonly notifier: PushNotifier
  ) {}

  async processAndPublish(rawVideoPath: string, userId: string) {
    // Coordinates 3 complex subsystems behind 1 simple method
    const optimized1080p = await this.transcoder.encode(rawVideoPath, '1080p');
    const cdnUrl = await this.s3Storage.upload(optimized1080p);
    await this.notifier.notifyUser(userId, 'Video ready!', cdnUrl);
    return { cdnUrl };
  }
}
```

---

## 2. The Command Pattern

Encapsulates a request as an object, thereby letting you parameterize clients with different requests, queue or log requests, and support undoable operations:

```typescript
export interface Command<TResult = void> {
  execute(): Promise<TResult>;
  undo?(): Promise<void>;
}

export class CreateUserCommand implements Command<User> {
  constructor(private readonly userData: CreateUserDTO, private readonly userRepo: UserRepository) {}

  async execute(): Promise<User> {
    const user = new User(this.userData);
    await this.userRepo.save(user);
    return user;
  }
}
```
Enables clean command buses, background job serialization, and transactional undo operations.
