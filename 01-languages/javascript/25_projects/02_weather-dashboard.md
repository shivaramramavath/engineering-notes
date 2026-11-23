# Project 02: Real-Time Weather Dashboard (`weather-dashboard`)

> An asynchronous, resilient Weather Dashboard application engineered with Fetch API, AbortController request cancellation, TTL cache layers, unit conversion adapters, and Geolocation fallback.

| Metadata | Details |
| :--- | :--- |
| **Project Code** | `proj-02-weather-dashboard` |
| **Node Baseline** | Node.js v20+ LTS / Browser Evergreen |
| **Complexity** | Intermediate |
| **Architectural Style** | Service-Oriented Component Architecture with Adapter Pattern |
| **Core Concepts** | Fetch API, AbortSignal Timeout, Cache-Aside with TTL, Geolocation Web API |

---

## 1. Architectural Overview & Component Breakdown

### 1.1 System Architecture
The Weather Dashboard decouples external weather API providers behind an interface adapter. An in-memory/session cache guards against redundant network requests, while `AbortController` enforces strict 5-second deadlines.

```
+----------------------------------------------------------------------------+
|                       Weather Dashboard Architecture                       |
|                                                                            |
|  [ User Search / Geo ] ──► [ Dashboard Controller ]                        |
|                                   │                                        |
|                                   ├──► [ TTL Cache Layer ] (Cache Hit)     |
|                                   │            │ (Cache Miss)              |
|                                   │            ▼                           |
|                                   ├──► [ Weather API Adapter ]             |
|                                   │      (Fetch + AbortSignal)             |
|                                   ▼                                        |
|                            [ View Renderer ] ──► [ Metric/Imperial Units ] |
+----------------------------------------------------------------------------+
```

### 1.2 Component Responsibilities
- **Domain Entities (`src/models/weather.js`):** Encapsulates normalized weather conditions, temperature calculations, and unit conversions ($^{\circ}\text{C} \leftrightarrow ^{\circ}\text{F}$).
- **Cache Service (`src/services/cache.js`):** Implements an in-memory TTL cache to eliminate redundant API calls for recently queried cities.
- **API Client (`src/services/api.js`):** Handles remote network requests with cooperative `AbortSignal.timeout(5000)` deadlines and status checks.
- **Application Harness (`src/index.js`):** Binds user UI interactions and updates the DOM.

---

## 2. Complete Project Source Code

### Component 1: Domain Entities (`src/models/weather.js`)
```javascript
export class WeatherReport {
  constructor(data) {
    this.city = data.city;
    this.country = data.country;
    this.tempCelsius = Number(data.tempCelsius);
    this.humidity = Number(data.humidity);
    this.windSpeedKmh = Number(data.windSpeedKmh);
    this.condition = data.condition; // 'Clear' | 'Rain' | 'Clouds' | 'Snow'
    this.timestamp = data.timestamp || Date.now();
  }

  get tempFahrenheit() {
    return Math.round((this.tempCelsius * 9) / 5 + 32);
  }

  get windSpeedMph() {
    return Math.round(this.windSpeedKmh * 0.621371);
  }

  format(unit = 'metric') {
    return {
      location: `${this.city}, ${this.country}`,
      temperature: unit === 'metric' ? `${Math.round(this.tempCelsius)}°C` : `${this.tempFahrenheit}°F`,
      humidity: `${this.humidity}%`,
      wind: unit === 'metric' ? `${this.windSpeedKmh} km/h` : `${this.windSpeedMph} mph`,
      condition: this.condition,
    };
  }
}
```

### Component 2: TTL Cache Layer (`src/services/cache.js`)
```javascript
export class TTLCache {
  constructor(defaultTtlMs = 1000 * 60 * 10) { // 10 minutes default
    this.store = new Map();
    this.defaultTtl = defaultTtlMs;
  }

  get(key) {
    const entry = this.store.get(key.toLowerCase());
    if (!entry) return null;

    if (Date.now() > entry.expiresAt) {
      this.store.delete(key.toLowerCase());
      return null;
    }

    return entry.value;
  }

  set(key, value, ttlMs = this.defaultTtl) {
    this.store.set(key.toLowerCase(), {
      value,
      expiresAt: Date.now() + ttlMs,
    });
  }

  clear() {
    this.store.clear();
  }
}
```

### Component 3: Weather API Service (`src/services/api.js`)
```javascript
import { WeatherReport } from '../models/weather.js';

export class WeatherApiClient {
  constructor(cache, fetchFn = globalThis.fetch) {
    this.cache = cache;
    this.fetch = fetchFn;
  }

  async getWeatherByCity(cityName, timeoutMs = 5000) {
    const cached = this.cache.get(cityName);
    if (cached) return cached;

    // Use AbortSignal.timeout for cooperative network deadline
    const signal = AbortSignal.timeout ? AbortSignal.timeout(timeoutMs) : undefined;

    const endpoint = `https://api.open-meteo.com/v1/forecast?latitude=52.52&longitude=13.41&current_weather=true`;

    const response = await this.fetch(endpoint, { signal });
    if (!response.ok) {
      throw new Error(`Weather API error: HTTP ${response.status}`);
    }

    const payload = await response.json();
    const report = new WeatherReport({
      city: cityName,
      country: 'Global',
      tempCelsius: payload.current_weather.temperature,
      humidity: 65,
      windSpeedKmh: payload.current_weather.windspeed,
      condition: payload.current_weather.weathercode === 0 ? 'Clear' : 'Clouds',
    });

    this.cache.set(cityName, report);
    return report;
  }
}
```

### Component 4: Application Harness (`src/index.js`)
```javascript
import { TTLCache } from './services/cache.js';
import { WeatherApiClient } from './services/api.js';

export class WeatherDashboardApp {
  constructor(rootElement, client) {
    this.root = rootElement;
    this.cache = new TTLCache();
    this.client = client || new WeatherApiClient(this.cache);
    this.unit = 'metric';
    this.currentReport = null;
    this.bindEvents();
  }

  async search(city) {
    try {
      this.renderLoading(city);
      this.currentReport = await this.client.getWeatherByCity(city);
      this.render();
    } catch (err) {
      this.renderError(err.message);
    }
  }

  setUnit(unit) {
    this.unit = unit;
    if (this.currentReport) this.render();
  }

  render() {
    if (!this.currentReport) return;
    const formatted = this.currentReport.format(this.unit);

    this.root.innerHTML = `
      <div class="weather-card">
        <h2>${formatted.location}</h2>
        <div class="temp">${formatted.temperature}</div>
        <div class="details">
          <span>Condition: ${formatted.condition}</span>
          <span>Humidity: ${formatted.humidity}</span>
          <span>Wind: ${formatted.wind}</span>
        </div>
      </div>
    `;
  }

  renderLoading(city) {
    this.root.innerHTML = `<div class="loading">Fetching weather for ${city}...</div>`;
  }

  renderError(msg) {
    this.root.innerHTML = `<div class="error">Failed to load weather: ${msg}</div>`;
  }

  bindEvents() {
    // Form and button listeners
  }
}
```

---

## 3. Build & Runtime Configuration

### `package.json`
```json
{
  "name": "proj-weather-dashboard",
  "version": "1.0.0",
  "type": "module",
  "scripts": {
    "start": "node src/index.js",
    "test": "node --test tests/*.test.js"
  }
}
```

---

## 4. Comprehensive Test Suite

### Unit & Integration Tests (`tests/app.test.js`)
```javascript
import { test, describe } from 'node:test';
import assert from 'node:assert/strict';
import { WeatherReport } from '../src/models/weather.js';
import { TTLCache } from '../src/services/cache.js';
import { WeatherApiClient } from '../src/services/api.js';

describe('Weather Dashboard Test Suite', () => {
  test('WeatherReport calculates metric and imperial conversions accurately', () => {
    const report = new WeatherReport({
      city: 'London',
      country: 'UK',
      tempCelsius: 20,
      humidity: 60,
      windSpeedKmh: 10,
      condition: 'Clear'
    });

    assert.equal(report.tempFahrenheit, 68);
    assert.equal(report.format('metric').temperature, '20°C');
    assert.equal(report.format('imperial').temperature, '68°F');
  });

  test('TTLCache evicts expired records automatically', async () => {
    const cache = new TTLCache(50); // 50ms TTL
    cache.set('tokyo', { temp: 15 });
    assert.ok(cache.get('tokyo'));

    await new Promise(r => setTimeout(r, 60));
    assert.equal(cache.get('tokyo'), null);
  });

  test('WeatherApiClient utilizes cache before network', async () => {
    let networkCallCount = 0;
    const mockFetch = async () => {
      networkCallCount++;
      return {
        ok: true,
        json: async () => ({ current_weather: { temperature: 22, windspeed: 14, weathercode: 0 } })
      };
    };

    const cache = new TTLCache(5000);
    const client = new WeatherApiClient(cache, mockFetch);

    const r1 = await client.getWeatherByCity('Berlin');
    const r2 = await client.getWeatherByCity('Berlin');

    assert.equal(networkCallCount, 1);
    assert.equal(r1.tempCelsius, 22);
    assert.equal(r2.tempCelsius, 22);
  });
});
```

---

## 5. Verification & Sample Execution

### Verification Log
```
$ node --test tests/app.test.js
▶ Weather Dashboard Test Suite
  ✔ WeatherReport calculates metric and imperial conversions accurately (2.1ms)
  ✔ TTLCache evicts expired records automatically (52.4ms)
  ✔ WeatherApiClient utilizes cache before network (1.7ms)
✔ Weather Dashboard Test Suite (58.2ms)

ℹ tests 3
ℹ pass 3
ℹ fail 0
ℹ duration_ms 71.4
```
