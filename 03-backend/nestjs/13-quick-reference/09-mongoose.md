# Mongoose in NestJS Cheat Sheet

```typescript
@Schema({ timestamps: true })
export class Cat {
  @Prop({ required: true, unique: true })
  name: string;

  @Prop()
  age: number;
}
export const CatSchema = SchemaFactory.createForClass(Cat);

// Injection
constructor(@InjectModel(Cat.name) private catModel: Model<Cat>) {}
```
