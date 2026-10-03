# Streams

A **stream** processes data in **chunks** over time instead of all at once. Streams keep memory use flat for large inputs, start producing output early, and let you compose steps (read → transform → compress → write). Files, HTTP requests and responses, sockets, `stdin`/`stdout`, and `zlib` are all streams.

## Why streams

```js
// Loads the whole 4 GB file into memory, then responds
const data = await fs.readFile('huge.mp4');
res.end(data);

// Constant memory: chunks flow from disk to the socket
createReadStream('huge.mp4').pipe(res);
```

## Four kinds

| Type | Direction | Examples |
|------|-----------|----------|
| **Readable** | Source of data | `fs.createReadStream`, HTTP request (server), `process.stdin` |
| **Writable** | Destination | `fs.createWriteStream`, HTTP response (server), `process.stdout` |
| **Duplex** | Both, independently | TCP socket |
| **Transform** | Duplex that modifies data | `zlib.createGzip()`, `crypto.createCipheriv` |

All streams are `EventEmitter`s. By default they work with `Buffer` or `string` chunks; in **object mode** they carry any JavaScript value.

## Reading

### Async iteration (preferred)

```js
import { createReadStream } from 'node:fs';

const stream = createReadStream('data.txt', { encoding: 'utf8' });
for await (const chunk of stream) {
  console.log(chunk.length);
}
```

The loop respects backpressure automatically and rejects if the stream errors.

### Events

```js
stream.on('data', (chunk) => { /* flowing mode */ });
stream.on('end', () => { /* no more data */ });
stream.on('error', (err) => { /* handle */ });
stream.on('close', () => { /* resources released */ });
```

Attaching a `'data'` listener switches the stream into **flowing mode**; `pause()` / `resume()` control it. The default `highWaterMark` for byte streams is 64 KiB for files (16 KiB for most others).

## Writing

```js
import { createWriteStream } from 'node:fs';

const out = createWriteStream('out.txt');
out.write('line 1\n');
out.write('line 2\n');
out.end('last line\n');          // flush and close

out.on('finish', () => console.log('all data flushed'));
out.on('error', console.error);
```

## Backpressure

`write()` returns `false` when the internal buffer is full. A producer that ignores this fills memory.

```js
function writeMany(out, lines) {
  let i = 0;
  function pump() {
    while (i < lines.length) {
      const ok = out.write(lines[i++] + '\n');
      if (!ok) {
        out.once('drain', pump);     // wait until the buffer empties
        return;
      }
    }
    out.end();
  }
  pump();
}
```

With `pipe`, `pipeline`, and `for await`, backpressure is handled for you. Using `once` from `node:events`:

```js
import { once } from 'node:events';

for (const line of lines) {
  if (!out.write(line + '\n')) await once(out, 'drain');
}
out.end();
```

## `pipeline`: the right way to connect streams

`readable.pipe(writable)` does **not** forward errors or clean up on failure. Use `pipeline`:

```js
import { pipeline } from 'node:stream/promises';
import { createReadStream, createWriteStream } from 'node:fs';
import { createGzip } from 'node:zlib';

try {
  await pipeline(
    createReadStream('app.log'),
    createGzip(),
    createWriteStream('app.log.gz'),
  );
  console.log('compressed');
} catch (err) {
  console.error('pipeline failed', err);   // all streams are destroyed
}
```

`pipeline` also accepts async generators as transform steps:

```js
await pipeline(
  createReadStream('names.txt', 'utf8'),
  async function* (source) {
    for await (const chunk of source) yield chunk.toUpperCase();
  },
  createWriteStream('upper.txt'),
);
```

Cancel with a signal:

```js
await pipeline(src, dest, { signal: AbortSignal.timeout(10_000) });
```

## Creating streams

### From an iterable

```js
import { Readable } from 'node:stream';

const r = Readable.from(['a', 'b', 'c']);                       // objectMode by default
const lines = Readable.from(generateLines());                    // async generator works too

for await (const x of r) console.log(x);
```

### Custom Readable

```js
class Counter extends Readable {
  #n = 0;
  _read() {
    this.push(this.#n < 5 ? String(this.#n++) : null);   // null = end of stream
  }
}
```

### Custom Writable

```js
import { Writable } from 'node:stream';

const sink = new Writable({
  write(chunk, encoding, callback) {
    console.log('got', chunk.toString());
    callback();                  // call when done; pass an Error to fail
  },
});
```

### Transform

```js
import { Transform } from 'node:stream';

const upper = new Transform({
  transform(chunk, encoding, callback) {
    callback(null, chunk.toString().toUpperCase());
  },
});

process.stdin.pipe(upper).pipe(process.stdout);
```

## Object mode

Chunks are arbitrary values instead of bytes. `highWaterMark` counts **objects** (default 16).

```js
const parseJson = new Transform({
  objectMode: true,
  transform(line, _enc, cb) {
    try { cb(null, JSON.parse(line)); }
    catch (err) { cb(err); }
  },
});
```

## Stream helpers

Readable streams have iterator-style helpers:

```js
const result = await Readable.from([1, 2, 3, 4, 5])
  .filter((n) => n % 2)
  .map((n) => n * 10)
  .toArray();            // [10, 30, 50]
```

Available: `map`, `filter`, `take`, `drop`, `flatMap`, `forEach`, `toArray`, `some`, `every`, `find`, `reduce`.

## Line-by-line processing

```js
import readline from 'node:readline';

const rl = readline.createInterface({ input: createReadStream('big.csv'), crlfDelay: Infinity });
for await (const line of rl) {
  handle(line);
}
```

Chunks do not align with lines or characters. A multi-byte UTF-8 character can be split across chunks; set `encoding: 'utf8'` on the stream (or use `StringDecoder`) rather than calling `chunk.toString()` on raw buffers.

## Streaming HTTP

```js
import http from 'node:http';
import { createReadStream } from 'node:fs';
import { pipeline } from 'node:stream/promises';

http.createServer(async (req, res) => {
  res.setHeader('Content-Type', 'video/mp4');
  try {
    await pipeline(createReadStream('movie.mp4'), res);
  } catch {
    res.destroy();     // client aborted or read failed
  }
}).listen(3000);
```

Upload to a file:

```js
await pipeline(req, createWriteStream('upload.bin'));
```

## Web Streams

Node implements the web-standard `ReadableStream`, `WritableStream`, and `TransformStream`. `fetch` response bodies are web streams. Convert between the two worlds:

```js
import { Readable } from 'node:stream';

const res = await fetch('https://example.com/big.json');

for await (const chunk of res.body) { /* web ReadableStream is async iterable */ }

const nodeStream = Readable.fromWeb(res.body);
const webStream  = Readable.toWeb(nodeStream);
```

## Stream states and errors

```js
stream.destroy();                 // tear down; emits 'close'
stream.destroy(new Error('x'));   // tear down with an error
stream.readableEnded;             // 'end' has been emitted
stream.writableFinished;          // 'finish' has been emitted
```

Always handle `'error'`: an unhandled stream error throws and can crash the process. `pipeline` and `for await` do this for you.

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| `a.pipe(b)` with no error handling | Errors do not propagate; leaks on failure | `pipeline` |
| Ignoring `write()` returning `false` | Unbounded memory | Wait for `'drain'`, or use `pipeline` |
| Reading whole files with `readFile` | Memory spikes | Stream |
| `chunk.toString()` on split UTF-8 | Corrupted characters | Set `encoding`, or use `StringDecoder` |
| Missing `'error'` listener | Crash | Handle it, or use `pipeline` |
| Calling `callback()` twice in `Transform` | Errors | Call exactly once per chunk |
| Mixing `'data'` handlers and async iteration | Chunks stolen or lost | Choose one consumption style |
| Forgetting `end()` on a writable | Consumer waits forever | Call `end()` or use `pipeline` |
| Assuming chunk size or boundaries | Chunks are arbitrary | Buffer and parse across chunks |

## Key takeaways

- Streams process data in chunks: constant memory, early output
- Four kinds: Readable, Writable, Duplex, Transform
- Consume with `for await`; connect with `pipeline` from `node:stream/promises`
- Respect backpressure (`write()` returning `false`, `'drain'`)
- Object mode carries arbitrary values; web streams interoperate through `fromWeb` / `toWeb`

**Next:** [Buffers](./07_buffers.md)
