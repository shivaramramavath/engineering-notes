# Pattern Identification from Real-World Code

1. **`new BufferedReader(new InputStreamReader(new FileInputStream("a.txt")))`**
   <details><summary>Answer</summary><strong>Decorator</strong> (stacking buffering/encoding onto `InputStream`/`Reader`) + <strong>Adapter</strong> (`InputStreamReader` adapts byte stream to char stream).</details>

2. **`list.sort((a, b) -> a.age() - b.age())`**
   <details><summary>Answer</summary><strong>Strategy</strong> (passed as a functional comparator).</details>

3. **`Arrays.asList(1, 2, 3)` / `Collections.unmodifiableList(list)`**
   <details><summary>Answer</summary><strong>Adapter</strong> (`Arrays.asList` adapts array to `List`) & <strong>Proxy</strong> (`unmodifiableList` blocks mutating calls).</details>

4. **`StringBuilder().append("a").append("b").toString()`**
   <details><summary>Answer</summary><strong>Builder</strong> (step-by-step mutable construction producing an immutable `String`).</details>

5. **`app.use(cors()); app.use(auth()); app.use(router);`**
   <details><summary>Answer</summary><strong>Chain of Responsibility / Onion Middleware</strong>.</details>
