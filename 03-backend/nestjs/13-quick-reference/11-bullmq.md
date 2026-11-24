# BullMQ in NestJS Cheat Sheet

### Producer & Consumer
```typescript
// Producer
await queue.add('sendEmail', { email, template: 'welcome' }, {
  attempts: 3,
  backoff: { type: 'exponential', delay: 1000 },
});

// Consumer
@Processor('emails')
export class EmailProcessor extends WorkerHost {
  async process(job: Job) {
    // Process email
  }
}
```
