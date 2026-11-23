# Modern Browser Web APIs

> Modern Web APIs provide standardized browser interfaces to underlying hardware, OS capabilities, peripheral subsystems, and device lifecycles without requiring native platform wrappers.

---

## Metadata

| Attribute | Details |
| :--- | :--- |
| **Level** | Intermediate to Advanced |
| **Prerequisites** | [The DOM](./01_dom.md), [Promises](../11_asynchronous-javascript/03_promises.md) |
| **Time to Read** | 24 minutes |
| **Status** | Complete |
| **Standard** | W3C / WHATWG Web Application APIs / Baseline Widely Available |

---

## Table of Contents

1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & API Reference](#4-syntax--api-reference)
5. [Practical Examples](#5-practical-examples)
6. [Edge Cases, Pitfalls & Gotchas](#6-edge-cases-pitfalls--gotchas)
7. [Common Mistakes & Anti-Patterns](#7-common-mistakes--anti-patterns)
8. [Performance Characteristics](#8-performance-characteristics)
9. [Engine Internals](#9-engine-internals)
10. [Best Practices & Design Guidelines](#10-best-practices--design-guidelines)
11. [Real-World Case Study](#11-real-world-case-study)
12. [Interview Questions](#12-interview-questions)
13. [Exercises & Challenges](#13-exercises--challenges)

---

## 1. What & Why

Historically, web browsers were restricted to rendering static hyperlinked documents. Accessing native device capabilities—such as hardware GPS coordinates, system clipboard buffers, operating system notifications, and battery status—required packaging web applications inside hybrid native wrappers (like Cordova, PhoneGap, or Electron).

Today, the modern web platform exposes direct, standardized **Browser Web APIs**. Governed by strict permission models and transient user activation checks, these APIs enable web applications to deliver native-grade capabilities directly in evergreen browsers across mobile and desktop devices.

---

## 2. Core Concepts

### Transient User Activation (User Gesture Requirement)
To prevent malicious websites from spamming users with dialogs, stealing clipboard contents, or auto-playing media, the browser enforces **Transient User Activation**:
- APIs like `navigator.clipboard.writeText()`, `element.requestFullscreen()`, and `navigator.share()` **must be triggered directly inside a user interaction handler** (e.g. `click` or `keydown`).
- Invoking these APIs outside a direct user gesture throws a `NotAllowedError` DOMException.

### The Permissions API
Modern hardware APIs integrate with the unified **Permissions API** (`navigator.permissions.query()`):
- States: `'granted'`, `'denied'`, `'prompt'`.
- Allows applications to inspect permission status before triggering invasive browser permission dialogs.

```text
                  navigator.permissions.query({ name: 'geolocation' })
                                         │
                   ┌─────────────────────┼─────────────────────┐
                   ▼                     ▼                     ▼
              'granted'               'prompt'              'denied'
        Access hardware directly   Show permission UI    Gracefully fallback
```

---

## 3. How It Works Internally

### Browser Sandbox & IPC Security Boundaries

Web page JavaScript runs in an unprivileged **Renderer Process**. Hardware access requires cross-process communication:

```
 [ Renderer Process (V8 JS Engine) ]
   User clicks button: navigator.geolocation.getCurrentPosition(...)
                    │
                    ▼
 [ Mojo IPC Channel (Chromium) ]
   Serializes request across sandbox boundary
                    │
                    ▼
 [ Browser / Host Process (OS Level) ]
   - Checks origin permissions and site settings
   - Queries OS location services (CoreLocation / Windows Location API / Android GPS)
   - Receives GPS coordinates: { lat, lng, accuracy }
                    │
                    ▼
 [ Mojo IPC Channel ]
   Dispatches response back to Renderer Process
                    │
                    ▼
 [ JavaScript Callback / Promise Settles ]
```

---

## 4. Syntax & API Reference

### 1. Page Visibility API
```javascript
// Check current visibility: 'visible' | 'hidden'
console.log('Visibility:', document.visibilityState);

// React to tab switching or minimizing
document.addEventListener('visibilitychange', () => {
  if (document.hidden) {
    pauseVideoPlayback();
    haltWebSocketHeartbeats();
  } else {
    resumeVideoPlayback();
  }
});
```

### 2. Clipboard API
```javascript
// Write plain text to system clipboard (Requires user gesture)
await navigator.clipboard.writeText('Copied text!');

// Read plain text from system clipboard (Requires permission)
const text = await navigator.clipboard.readText();
```

### 3. Geolocation API
```javascript
navigator.geolocation.getCurrentPosition(
  (position) => {
    const { latitude, longitude, accuracy } = position.coords;
    console.log(`Lat: ${latitude}, Lng: ${longitude} (Accuracy: ±${accuracy}m)`);
  },
  (error) => console.error('Location error:', error.message),
  { enableHighAccuracy: true, timeout: 5000, maximumAge: 0 }
);
```

### 4. Fullscreen API
```javascript
// Enter fullscreen
await videoElement.requestFullscreen();

// Exit fullscreen
await document.exitFullscreen();

// Check state
console.log('Is Fullscreen:', !!document.fullscreenElement);
```

### 5. Web Share API
```javascript
if (navigator.share) {
  await navigator.share({
    title: 'Modern JavaScript Notes',
    text: 'Master advanced browser APIs',
    url: 'https://example.com/notes',
  });
}
```

---

## 5. Practical Examples

### Example 1: Resource-Preserving Background Tab Throttler
```javascript
class VideoPlayerManager {
  #video;
  #wasPlaying = false;

  constructor(videoElement) {
    this.#video = videoElement;
    this.#attachVisibilityListener();
  }

  #attachVisibilityListener() {
    document.addEventListener('visibilitychange', () => {
      if (document.hidden) {
        if (!this.#video.paused) {
          this.#wasPlaying = true;
          this.#video.pause();
          console.log('Tab hidden: video paused to save CPU/battery');
        }
      } else {
        if (this.#wasPlaying) {
          this.#video.play().catch(() => {});
          this.#wasPlaying = false;
          console.log('Tab visible: video resumed');
        }
      }
    });
  }
}
```

### Example 2: Robust Copy-to-Clipboard Button with Visual Feedback
```javascript
function attachCopyButton(buttonElement, targetTextGetter) {
  buttonElement.addEventListener('click', async () => {
    const textToCopy = targetTextGetter();

    try {
      await navigator.clipboard.writeText(textToCopy);
      buttonElement.textContent = 'Copied!';
      buttonElement.classList.add('btn-success');
    } catch (err) {
      console.warn('Modern clipboard failed, using fallback:', err);
      fallbackCopy(textToCopy);
    } finally {
      setTimeout(() => {
        buttonElement.textContent = 'Copy';
        buttonElement.classList.remove('btn-success');
      }, 2000);
    }
  });
}

function fallbackCopy(text) {
  const textArea = document.createElement('textarea');
  textArea.value = text;
  textArea.style.position = 'fixed';
  textArea.style.left = '-9999px';
  document.body.appendChild(textArea);
  textArea.select();
  document.execCommand('copy');
  textArea.remove();
}
```

### Example 3: Geolocation Permission Gatekeeper
```javascript
async function requestUserCoordinates() {
  if (!navigator.permissions || !navigator.geolocation) {
    throw new Error('Geolocation unsupported');
  }

  const permission = await navigator.permissions.query({ name: 'geolocation' });

  if (permission.state === 'denied') {
    throw new Error('Location access is blocked. Please enable permissions in your browser settings.');
  }

  return new Promise((resolve, reject) => {
    navigator.geolocation.getCurrentPosition(
      (pos) => resolve(pos.coords),
      (err) => reject(err),
      { timeout: 8000, enableHighAccuracy: true }
    );
  });
}
```

### Example 4: Native Desktop Notifications with Permission Check
```javascript
async function notifyUser(title, body) {
  if (!('Notification' in window)) return;

  let permission = Notification.permission;

  if (permission === 'default') {
    permission = await Notification.requestPermission();
  }

  if (permission === 'granted') {
    const notification = new Notification(title, {
      body,
      icon: '/icons/notification-icon.png',
      badge: '/icons/badge.png',
    });

    notification.addEventListener('click', () => {
      window.focus();
      notification.close();
    });
  }
}
```

### Example 5: Network Connectivity Monitor
```javascript
class NetworkMonitor {
  constructor(onStatusChange) {
    this.onStatusChange = onStatusChange;
    this.#init();
  }

  #init() {
    window.addEventListener('online', () => this.#update(true));
    window.addEventListener('offline', () => this.#update(false));

    // Optional: Network Information API
    if (navigator.connection) {
      navigator.connection.addEventListener('change', () => {
        console.log(`Connection type changed: ${navigator.connection.effectiveType}`);
      });
    }
  }

  #update(isOnline) {
    console.log(`Network status changed: ${isOnline ? 'ONLINE' : 'OFFLINE'}`);
    this.onStatusChange(isOnline);
  }

  get isOnline() {
    return navigator.onLine;
  }
}
```

---

## 6. Edge Cases, Pitfalls & Gotchas

1. **Transient Activation Expiration**: User activation tokens expire after a few seconds or upon consuming operations. Invoking `setTimeout` before calling `element.requestFullscreen()` or `navigator.share()` causes the user activation window to close, throwing `NotAllowedError`.
2. **Secure Contexts (HTTPS) Required**: Nearly all modern hardware APIs (`clipboard`, `geolocation`, `Notification`, `web-share`) **fail immediately in non-secure contexts (`http://`)**. They operate exclusively on `https://` or `http://localhost`.
3. **`navigator.onLine` False Positives**: `navigator.onLine === true` guarantees only that the machine is connected to a local area network (LAN) or router; it does **not** guarantee that the internet or DNS servers are reachable. Always verify connectivity with real HTTP fetch health checks.
4. **Clipboard Permissions in Unfocused Windows**: Calling `navigator.clipboard.readText()` throws `NotAllowedError` if the browser document window does not currently have active operating system focus.

---

## 7. Common Mistakes & Anti-Patterns

| Mistake | Incorrect Pattern | Correct Solution | Why It Fails |
| :--- | :--- | :--- | :--- |
| **Async Activation Loss** | `setTimeout(() => el.requestFullscreen(), 1000)` | Request fullscreen directly in event handler | User gesture token expires; browser blocks call. |
| **Relying Solely on `navigator.onLine`**| Assuming `onLine === true` means backend is up | Verify with real fetch ping | Connected to Wi-Fi without internet still returns true. |
| **Hardcoding Non-HTTPS URLs** | Testing Geolocation over plain HTTP IP | Use HTTPS or localhost | Web APIs are blocked on non-secure origins. |
| **Missing Notification Permission Check**| Calling `new Notification()` without check | Await `Notification.requestPermission()` | Throws error or fails silently if permission is default/denied. |
| **Assuming Web Share Exists Everywhere**| Calling `navigator.share()` directly | Check `if (navigator.share)` first | Unsupported on many desktop browsers. |
| **Unrevoked Clipboard Listeners** | Leaving persistent paste event listeners | Clean up listeners on teardown | Inadvertently captures user sensitive clipboard contents. |

---

## 8. Performance Characteristics

- **Zero Polling with Page Visibility**: Pausing animations, timers, and WebSockets when `document.hidden === true` reduces idle tab CPU consumption to nearly 0%, preserving mobile battery life.
- **Hardware GPS Power Consumption**: Setting `enableHighAccuracy: true` in `navigator.geolocation` powers up the device's hardware satellite GPS chip, consuming substantial battery. Use coarse cellular/Wi-Fi positioning (`enableHighAccuracy: false`) unless turn-by-turn navigation is required.

---

## 9. Engine Internals

1. **Chromium UserActivation State**: Blink tracks user gestures using the `blink::UserActivation` class, checking `hasBeenActive()` and `isActive()` flags before executing privileged API calls.
2. **Permissions Service IPC**: In Chromium, `navigator.permissions.query()` communicates with the browser's profile database using Mojo IPC, caching permission states per origin in the host process.

---

## 10. Best Practices & Design Guidelines

1. **Always Feature-Detect Before Invoking**: Wrap calls in `if ('share' in navigator)` and provide UI fallbacks.
2. **Respect Tab Visibility**: Pause non-essential network polling and canvas rendering when `document.hidden` is true.
3. **Trigger Privileged APIs Synchronously in User Gestures**: Never place `requestFullscreen()` or `clipboard.writeText()` after asynchronous delay steps.
4. **Provide Context Before Requesting Permissions**: Explain to users why location or notification access is needed *before* triggering the browser's native permission prompt.

---

## 11. Real-World Case Study

### Battery-Conscious Turn-by-Turn Navigation PWA

A delivery driver PWA provides real-time route guidance while driving, automatically dimming the screen, pausing geolocation polling when the tab is hidden, and waking the screen using the Screen Wake Lock API.

```javascript
class DeliveryNavigator {
  #watchId = null;
  #wakeLock = null;

  async startNavigation(onLocationUpdate) {
    // 1. Request Screen Wake Lock (keeps screen awake during driving)
    await this.#requestWakeLock();

    // 2. Start high-accuracy location tracking
    this.#watchId = navigator.geolocation.watchPosition(
      (pos) => onLocationUpdate(pos.coords),
      (err) => console.error('GPS error:', err.message),
      { enableHighAccuracy: true, maximumAge: 1000 }
    );

    // 3. Pause GPS when driver locks phone or switches apps to preserve battery
    document.addEventListener('visibilitychange', () => {
      if (document.hidden) {
        if (this.#watchId !== null) {
          navigator.geolocation.clearWatch(this.#watchId);
          this.#watchId = null;
          console.log('App hidden: GPS paused');
        }
      } else {
        if (this.#watchId === null) {
          this.startNavigation(onLocationUpdate);
          console.log('App visible: GPS resumed');
        }
      }
    });
  }

  async #requestWakeLock() {
    if ('wakeLock' in navigator) {
      try {
        this.#wakeLock = await navigator.wakeLock.request('screen');
        this.#wakeLock.addEventListener('release', () => {
          console.log('Wake Lock was released');
        });
      } catch (err) {
        console.warn('Wake Lock error:', err.message);
      }
    }
  }

  stopNavigation() {
    if (this.#watchId !== null) {
      navigator.geolocation.clearWatch(this.#watchId);
      this.#watchId = null;
    }
    this.#wakeLock?.release();
  }
}
```

---

## 12. Interview Questions

<details>
<summary><strong>1. What is "Transient User Activation", and why is it enforced?</strong></summary>

Transient user activation is a temporary security state entered when a user interacts with the page (via a click, tap, or key press). Privileged APIs (like `requestFullscreen`, `clipboard.writeText`, or `navigator.share`) require active transient activation to prevent malicious sites from hijacking device capabilities or spamming users without their explicit consent.
</details>

<details>
<summary><strong>2. How does the Page Visibility API improve web application performance?</strong></summary>

It informs the application when a tab is hidden, minimized, or covered. Developers can pause CPU-intensive canvas animations, audio/video playback, and recurring WebSocket or HTTP polling, saving substantial CPU, memory, and battery.
</details>

<details>
<summary><strong>3. Why is <code>navigator.onLine</code> unreliable as a true indicator of internet connectivity?</strong></summary>

`navigator.onLine` only indicates whether the device has an active network interface or local connection (like an active Wi-Fi or Ethernet link). It returns `true` even if the local router has no internet access, the DNS server is down, or a captive portal is blocking traffic.
</details>

<details>
<summary><strong>4. Why must modern Web APIs be served over HTTPS?</strong></summary>

Hardware APIs expose sensitive user data (GPS coordinates, camera, microphone, clipboard). Serving over HTTPS (Secure Context) ensures that communication is encrypted in transit and guarantees site identity, preventing man-in-the-middle attacks from intercepting private user telemetry.
</details>

<details>
<summary><strong>5. What are the three states returned by <code>navigator.permissions.query()</code>?</strong></summary>

- `'granted'`: Permission has been given; the API can be invoked without prompting.
- `'denied'`: Permission was explicitly refused; invoking the API will fail immediately.
- `'prompt'`: The user will be shown a browser permission dialog when the API is invoked.
</details>

<details>
<summary><strong>6. How does <code>navigator.clipboard.writeText()</code> differ from the legacy <code>document.execCommand('copy')</code>?</strong></summary>

`navigator.clipboard.writeText()` is an asynchronous, Promise-based API that writes directly to the system clipboard without requiring hidden `<textarea>` DOM elements, selection ranges, or synchronous UI thread blocking.
</details>

<details>
<summary><strong>7. What does the Screen Wake Lock API do?</strong></summary>

It prevents the device display from dimming or locking automatically (e.g. while reading a recipe, watching a presentation, or using GPS navigation).
</details>

<details>
<summary><strong>8. What is the difference between <code>getCurrentPosition()</code> and <code>watchPosition()</code> in the Geolocation API?</strong></summary>

`getCurrentPosition()` retrieves the device's current location once and completes. `watchPosition()` registers an ongoing handler that automatically executes whenever the device's physical position changes, returning a numeric ID used to stop tracking via `clearWatch()`.
</details>

---

## 13. Exercises & Challenges

### Challenge 1: Implement an Unsaved Changes Tab Lock
Use the `beforeunload` event and the Page Visibility API to notify users if they attempt to navigate away or close a browser tab containing unsaved form changes.

<details>
<summary>View Solution</summary>

```javascript
function protectUnsavedChanges(hasUnsavedChangesGetter) {
  window.addEventListener('beforeunload', (event) => {
    if (hasUnsavedChangesGetter()) {
      // Standard browser confirmation prompt
      event.preventDefault();
      event.returnValue = ''; // Standard trigger for modern browsers
      return '';
    }
  });
}
```
</details>

### Challenge 2: Native Share with Fallback Copy
Write a utility function `shareOrCopy(shareData)` that uses `navigator.share()` if supported, falling back to copying the URL to the clipboard via `navigator.clipboard.writeText()` if sharing is unsupported or declined.

<details>
<summary>View Solution</summary>

```javascript
async function shareOrCopy(shareData) {
  if (navigator.share && navigator.canShare?.(shareData)) {
    try {
      await navigator.share(shareData);
      return { status: 'shared' };
    } catch (err) {
      if (err.name === 'AbortError') {
        return { status: 'cancelled' };
      }
      // Fall through to clipboard fallback on failure
    }
  }

  // Fallback: Copy URL to clipboard
  try {
    await navigator.clipboard.writeText(shareData.url || window.location.href);
    return { status: 'copied' };
  } catch (err) {
    throw new Error('Could not share or copy link');
  }
}
```
</details>
