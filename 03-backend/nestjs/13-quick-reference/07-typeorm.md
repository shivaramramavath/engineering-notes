# TypeORM in NestJS Cheat Sheet

### Common Repository Methods
- `repo.find({ where: { status: 'ACTIVE' }, relations: ['profile'], order: { createdAt: 'DESC' } })`
- `repo.findOneBy({ id })`
- `repo.save(entity)` $	o$ Inserts or updates
- `repo.softDelete(id)` $	o$ Sets `deleted_at = NOW()`
- `repo.update({ id }, { status: 'INACTIVE' })`

### QueryBuilder with Lock
```typescript
const item = await repo.createQueryBuilder('item')
  .setLock('pessimistic_write')
  .where('item.id = :id', { id })
  .getOne();
```
