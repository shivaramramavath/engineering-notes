# `streams` — Processing Data in Chunks

Streams let you process data piece by piece as it arrives, instead of loading everything into memory at once. They're built on `EventEmitter` (`04-events.md`) and underpin `fs`, `http`, and most I/O in Node.

```js
import { Readable, Writable, Transform, pipeline } from "node:stream";
```

## Why streams exist

```js
// ❌ loads the entire file into memory before doing anything with it
const data = await fs.readFile("huge-video.mp4");
processVideo(data);
```

```js
// ✅ processes the file in small chunks, using a small, constant amount of memory
const stream = fs.createReadStream("huge-video.mp4");
stream.pipe(processingStream);
```

For a 10MB file, the difference barely matters. For a 10GB file, the first approach can crash your process; the second handles it identically regardless of size, because it never holds more than a chunk in memory at once.

---

## The four stream types

| Type          | Direction                                 | Examples                                                     |
| ------------- | ----------------------------------------- | ------------------------------------------------------------ |
| **Readable**  | Data flows out of it                      | `fs.createReadStream()`, an HTTP request body (`03-http.md`) |
| **Writable**  | Data flows into it                        | `fs.createWriteStream()`, an HTTP response                   |
| **Duplex**    | Both readable and writable, independently | A TCP socket                                                 |
| **Transform** | Duplex, but output is derived from input  | `zlib.createGzip()`, a CSV parser                            |

---

## Reading from a stream

```js
import { createReadStream } from "node:fs";

const stream = createReadStream("file.txt", { encoding: "utf-8" });

stream.on("data", (chunk) => {
  console.log("Received chunk:", chunk);
});

stream.on("end", () => {
  console.log("Done reading");
});

stream.on("error", (err) => {
  console.error("Stream error:", err); // always handle this — see 04-events.md
});
```

### The modern alternative: `for await`

```js
for await (const chunk of createReadStream("file.txt", { encoding: "utf-8" })) {
  console.log("Received chunk:", chunk);
}
```

Cleaner for straightforward sequential processing, and works because readable streams implement the async iterator protocol.

---

## Writing to a stream

```js
import { createWriteStream } from "node:fs";

const stream = createWriteStream("output.txt");

stream.write("First line\n");
stream.write("Second line\n");
stream.end("Final line\n"); // signals no more writes are coming
```

### Respecting backpressure

```js
const canContinue = stream.write(chunk);
if (!canContinue) {
  // the internal buffer is full — wait for "drain" before writing more
  await new Promise((resolve) => stream.once("drain", resolve));
}
```

**Backpressure** is what keeps a fast data source from overwhelming a slow destination — `write()` returns `false` when the writable side's internal buffer is full, signaling "slow down." `pipe()` (below) handles this automatically, which is a big part of why it's preferred over manually shuttling `data` events into `write()` calls yourself.

---

## `.pipe()` — connecting streams together

```js
import { createReadStream, createWriteStream } from "node:fs";

createReadStream("input.txt").pipe(createWriteStream("output.txt"));
```

Reads from the source and writes to the destination automatically, chunk by chunk, handling backpressure for you. Streams can be chained:

```js
import zlib from "node:zlib";

createReadStream("file.txt")
  .pipe(zlib.createGzip())
  .pipe(createWriteStream("file.txt.gz"));
```

Read → compress → write, entirely chunk-by-chunk, without ever holding the whole file in memory.

---

## `pipeline()` — the recommended way to connect streams

```js
import { pipeline } from "node:stream/promises";

await pipeline(
  createReadStream("file.txt"),
  zlib.createGzip(),
  createWriteStream("file.txt.gz"),
);
```

Does the same job as chained `.pipe()` calls, but with proper error propagation and automatic cleanup if any stream in the chain fails — with plain `.pipe()`, an error partway through the chain can leave some streams open and leaking resources. `pipeline` is the modern, recommended approach for connecting more than one stream.

---

## `Transform` streams — modifying data in-flight

```js
import { Transform } from "node:stream";

const upperCaseTransform = new Transform({
  transform(chunk, encoding, callback) {
    this.push(chunk.toString().toUpperCase());
    callback();
  },
});

createReadStream("file.txt").pipe(upperCaseTransform).pipe(process.stdout);
```

A `Transform` stream sits in the middle of a pipeline, taking input and producing (usually different) output — `zlib.createGzip()` and a CSV parser are both examples of this pattern.

---

## Where you've already seen streams

- `req` in an HTTP server is a **readable** stream (`03-http.md`) — reading a request body means consuming it like any other readable stream
- `res` is a **writable** stream — `res.write()`/`res.end()` are exactly the writable stream API
- `fs.createReadStream`/`createWriteStream` (`01-fs.md`) — the standard way to handle large files
- File uploads (`file-upload/multer.md`) are typically processed as streams under the hood, rather than loaded fully into memory

## Common mistakes

- **Loading a large file with `readFile` instead of streaming it** — works until the file is big enough to cause memory pressure or long blocking pauses.
- **Manually forwarding `data` events into `write()` calls** instead of using `pipe()`/`pipeline()` — easy to get backpressure handling wrong, causing unbounded memory growth if the destination is slower than the source.
- **Not handling the `"error"` event** on a stream — per `04-events.md`, an unhandled error event crashes the process.
- **Using `.pipe()` chains without cleanup handling** — prefer `pipeline()` so a failure partway through doesn't leave dangling open streams.

## Quick summary

- Streams process data in chunks, using constant memory regardless of the total data size
- Four types: Readable (out), Writable (in), Duplex (both), Transform (both, with modification in between)
- `.pipe()` connects streams and handles backpressure automatically; `pipeline()` does the same with better error handling and cleanup — prefer it for anything beyond a single simple pipe
- HTTP requests/responses and file I/O are streams under the hood — this is why frameworks and libraries you already use behave the way they do

## Next

**`06-buffer.md`** covers `Buffer` — the raw binary data type streams and file I/O actually move around under the hood.
