# GraphQL (Code-First) Cheat Sheet

```typescript
@ObjectType()
export class Post {
  @Field(() => ID)
  id: string;

  @Field()
  title: string;
}

@Resolver(() => Post)
export class PostResolver {
  @Query(() => [Post])
  async posts(): Promise<Post[]> { return []; }
}
```
