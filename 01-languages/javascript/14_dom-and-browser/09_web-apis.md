# Web APIs

The browser exposes many **platform APIs** beyond the DOM: navigation, clipboard, location, notifications, sharing, permissions and more. Most are asynchronous, require a **secure context (HTTPS)**, and many require **user permission** or a **user gesture**.

## Using any Web API safely

1. **Feature-detect**, do not sniff user agents
2. Expect **permission prompts** and denials
3. Require **HTTPS** (`window.isSecureContext`)
4. Trigger sensitive APIs from a **user gesture** (click/keypress)
5. Provide a **fallback** or hide the feature when unsupported
6. Respect privacy: ask only when needed, explain why

```js
if ("clipboard" in navigator && window.isSecureContext) { /* use it */ }
```

## Permissions API

```js
const status = await navigator.permissions.query({ name: "geolocation" });
status.state;                              // "granted" | "denied" | "prompt"
status.addEventListener("change", () => update(status.state));
```

Names supported vary by browser (`geolocation`, `notifications`, `clipboard-read`, `camera`, `microphone`, ...). It tells you the state **without prompting**.

## History API and navigation (single-page apps)

```js
history.pushState({ page: 2 }, "", "/products?page=2");      // add an entry, no reload
history.replaceState({ page: 2 }, "", "/products?page=2");   // modify current entry
history.state;                                                // the state object
history.back(); history.forward(); history.go(-2);
history.length;
history.scrollRestoration = "manual";                         // control scroll restoring

window.addEventListener("popstate", (event) => {              // back/forward (NOT fired by pushState)
  render(location.pathname, event.state);
});
```

| Rule | Detail |
|------|--------|
| Same-origin URLs only | cross-origin throws `SecurityError` |
| State must be structured-cloneable | keep it small |
| Server must serve the same app for those URLs | otherwise refresh gives 404 |
| Hash routing (`#/path`) | works without server config, uses `hashchange` |

Minimal router:

```js
document.addEventListener("click", (e) => {
  const link = e.target.closest("a[data-link]");
  if (!link) return;
  e.preventDefault();
  history.pushState({}, "", link.href);
  render(location.pathname);
});
window.addEventListener("popstate", () => render(location.pathname));
```

The newer **Navigation API** (`navigation.navigate`, `navigate` event) improves SPA routing in supporting browsers (check support).

## Location and URLs

```js
location.href;  location.origin;  location.pathname;  location.search;  location.hash;
location.assign("/login");            // navigate (adds history entry)
location.replace("/login");           // navigate without a history entry
location.reload();

const url = new URL(location.href);
url.searchParams.get("q");
url.searchParams.set("page", "2");
history.replaceState(null, "", url);

new URLSearchParams({ a: "1", b: "x y" }).toString();    // "a=1&b=x+y"
```

Always build URLs with `URL`/`URLSearchParams` instead of string concatenation.

## Clipboard API

```js
// write text (requires secure context, usually a user gesture)
await navigator.clipboard.writeText("Copied!");

// read text (prompts for permission, focused document required)
const text = await navigator.clipboard.readText();

// rich content
await navigator.clipboard.write([
  new ClipboardItem({
    "text/plain": new Blob(["hello"], { type: "text/plain" }),
    "text/html": new Blob(["<b>hello</b>"], { type: "text/html" }),
  }),
]);
```

Events:

```js
document.addEventListener("copy", (e) => {
  e.clipboardData.setData("text/plain", "custom text");
  e.preventDefault();
});
document.addEventListener("paste", (e) => {
  const text = e.clipboardData.getData("text/plain");
  const files = [...e.clipboardData.files];                   // pasted images
});
```

Fallback (legacy): select text, then `document.execCommand("copy")` (deprecated).

```js
async function copy(text) {
  try { await navigator.clipboard.writeText(text); return true; }
  catch { return false; }                                      // denied or unsupported
}
```

## Geolocation

```js
if ("geolocation" in navigator) {
  navigator.geolocation.getCurrentPosition(
    (pos) => {
      const { latitude, longitude, accuracy } = pos.coords;     // accuracy in meters
    },
    (err) => {
      // err.code: 1 PERMISSION_DENIED, 2 POSITION_UNAVAILABLE, 3 TIMEOUT
      console.warn(err.message);
    },
    { enableHighAccuracy: false, timeout: 10_000, maximumAge: 60_000 },
  );

  const id = navigator.geolocation.watchPosition(onMove, onError);   // continuous updates
  navigator.geolocation.clearWatch(id);
}

const getPosition = (options) => new Promise((resolve, reject) => navigator.geolocation.getCurrentPosition(resolve, reject, options));
```

Privacy tips: ask **only after** the user takes an action that needs location, explain why, offer manual entry as a fallback, do not store precise location unnecessarily. `enableHighAccuracy` uses more battery.

## Notifications and Push

```js
if ("Notification" in window) {
  const permission = await Notification.requestPermission();      // "granted" | "denied" | "default"
  if (permission === "granted") {
    const n = new Notification("New message", { body: "Ada: Hello!", icon: "/icon.png", tag: "chat-1" });
    n.onclick = () => { window.focus(); n.close(); };
  }
}
```

For notifications while the page is closed, use a **service worker** and the **Push API**:

```js
const reg = await navigator.serviceWorker.register("/sw.js");
reg.showNotification("Hello", { body: "From the service worker" });

const sub = await reg.pushManager.subscribe({ userVisibleOnly: true, applicationServerKey: vapidPublicKey });
await fetch("/api/push/subscribe", { method: "POST", body: JSON.stringify(sub) });
```

Request permission **in response to a user action**, never on page load. Many browsers auto-block prompts from sites that ask too eagerly. iOS web push requires an installed PWA.

## Page Visibility, online status, lifecycle

```js
document.addEventListener("visibilitychange", () => {
  if (document.hidden) pauseVideo(); else resume();
});

window.addEventListener("online", syncQueue);
window.addEventListener("offline", showOfflineBanner);
navigator.onLine;                          // true only means "has a network interface", not "reaches the internet"

navigator.connection?.effectiveType;       // "4g", "3g" (Chromium; Network Information API)
```

## matchMedia and user preferences

```js
const dark = matchMedia("(prefers-color-scheme: dark)");
dark.matches;
dark.addEventListener("change", (e) => setTheme(e.matches ? "dark" : "light"));

matchMedia("(prefers-reduced-motion: reduce)").matches;     // disable heavy animations
matchMedia("(min-width: 768px)").matches;
matchMedia("(hover: none)").matches;                         // touch devices
```

## Web Share

```js
if (navigator.share) {
  await navigator.share({ title: "Article", text: "Read this", url: location.href });   // user gesture required
}
if (navigator.canShare?.({ files })) await navigator.share({ files });
```

## Fullscreen and Screen Wake Lock

```js
await element.requestFullscreen();
await document.exitFullscreen();
document.fullscreenElement;
document.addEventListener("fullscreenchange", onChange);

const lock = await navigator.wakeLock?.request("screen");    // keep the screen on (video, recipes)
lock?.release();
```

## Other useful APIs

| API | Purpose |
|-----|---------|
| `fetch`, `AbortController`, `WebSocket`, `EventSource` | networking (next chapter) |
| `Web Workers`, `Service Workers`, `SharedWorker` | background work, offline, push |
| `IntersectionObserver` etc. | see the observers file |
| `requestAnimationFrame`, `requestIdleCallback`, `scheduler` | scheduling |
| `Canvas`, `WebGL`, `WebGPU`, `Web Audio`, `MediaRecorder`, `getUserMedia` | graphics, audio, camera/mic |
| `Intl`, `Temporal` | formatting and time |
| `Web Crypto` (`crypto.subtle`, `crypto.randomUUID`) | cryptography |
| `WebAuthn` / Passkeys (`navigator.credentials`) | passwordless auth |
| `Payment Request`, `Credential Management` | checkout and sign-in helpers |
| `File System Access`, `Drag and Drop`, `FileReader` | files |
| `Web Speech`, `Vibration`, `Battery`, `Sensors`, `Bluetooth`, `USB`, `Serial` | device features (varying support, often Chromium only) |
| `BroadcastChannel`, `MessageChannel`, `postMessage` | messaging between tabs/frames/workers |
| `PerformanceObserver`, `Reporting API` | monitoring |

Always check **MDN compatibility tables** and [caniuse.com](https://caniuse.com) before depending on an API.

## cross-window messaging

```js
iframe.contentWindow.postMessage({ type: "hello" }, "https://trusted.example");

window.addEventListener("message", (event) => {
  if (event.origin !== "https://trusted.example") return;       // ALWAYS verify the origin
  handle(event.data);
});
```

Never use `"*"` as the target origin for sensitive data, and never trust `event.data` without validation.

## Progressive enhancement pattern

```js
async function share(data) {
  if (navigator.share) {
    try { return await navigator.share(data); }
    catch (err) { if (err.name === "AbortError") return; }       // user canceled
  }
  await copy(data.url);                                           // fallback
  toast("Link copied");
}
```

Design features so they **work without** the API, and improve when it is available.

## Pitfalls

| Pitfall | Why it hurts | Better |
|---------|--------------|--------|
| Prompting for permissions on page load | Users deny; browsers may auto-block | Ask after a relevant user action |
| Not handling denial | Broken UI | Fallbacks and clear messaging |
| Using APIs over HTTP | Unavailable in insecure contexts | HTTPS everywhere |
| `navigator.onLine` as proof of connectivity | False positives | Try the request and handle failure |
| User-agent sniffing | Fragile | Feature detection |
| Ignoring `popstate` when using `pushState` | Back button breaks | Handle `popstate` |
| `postMessage` without origin checks | Cross-site attacks | Verify `event.origin` and target origin |
| Storing precise location unnecessarily | Privacy risk | Keep minimal, short-lived |
| Calling Clipboard/Share outside a user gesture | Rejected | Call inside click/keypress handlers |
| Assuming Chromium-only APIs everywhere | Breaks in Safari/Firefox | Check support and degrade gracefully |

## Key takeaways

- Feature-detect, require HTTPS, and expect permission prompts and denials
- `history.pushState` plus `popstate` powers SPA routing; build URLs with `URL`
- Clipboard, Geolocation, Notifications and Share need user gestures or permissions: ask in context
- Verify origins in `postMessage`, and always provide fallbacks for unsupported APIs

**Next:** [Web Components](./10_web-components.md)