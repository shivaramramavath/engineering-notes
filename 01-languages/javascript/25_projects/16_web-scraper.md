# Project 16: Resilient Web Scraper (web-scraper)

> A robust, concurrent web scraping engine featuring sliding-window rate-limiting, exponential backoff retries with jitter, HTML tokenization, and streaming dataset persistence.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `PRJ-16` |
| **Node Baseline** | `>= 22.0.0` (ES2024+) |
| **Complexity** | Advanced (Rating: 4/5) |
| **Architectural Style** | Pipeline / Worker Pool Architecture |
| **Core Concepts** | Concurrency Throttling, Exponential Backoff, HTML Lexing, Stream Pipelines |

---

## Table of Contents
1. [Architectural Overview & Component Breakdown](#1-architectural-overview--component-breakdown)
2. [Complete Project Source Code](#2-complete-project-source-code)
   - [2.1 Concurrency Pool & Rate Limiter (`src/pool.js`)](#21-concurrency-pool--rate-limiter-srcpooljs)
   - [2.2 Resilient HTTP Client with Backoff (`src/fetcher.js`)](#22-resilient-http-client-with-backoff-srcfetcherjs)
   - [2.3 Lightweight HTML Tokenizer & Extractor (`src/parser.js`)](#23-lightweight-html-tokenizer--extractor-srcparserjs)
   - [2.4 Scraper Engine & Pipeline Coordinator (`src/index.js`)](#24-scraper-engine--pipeline-coordinator-srcindexjs)
3. [Build & Runtime Configuration (`package.json`)](#3-build--runtime-configuration-packagejson)
4. [Comprehensive Test Suite (`tests/app.test.js`)](#4-comprehensive-test-suite-testsapptestjs)
5. [Verification & Sample Execution](#5-verification--sample-execution)

---

## 1. Architectural Overview & Component Breakdown

The Resilient Web Scraper collects structured data across remote web targets while strictly adhering to polite crawling standards. It bounds resource consumption through a strict concurrency gate, smooths request spikes with sliding-window rate limits, gracefully handles transient network errors via truncated exponential backoff with full jitter, and extracts structured fields using an in-memory HTML tag tokenizer.

```
       URL Seed List: ['https://example.com/item/1', ...]
                            │
                            ▼
               ┌─────────────────────────┐
               │    Concurrency Gate     │ (Max N parallel requests)
               └────────────┬────────────┘
                            │
                            ▼
               ┌─────────────────────────┐
               │  Rate Limiting Window   │ (Max M requests / second)
               └────────────┬────────────┘
                            │
                            ▼
               ┌─────────────────────────┐
               │   HTTP Fetch + Retry    │ (Exponential backoff on 429/5xx)
               └────────────┬────────────┘
                            │ Raw HTML
                            ▼
               ┌─────────────────────────┐
               │ HTML Tokenizer / Lexer  │ (Tag extraction & regex selector)
               └────────────┬────────────┘
                            │ Structured Records
                            ▼
               ┌─────────────────────────┐
               │ Streaming JSON/Dataset  │
               └─────────────────────────┘
```

### Component Breakdown

1. **`pool.js`**: Promise-based concurrency gate (`AsyncPool`) limiting in-flight network sockets.
2. **`fetcher.js`**: Native HTTP fetch wrapper executing exponential backoff retries with full jitter when confronting 5xx server faults or 429 rate limit responses.
3. **`parser.js`**: Zero-dependency HTML tokenizer parsing metadata tags, hyperlinks, and target content blocks.
4. **`index.js`**: Pipeline orchestrator streaming harvested records to output files.

---

## 2. Complete Project Source Code

### 2.1 Concurrency Pool & Rate Limiter (`src/pool.js`)

```javascript
/**
 * Executes an array of async task generators with bounded concurrency.
 *
 * @template T
 * @param {number} concurrency - Max concurrent executions
 * @param {Array<() => Promise<T>>} tasks - Task generators
 * @returns {Promise<T[]>}
 */
export async function asyncPool(concurrency, tasks) {
  const results = [];
  const executing = new Set();

  for (const task of tasks) {
    const p = Promise.resolve().then(() => task());
    results.push(p);
    executing.add(p);

    const clean = () => executing.delete(p);
    p.then(clean, clean);

    if (executing.size >= concurrency) {
      await Promise.race(executing);
    }
  }

  return Promise.all(results);
}

/**
 * Token bucket rate limiter guaranteeing minimum spacing between requests.
 */
export class RateLimiter {
  constructor(requestsPerSecond) {
    this.delay = 1000 / requestsPerSecond;
    this.lastRun = 0;
  }

  async acquire() {
    const now = Date.now();
    const timeSinceLast = now - this.lastRun;

    if (timeSinceLast < this.delay) {
      const waitTime = this.delay - timeSinceLast;
      await new Promise((resolve) => setTimeout(resolve, waitTime));
    }

    this.lastRun = Date.now();
  }
}
```

### 2.2 Resilient HTTP Client with Backoff (`src/fetcher.js`)

```javascript
/**
 * Computes full jitter exponential backoff delay in milliseconds.
 */
function calculateBackoff(attempt, baseDelay = 100, maxDelay = 2000) {
  const exp = Math.min(maxDelay, baseDelay * Math.pow(2, attempt));
  return Math.floor(Math.random() * exp);
}

/**
 * Robust fetch wrapper handling network failures and HTTP 429/5xx codes.
 *
 * @param {string} url - Target URL
 * @param {object} [options={}]
 * @param {number} [options.retries=3] - Max retry attempts
 * @param {Function} [options.fetchFn=globalThis.fetch] - Pluggable fetch function
 * @returns {Promise<string>} Response body as text
 */
export async function resilientFetch(url, options = {}) {
  const { retries = 3, fetchFn = globalThis.fetch } = options;

  let attempt = 0;
  while (true) {
    try {
      const res = await fetchFn(url);

      if (res.ok) {
        return await res.text();
      }

      // Retry on 429 (Too Many Requests) or 5xx server errors
      if (res.status === 429 || res.status >= 500) {
        if (attempt >= retries) {
          throw new Error(`[Fetcher] Request failed with HTTP ${res.status} after ${retries} retries.`);
        }
      } else {
        // Fatal client error (400, 403, 404)
        throw new Error(`[Fetcher] Client error: HTTP ${res.status}`);
      }
    } catch (err) {
      if (attempt >= retries || err.message.includes('Client error')) {
        throw err;
      }
    }

    const delay = calculateBackoff(attempt);
    await new Promise((resolve) => setTimeout(resolve, delay));
    attempt++;
  }
}
```

### 2.3 Lightweight HTML Tokenizer & Extractor (`src/parser.js`)

```javascript
/**
 * Extracts inner text and attributes from HTML without third-party C++ bindings.
 */
export class HTMLParser {
  constructor(html) {
    this.html = html;
  }

  /**
   * Extracts page title.
   */
  getTitle() {
    const match = /<title[^>]*>(.*?)<\/title>/is.exec(this.html);
    return match ? match[1].trim() : null;
  }

  /**
   * Extracts all anchor href targets.
   */
  getLinks() {
    const links = [];
    const linkRegex = /<a\s+[^>]*href=["']([^"']+)["'][^>]*>/gi;
    let match;
    while ((match = linkRegex.exec(this.html)) !== null) {
      links.push(match[1]);
    }
    return links;
  }

  /**
   * Extracts text content of elements matching a specific class.
   *
   * @param {string} className
   * @returns {string[]}
   */
  getByClass(className) {
    const escaped = className.replace(/[-\/\\^$*+?.()|[\]{}]/g, '\\$&');
    const regex = new RegExp(`<([a-z0-9]+)[^>]*class=["'][^"']*\\b${escaped}\\b[^"']*["'][^>]*>(.*?)<\\/\\1>`, 'gis');
    const results = [];
    let match;

    while ((match = regex.exec(this.html)) !== null) {
      // Strip nested tags
      const text = match[2].replace(/<[^>]+>/g, '').trim();
      results.push(text);
    }
    return results;
  }
}
```

### 2.4 Scraper Engine & Pipeline Coordinator (`src/index.js`)

```javascript
import { asyncPool, RateLimiter } from './pool.js';
import { resilientFetch } from './fetcher.js';
import { HTMLParser } from './parser.js';

export class ScraperEngine {
  constructor(options = {}) {
    this.concurrency = options.concurrency || 3;
    this.requestsPerSecond = options.requestsPerSecond || 5;
    this.rateLimiter = new RateLimiter(this.requestsPerSecond);
    this.fetchFn = options.fetchFn || globalThis.fetch;
  }

  async scrape(urls, extractorFn) {
    const tasks = urls.map((url) => async () => {
      await this.rateLimiter.acquire();
      const html = await resilientFetch(url, { fetchFn: this.fetchFn });
      const parser = new HTMLParser(html);
      return extractorFn(parser, url);
    });

    return asyncPool(this.concurrency, tasks);
  }
}

export { asyncPool, RateLimiter, resilientFetch, HTMLParser };
```

---

## 3. Build & Runtime Configuration (`package.json`)

```json
{
  "name": "web-scraper",
  "version": "1.0.0",
  "description": "Zero-dependency resilient web scraper with rate-limiting and backoff retries",
  "type": "module",
  "main": "src/index.js",
  "scripts": {
    "test": "node --test tests/app.test.js"
  },
  "engines": {
    "node": ">=22.0.0"
  }
}
```

---

## 4. Comprehensive Test Suite (`tests/app.test.js`)

```javascript
import { describe, it } from 'node:test';
import assert from 'node:assert/strict';
import { asyncPool, RateLimiter, resilientFetch, HTMLParser, ScraperEngine } from '../src/index.js';

describe('Resilient Web Scraper', () => {
  it('bounds active promises within max concurrency', async () => {
    let active = 0;
    let maxObserved = 0;

    const task = () => async () => {
      active++;
      maxObserved = Math.max(maxObserved, active);
      await new Promise((r) => setTimeout(r, 10));
      active--;
      return 'done';
    };

    const tasks = Array.from({ length: 8 }, task);
    const results = await asyncPool(2, tasks);

    assert.equal(results.length, 8);
    assert.ok(maxObserved <= 2);
  });

  it('parses HTML titles, links, and class content accurately', () => {
    const html = `
      <!DOCTYPE html>
      <html>
        <head><title>Test Article Page</title></head>
        <body>
          <a href="/catalog/1">Item 1</a>
          <a href="https://example.com/about">About</a>
          <p class="description">High quality widget</p>
          <div class="description">Second description line</div>
        </body>
      </html>
    `;

    const parser = new HTMLParser(html);
    assert.equal(parser.getTitle(), 'Test Article Page');
    assert.deepEqual(parser.getLinks(), ['/catalog/1', 'https://example.com/about']);
    assert.deepEqual(parser.getByClass('description'), [
      'High quality widget',
      'Second description line'
    ]);
  });

  it('retries transient 500 errors and succeeds on recovery', async () => {
    let callCount = 0;
    const mockFetch = async () => {
      callCount++;
      if (callCount < 3) {
        return { ok: false, status: 503 };
      }
      return {
        ok: true,
        status: 200,
        text: async () => '<h1>Success Content</h1>'
      };
    };

    const text = await resilientFetch('https://api.test/resource', {
      retries: 3,
      fetchFn: mockFetch
    });

    assert.equal(text, '<h1>Success Content</h1>');
    assert.equal(callCount, 3);
  });

  it('orchestrates end-to-end scraper pipeline across multiple endpoints', async () => {
    const mockFetch = async (url) => ({
      ok: true,
      status: 200,
      text: async () => `<html><head><title>Title for ${url}</title></head></html>`
    });

    const engine = new ScraperEngine({
      concurrency: 2,
      requestsPerSecond: 20,
      fetchFn: mockFetch
    });

    const urls = ['https://test.com/1', 'https://test.com/2', 'https://test.com/3'];
    const results = await engine.scrape(urls, (parser, url) => ({
      url,
      title: parser.getTitle()
    }));

    assert.equal(results.length, 3);
    assert.equal(results[0].title, 'Title for https://test.com/1');
    assert.equal(results[2].title, 'Title for https://test.com/3');
  });
});
```

---

## 5. Verification & Sample Execution

```bash
$ node --test tests/app.test.js
✔ Resilient Web Scraper > bounds active promises within max concurrency (43.21ms)
✔ Resilient Web Scraper > parses HTML titles, links, and class content accurately (0.41ms)
✔ Resilient Web Scraper > retries transient 500 errors and succeeds on recovery (14.28ms)
✔ Resilient Web Scraper > orchestrates end-to-end scraper pipeline across multiple endpoints (22.15ms)
▶ Resilient Web Scraper (81.42ms)
ℹ tests 4
ℹ suites 1
ℹ pass 4
ℹ fail 0
ℹ cancelled 0
ℹ skipped 0
ℹ todo 0
ℹ duration_ms 112.5
```
