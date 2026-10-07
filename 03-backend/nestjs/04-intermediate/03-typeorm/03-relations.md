# Relations

Relations connect entities: a user has many posts, a post belongs to a user, posts have many tags. In SQL they become **foreign keys** (and join tables for many-to-many). TypeORM gives you decorators to declare them and several ways to load them. The decisions that matter: **which side owns the key**, **how and when relations are loaded**, and **what cascades**.

Prerequisites: [Entities](./02-entities.md), [indexing and N+1](../02-database-foundations/06-indexing-and-query-basics.md).

## The four relation types

| Type | Example | Foreign key lives on |
|------|---------|----------------------|
| `@ManyToOne` / `@OneToMany` | Post → User | The **many** side (`post.author_id`) |
| `@OneToOne` | User ↔ Profile | The side with `@JoinColumn()` |
| `@ManyToMany` | Post ↔ Tag | A **join table** (side with `@JoinTable()` owns it) |

### One-to-many / many-to-one

```ts
@Entity('users')
export class User {
  @PrimaryGeneratedColumn('uuid') id: string;

  @OneToMany(() => Post, (post) => post.author)
  posts: Post[];                                 // inverse side: no column in `users`
}

@Entity('posts')
export class Post {
  @PrimaryGeneratedColumn('uuid') id: string;

  @ManyToOne(() => User, (user) => user.posts, { nullable: false, onDelete: 'CASCADE' })
  @JoinColumn({ name: 'author_id' })
  author: User;                                  // owning side: creates author_id

  @Column({ name: 'author_id' })
  authorId: string;                              // optional: expose the FK as a plain column
}
```

- `@ManyToOne` is the **owning** side and creates the foreign key column.
- `@OneToMany` is the inverse and **requires** a matching `@ManyToOne`.
- Declaring `authorId` alongside the relation lets you filter and assign by id without loading the user:

```ts
await posts.save(posts.create({ title: 'Hi', authorId: userId }));
await posts.find({ where: { authorId: userId } });
```

Make sure the `name` in `@JoinColumn` and `@Column` match, so both refer to the same column.

### One-to-one

```ts
@Entity()
export class Profile {
  @PrimaryGeneratedColumn() id: number;
  @Column() bio: string;
}

@Entity()
export class User {
  @OneToOne(() => Profile, { cascade: true })
  @JoinColumn()                                  // User owns the FK (profileId)
  profile: Profile;
}
```

`@JoinColumn()` goes on **exactly one** side: the table that holds the foreign key.

### Many-to-many

```ts
@Entity()
export class Post {
  @ManyToMany(() => Tag, (tag) => tag.posts)
  @JoinTable({ name: 'post_tags' })              // owning side: creates the join table
  tags: Tag[];
}

@Entity()
export class Tag {
  @ManyToMany(() => Post, (post) => post.tags)
  posts: Post[];
}
```

If the link needs **extra data** (who added the tag, when, an order), `@ManyToMany` can't hold it. Model the join as its own entity with two `@ManyToOne`s:

```ts
@Entity('post_tags')
export class PostTag {
  @ManyToOne(() => Post, { onDelete: 'CASCADE' }) post: Post;
  @ManyToOne(() => Tag, { onDelete: 'CASCADE' }) tag: Tag;
  @Column() addedBy: string;
}
```

## Loading relations

By default relations are **not** loaded. Ask for them explicitly:

```ts
// find options
await users.find({ relations: { posts: true } });
await users.findOne({ where: { id }, relations: { posts: { tags: true } } });   // nested

// query builder
await users.createQueryBuilder('u').leftJoinAndSelect('u.posts', 'p').where('u.id = :id', { id }).getOne();
```

| Strategy | Behavior | Verdict |
|----------|----------|---------|
| **Explicit `relations`/joins** | You choose per query | **Recommended** |
| **`eager: true`** on a relation | Always loaded with `find*` methods | Convenient but hides cost; doesn't apply to QueryBuilder; easy to over-fetch |
| **Lazy relations** (`Promise<Post[]>` typed property) | Loaded on first `await user.posts` | Discouraged: hidden queries, awkward typing, N+1 traps |

Explicit loading keeps queries visible. Every `relations` entry adds joins, so load only what the endpoint returns. Deeply nested `relations` can explode row counts.

### Load strategy: join vs separate queries

`find` options support choosing how relations are fetched:

```ts
await users.find({ relations: { posts: true }, relationLoadStrategy: 'query' });
```

`'join'` fetches everything in one joined query; `'query'` issues one query per relation and merges results, which can be cheaper for large collections (it avoids duplicating the parent row for each child). Check your TypeORM version supports it and measure on real data.

### Avoiding N+1

```ts
// ❌ N+1
const posts = await postsRepo.find();
for (const p of posts) p.author = await usersRepo.findOneBy({ id: p.authorId });

// ✅ one query with a join
const posts = await postsRepo.find({ relations: { author: true } });
```

Turn on `logging: ['query']` in development and watch for repeated queries per request.

### Selecting columns of relations

```ts
await posts.find({
  relations: { author: true },
  select: { id: true, title: true, author: { id: true, name: true } },
});
```

Restrict columns to avoid loading large or sensitive fields along with the relation.

## Saving relations

```ts
// by id (no need to load the related entity)
await posts.save({ title: 'Hi', author: { id: userId } });

// many-to-many: replace the set
post.tags = [{ id: 1 } as Tag, { id: 2 } as Tag];
await posts.save(post);

// add/remove without loading everything
await dataSource.createQueryBuilder().relation(Post, 'tags').of(postId).add(tagId);
await dataSource.createQueryBuilder().relation(Post, 'tags').of(postId).remove(tagId);
```

Assigning `post.tags` and saving **replaces** the join rows to match; if you loaded only a subset, you may delete links you didn't mean to. For incremental changes prefer the `relation().add/remove` builder or an explicit join entity.

## Cascades and deletes

Two different mechanisms with confusingly similar names:

| Option | Where it acts | Meaning |
|--------|---------------|---------|
| `cascade: true` / `['insert','update', ...]` | **ORM** (in your app) | Saving the parent also saves related child entities passed with it |
| `onDelete: 'CASCADE'` / `'SET NULL'` / `'RESTRICT'` | **Database** foreign key | The database deletes/nullifies/blocks children when the parent is deleted |

```ts
@OneToMany(() => Comment, (c) => c.post, { cascade: ['insert'] })   // saving a Post with new comments inserts them
comments: Comment[];

@ManyToOne(() => Post, (p) => p.comments, { onDelete: 'CASCADE' }) // deleting a post deletes its comments in the DB
post: Post;
```

Guidelines:

- Prefer **database-level** `onDelete` for referential behavior; it works regardless of how rows are deleted (including `delete()` and raw SQL).
- Use ORM `cascade` sparingly, for aggregate-style saves (an order and its lines). `cascade: true` everywhere makes it easy to modify related data accidentally.
- `delete(id)` and `softDelete` don't walk the object graph; ORM cascades apply to `save`/`remove` on entity instances.
- Be deliberate about `CASCADE` on deletes of important data; `RESTRICT` makes mistakes loud.

## Circular imports between entities

Entities reference each other, which can cause import cycles and "cannot access before initialization" errors (especially with ESM). Use the `Relation<T>` wrapper for the property type:

```ts
import { Relation } from 'typeorm';

@ManyToOne(() => User, (u) => u.posts)
author: Relation<User>;
```

It prevents TypeScript from emitting an eager reference to the other class. Prefer direct file imports over barrel files for entities ([circular dependencies](../../03-core-concepts/04-modules-and-di/04-circular-dependencies.md)).

## Pagination with joins

Joining a collection multiplies parent rows, which breaks naive `LIMIT`. With `find` options, `take`/`skip` are handled correctly (TypeORM selects distinct parent ids first). With QueryBuilder, use `take()`/`skip()` rather than `limit()`/`offset()` when joins are involved ([query builder](./05-query-builder.md)).

## Common mistakes

- **Expecting relations to be loaded** by default, then reading `undefined`.
- **`eager: true` on many relations**, causing over-fetching everywhere.
- **Lazy relations** that hide queries.
- **`@JoinColumn()` on both sides** of a one-to-one, or on the wrong side.
- **Missing inverse side** or a wrong inverse property function for bidirectional relations.
- **N+1 queries** from looping and querying.
- **Reassigning a many-to-many array loaded partially**, silently dropping links.
- **Confusing ORM `cascade` with database `onDelete`.**
- **No index on foreign key columns.**
- **Deep `relations` trees** producing huge result sets.
- **Mismatched `@JoinColumn` name and the plain FK column**, creating two columns.

## Debugging

- Log queries and read the SQL for the endpoint; count them.
- Relation property is `undefined`: you didn't load it (`relations`, join, or `eager`).
- Duplicate FK columns (`authorId` and `authorIdId`): the plain column and `@JoinColumn` names disagree.
- "Cannot delete or update a parent row: a foreign key constraint fails": children exist and `onDelete` is `RESTRICT`/default; handle deliberately ([database errors](../02-database-foundations/07-database-errors.md)).
- Missing rows in a join result: `leftJoin` vs `innerJoin`, or a `where` on the joined table turning a left join into an inner one.

## Quick Summary

- `@ManyToOne` owns the FK; `@OneToMany` is the inverse; `@OneToOne` needs `@JoinColumn()` on one side; `@ManyToMany` needs `@JoinTable()` on one side (use a join entity for extra columns).
- Load relations explicitly (`relations`/joins); avoid lazy relations and be careful with `eager`.
- Fix N+1 with joins or `relations`; select only needed columns.
- ORM `cascade` saves related entities; database `onDelete` enforces referential behavior. Prefer the latter for deletes.
- Use `Relation<T>` to avoid circular-import issues; index foreign keys.

## Next

[Repositories →](./04-repositories.md)
