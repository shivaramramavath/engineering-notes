# Progressive Web App (PWA) Manifest, Icons, and iOS Integration

| Metadata | Details |
| :--- | :--- |
| **Specification** | W3C Web App Manifest / WHATWG HTML Living Standard |
| **Prerequisites** | `05-head-metadata-and-seo/02-meta-charset-viewport-and-color-scheme.md`, `01-fundamentals/01-document-structure.md` |
| **Estimated Reading Time** | 25 minutes |
| **Status** | Production Ready |
| **Baseline Status** | Widely Available (`<link rel="manifest">`, Web App Manifest JSON standard) |
| **Browsers Tested** | Chrome 124+, Firefox 125+, Safari 17.4+ (iOS & macOS), Edge 124+ |
| **Tested With** | Chrome DevTools Application Panel, Lighthouse PWA Audit, iOS Add to Home Screen |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
   - [Example 1: The Canonical Production Web App Manifest (`manifest.webmanifest`)](#example-1-the-canonical-production-web-app-manifest-manifestwebmanifest)
   - [Example 2: Complete HTML `<head>` Integration (PWA + Apple iOS Touch Icons)](#example-2-complete-html-head-integration-pwa--apple-ios-touch-icons)
   - [Example 3: Maskable Icons for Adaptive Android App Launchers](#example-3-maskable-icons-for-adaptive-android-app-launchers)
   - [Example 4: PWA App Shortcuts for Instant Actions](#example-4-pwa-app-shortcuts-for-instant-actions)
   - [Example 5: Intercepting the `beforeinstallprompt` Event in JavaScript](#example-5-intercepting-the-beforeinstallprompt-event-in-javascript)
6. [Best Practices](#6-best-practices)
7. [Common Mistakes & Pitfalls](#7-common-mistakes--pitfalls)
8. [Performance, Accessibility, SEO & Security Notes](#8-performance-accessibility-seo--security-notes)
9. [Comparison / When to Use What](#9-comparison--when-to-use-what)
10. [Interview Questions](#10-interview-questions)
11. [Practice Exercises](#11-practice-exercises)
12. [Summary / Cheat Sheet](#12-summary--cheat-sheet)
13. [Further Reading](#13-further-reading)

---

## 1. What & Why

Historically, web applications were trapped inside browser navigation bars, tabs, and bookmark lists. If users wanted a desktop dock icon, push notifications, offline functionality, and an immersive full-screen application shell without an address bar, developers had to rewrite their entire codebase as native applications for iOS (Swift/Obj-C) and Android (Kotlin/Java).

The **Web App Manifest** (linked via `<link rel="manifest">`) changes everything. It is a standardized JSON document that provides the operating system with metadata explaining how to install and launch the web application natively on mobile phones, tablets, and desktop workstations (Windows, macOS, ChromeOS).

Combined with service workers and modern media capabilities, the Web App Manifest allows users to install a website directly onto their home screen or app dock, complete with custom splash screens, standalone window frames, app shortcuts, and adaptive maskable icons.

---

## 2. Core Concepts

### Core Manifest Properties
- **`name`**: The full title of the application displayed on install prompts, splash screens, and store listings.
- **`short_name`**: A concise label (under 12 characters) displayed beneath the app icon on mobile home screens where space is restricted.
- **`start_url`**: The URL loaded when the user taps the installed home screen icon. Should typically point to the app root (e.g., `/?source=pwa`).
- **`scope`**: Defines the navigation boundary of the PWA. If a user clicks a link outside the `scope`, the browser opens a standard browser tab instead of staying within the standalone window.
- **`display`**: Defines the window framing:
  - `standalone`: The industry standard. Hides the browser address bar and navigation controls; looks like a native app.
  - `fullscreen`: Completely fills the entire screen (no OS status bars or dock).
  - `minimal-ui`: Displays a minimal back/refresh button strip without a full address bar.
  - `browser`: Opens in a standard browser tab.
- **`background_color`**: The background color displayed on the native splash screen while the app boots.
- **`theme_color`**: Sets the color of the native operating system title bar and status bar.

### Icon Standards: "Any" vs. "Maskable"
Android devices automatically clip app icons into various shapes depending on the OEM theme (circles on Google Pixels, rounded squircles on Samsung Galaxy devices).
- **`purpose: "any"`**: A standard icon with a transparent background. If placed in an adaptive launcher, the OS adds an unsightly white border around it.
- **`purpose: "maskable"`**: Designed with a safe-zone: the artwork occupies the central 80% circle, allowing the OS to crop the icon into any geometric shape without cutting off logos.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------------+
|                  PWA Installation & Operating System Handshake                          |
+-----------------------------------------------------------------------------------------+

  User visits HTTPS website
       |
       v
  1. Browser parses <link rel="manifest" href="/manifest.webmanifest">
       |
       +--> Fetches and validates JSON structure.
       |
  2. Installation Criteria Verification:
     - Served over secure HTTPS?                  [OK]
     - Manifest has valid name, start_url, icons? [OK]
     - Registered active Service Worker?          [OK]
       |
       v
  3. Browser fires `beforeinstallprompt` event:
     - Allows developer to present a custom "Install App" button in UI.
       |
       v
  4. User taps "Install":
     - OS registers Web App into native registry (Windows Start Menu / Android Launcher / macOS Dock).
     - Downloads 192px and 512px icons.
       |
       v
  5. User launches PWA from Home Screen:
     - OS boots custom splash screen (using background_color).
     - Renders standalone app window without browser URL bar!
```

---

## 4. Syntax & Usage

```html
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>Pulse Task Manager</title>

  <!-- 1. Standard Web App Manifest -->
  <link rel="manifest" href="/manifest.webmanifest" />

  <!-- 2. Apple iOS Safari Proprietary Fallbacks -->
  <link rel="apple-touch-icon" href="/icons/apple-touch-icon-180x180.png" />
  <meta name="apple-mobile-web-app-capable" content="yes" />
  <meta name="apple-mobile-web-app-status-bar-style" content="black-translucent" />
  <meta name="apple-mobile-web-app-title" content="Pulse" />

  <!-- 3. Operating System Theme Colors -->
  <meta name="theme-color" content="#0284c7" />
</head>
```

> **Cross-Browser & Baseline Status Box**
>
> - Web App Manifest (`<link rel="manifest">`): **Baseline Widely Available** across Chrome, Edge, Firefox, and Safari (iOS 11.3+ and macOS 14+ Sonoma).
> - Maskable Icons: Supported across Android Chromium engines.
> - PWA Installability on Desktop: Chrome, Edge, and Safari on macOS Sonoma.

---

## 5. Practical Examples

### Example 1: The Canonical Production Web App Manifest (`manifest.webmanifest`)
A complete, production-grade manifest file conforming to modern W3C specifications.

```json
{
  "$schema": "https://json.schemastore.org/web-manifest-combined.json",
  "name": "Pulse Productivity Suite",
  "short_name": "Pulse",
  "description": "Collaborative project management and realtime task tracking for distributed teams.",
  "start_url": "/?source=pwa",
  "scope": "/",
  "display": "standalone",
  "orientation": "portrait-primary",
  "background_color": "#0f172a",
  "theme_color": "#0284c7",
  "categories": ["productivity", "utilities", "business"],
  "icons": [
    {
      "src": "/icons/icon-192x192.png",
      "sizes": "192x192",
      "type": "image/png",
      "purpose": "any"
    },
    {
      "src": "/icons/icon-512x512.png",
      "sizes": "512x512",
      "type": "image/png",
      "purpose": "any"
    },
    {
      "src": "/icons/maskable-icon-512x512.png",
      "sizes": "512x512",
      "type": "image/png",
      "purpose": "maskable"
    }
  ]
}
```

---

### Example 2: Complete HTML `<head>` Integration (PWA + Apple iOS Touch Icons)
While modern iOS Safari supports the Web App Manifest, Apple still relies on specific proprietary `<link>` and `<meta>` tags for home screen icons and launch screens.

```html
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover" />
  <title>Pulse Task Manager</title>

  <!-- W3C Standard Manifest -->
  <link rel="manifest" href="/manifest.webmanifest" />

  <!-- Apple iOS Touch Icons (180x180 standard for iPhone Retina) -->
  <link rel="apple-touch-icon" sizes="180x180" href="/icons/apple-touch-icon-180x180.png" />

  <!-- Apple iOS Standalone Mode Directives -->
  <meta name="apple-mobile-web-app-capable" content="yes" />
  <meta name="apple-mobile-web-app-status-bar-style" content="default" />
  <meta name="apple-mobile-web-app-title" content="Pulse" />

  <!-- Universal Favicons -->
  <link rel="icon" href="/favicon.ico" sizes="32x32" />
  <link rel="icon" href="/icon.svg" type="image/svg+xml" />

  <!-- Native Browser Theme Color -->
  <meta name="theme-color" content="#0284c7" />
</head>
<body>
  <h1>Pulse App Interface</h1>
</body>
</html>
```

---

### Example 3: Maskable Icons for Adaptive Android App Launchers
Illustrates how the maskable safe zone is defined inside a 512x512 PNG image.

```json
{
  "icons": [
    {
      "src": "/icons/icon-any.png",
      "sizes": "512x512",
      "type": "image/png",
      "purpose": "any"
    },
    {
      "src": "/icons/icon-maskable.png",
      "sizes": "512x512",
      "type": "image/png",
      "purpose": "maskable"
    }
  ]
}
```
*Note: In `icon-maskable.png`, the critical logo graphic must reside entirely within the central **safe zone** (a circle with a diameter of 80% of the canvas = 410 pixels), while the background color fills the remaining outer 10% bleed area.*

---

### Example 4: PWA App Shortcuts for Instant Actions
App Shortcuts allow users on Android, Windows, and macOS to right-click or long-press the installed PWA icon to access quick menu actions.

```json
{
  "name": "Pulse Productivity Suite",
  "short_name": "Pulse",
  "start_url": "/",
  "display": "standalone",
  "shortcuts": [
    {
      "name": "Create New Task",
      "short_name": "New Task",
      "description": "Quickly create and assign a new project task",
      "url": "/tasks/new?source=shortcut",
      "icons": [
        {
          "src": "/icons/shortcut-new-task.png",
          "sizes": "96x96",
          "type": "image/png"
        }
      ]
    },
    {
      "name": "View Team Calendar",
      "short_name": "Calendar",
      "description": "Jump directly to the team sprint schedule",
      "url": "/calendar?source=shortcut",
      "icons": [
        {
          "src": "/icons/shortcut-calendar.png",
          "sizes": "96x96",
          "type": "image/png"
        }
      ]
    }
  ]
}
```

---

### Example 5: Intercepting the `beforeinstallprompt` Event in JavaScript
Enabling a custom in-app "Install App" button rather than relying on browser-default omnibox prompts.

```html
<header class="app-header">
  <h2>Pulse Workspace</h2>
  <button type="button" id="pwa-install-btn" hidden class="install-btn">
    Install Pulse App
  </button>
</header>

<script>
  const installBtn = document.getElementById('pwa-install-btn');
  let deferredPrompt = null;

  // 1. Browser fires this event if the PWA meets all installability criteria
  window.addEventListener('beforeinstallprompt', (e) => {
    // Prevent the default mini-infobar from appearing automatically
    e.preventDefault();
    // Stash the event so it can be triggered later
    deferredPrompt = e;
    // Reveal our custom branded Install button in the UI
    installBtn.hidden = false;
  });

  // 2. User clicks the custom button
  installBtn.addEventListener('click', async () => {
    if (!deferredPrompt) return;

    // Show the native OS installation prompt
    deferredPrompt.prompt();

    // Wait for the user to respond to the prompt
    const { outcome } = await deferredPrompt.userChoice;
    console.log(`User installation choice: ${outcome}`);

    // Reset prompt and hide button
    deferredPrompt = null;
    installBtn.hidden = true;
  });

  // 3. Track successful installation
  window.addEventListener('appinstalled', () => {
    console.log('Pulse PWA was successfully installed on the operating system!');
    installBtn.hidden = true;
  });
</script>
```

---

## 6. Best Practices

- [ ] **Name the manifest file with `.webmanifest` extension**: Standardized by the W3C specification (`manifest.webmanifest`), and configure your web server to return `Content-Type: application/manifest+json`.
- [ ] **Provide both 192x192 and 512x512 icons**: 192px is used for mobile home screens and task switchers; 512px is used for desktop splash screens and app store listings.
- [ ] **Always include at least one `maskable` icon**: Guarantees Android devices do not render an ugly white box around your brand icon.
- [ ] **Supply `apple-touch-icon` in HTML `<head>`**: While iOS Safari reads the manifest, it still prioritizes `<link rel="apple-touch-icon" href="...">` for home screen bookmark generation.
- [ ] **Append `?source=pwa` to `start_url`**: Enables web analytics to cleanly differentiate desktop browser sessions from installed PWA standalone sessions.
- [ ] **Match `theme_color` across HTML `<meta>` and the manifest**: Keep the color declared in `<meta name="theme-color">` identical to `theme_color` in `manifest.webmanifest`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Severity | Consequence | Correction |
| :--- | :--- | :--- | :--- |
| Missing `apple-touch-icon` in HTML `<head>` | **High** | iOS Safari generates an ugly screenshot thumbnail of the webpage instead of an app icon. | Add `<link rel="apple-touch-icon" sizes="180x180" href="...">`. |
| Serving manifest over insecure HTTP | **Critical** | Browsers strictly disallow PWA installation over insecure origins. | Ensure the entire domain enforces HTTPS. |
| Serving manifest as `text/plain` | **Moderate** | Some browsers fail to parse the manifest or log console warnings. | Serve with `Content-Type: application/manifest+json`. |
| Putting critical logos in the outer 10% of a maskable icon | **Moderate** | Android adaptive launchers crop off parts of the logo or text. | Ensure all logos sit inside the 80% central safe circle. |
| Setting `scope` narrower than `start_url` | **High** | Launching the PWA immediately breaks out of standalone mode into a browser tab. | Ensure `start_url` falls strictly within the `scope` directory. |
| Forgetting `short_name` | **Low** | Mobile OS truncates a long `name` with awkward ellipses on home screens. | Include `short_name` with 12 characters or fewer. |

---

## 8. Performance, Accessibility, SEO & Security Notes

### Accessibility (WCAG 2.2 AA)
- **High-Contrast App Icons**: App icons should maintain high contrast against both light and dark wallpaper backgrounds.
- **Accessible Names on Install Prompts**: Custom install buttons must have clear accessible names (e.g., `aria-label="Install Pulse Task Manager to your home screen"`).

### Security
- **Origin Isolation**: Installed PWAs run in the exact same security sandbox as their originating web origin. Cookies, `localStorage`, and IndexedDB are shared between the browser tab and the installed standalone window.

---

## 9. Comparison / When to Use What

| Display Mode | Window UI | Address Bar Shown? | Best Used For |
| :--- | :--- | :--- | :--- |
| **`standalone`** | Native OS Window Frame | **No** (Completely hidden) | Standard web applications, productivity tools, dashboards. |
| **`fullscreen`** | Entire Screen Surface | **No** (Status bar hidden) | Immersive games, video streaming players, kiosks. |
| **`minimal-ui`** | Minimal Navigation Strip | No (Only back/forward/refresh) | Content-heavy sites where back navigation is critical. |
| **`browser`** | Normal Browser Tab | **Yes** (Full URL bar) | Traditional websites with simple bookmarking needs. |

---

## 10. Interview Questions

<details>
<summary><strong>1. What technical criteria must an application fulfill to qualify for PWA installability in Google Chrome?</strong></summary>

Chrome requires:
1. The web application must be served over secure HTTPS (or `localhost` for development).
2. It must have a valid Web App Manifest containing: `name` or `short_name`, `start_url`, `display` set to `standalone`, `fullscreen`, or `minimal-ui`, and icons including at least a 192x192 and a 512x512 PNG icon.
3. It must register a Service Worker containing a `fetch` event handler to provide basic offline functionality.
</details>

<details>
<summary><strong>2. What is a "Maskable Icon", and why is it essential for Android PWA deployment?</strong></summary>

Android devices support "Adaptive Icons", where the operating system automatically masks and crops app icons into circles, squircles, or rounded rectangles depending on the user's launcher theme. A standard icon (`purpose: "any"`) has a fixed transparent background, which causes Android to shrink the icon and place it inside a generic white box. A **maskable icon** (`purpose: "maskable"`) provides full-bleed background artwork with all vital logo graphics contained within an 80% central safe zone, allowing the OS to crop the icon into any shape without losing visual fidelity.
</details>

<details>
<summary><strong>3. How does Apple iOS handle PWA installation differently from Chromium-based browsers?</strong></summary>

Unlike Chromium, iOS Safari does not support the `beforeinstallprompt` event; websites cannot programmatically display an "Install" button. Users must manually tap the iOS Share button and select "Add to Home Screen". Furthermore, while iOS Safari now reads the Web App Manifest, it historically required and still prioritizes proprietary tags: `<link rel="apple-touch-icon">` for home screen icons, and `<meta name="apple-mobile-web-app-capable" content="yes">` to enable standalone mode.
</details>

<details>
<summary><strong>4. What does the <code>scope</code> property in a Web App Manifest govern?</strong></summary>

The `scope` defines the set of URLs that belong to the PWA application. If a user navigates to a URL within the scope (e.g., `/tasks/1`), the page loads inside the standalone PWA window. If the user clicks an external link or a link outside the declared scope (e.g., `/blog` when scope is `/app/`), the operating system launches a standard web browser tab rather than navigating within the standalone app.
</details>

<details>
<summary><strong>5. How does the <code>beforeinstallprompt</code> event allow developers to customize the PWA install experience?</strong></summary>

When Chrome determines a PWA is installable, it fires the `beforeinstallprompt` event. By calling `e.preventDefault()`, developers can prevent the browser's default generic mini-infobar from popping up. The developer saves the event reference and unhides a custom, branded in-app button ("Install App"). When the user clicks the button, the developer calls `e.prompt()`, triggering the native OS install dialog at the optimal moment in the user journey.
</details>

<details>
<summary><strong>6. What is the difference between <code>theme_color</code> and <code>background_color</code> in the manifest?</strong></summary>

- `background_color`: Defines the background color of the native splash screen shown while the operating system boots the PWA and loads the stylesheet.
- `theme_color`: Sets the color of the application window's title bar, operating system status bar, and task switcher header.
</details>

<details>
<summary><strong>7. What are PWA Shortcuts, and how are they surfaced to users?</strong></summary>

PWA Shortcuts are defined in the `shortcuts` array in `manifest.webmanifest`. When a user right-clicks the installed PWA desktop icon (Windows taskbar, macOS dock) or long-presses the icon on a mobile home screen, the operating system displays a native quick-action menu (e.g., "New Message", "View Calendar") that links directly to specific deep URLs.
</details>

<details>
<summary><strong>8. What official MIME type should be returned by web servers when serving <code>manifest.webmanifest</code>?</strong></summary>

The official W3C MIME type is `application/manifest+json`. While many browsers tolerate `application/json`, configuring your web server to return `application/manifest+json; charset=utf-8` conforms strictly to the specification and prevents browser console warnings.
</details>

---

## 11. Practice Exercises

### Exercise 1: Minimal Valid PWA Manifest (Easy)
- **Task**: Author a minimal, 100% valid `manifest.webmanifest` file for an application named "QuickNotes" with standalone display, start URL `/`, and a 192x192 icon.

<details>
<summary>View Solution</summary>

```json
{
  "name": "QuickNotes",
  "short_name": "Notes",
  "start_url": "/?source=pwa",
  "display": "standalone",
  "background_color": "#ffffff",
  "theme_color": "#4f46e5",
  "icons": [
    {
      "src": "/icon-192.png",
      "sizes": "192x192",
      "type": "image/png"
    }
  ]
}
```
</details>

---

### Exercise 2: Complete HTML Head PWA Setup (Medium)
- **Task**: Author the HTML `<head>` containing the manifest link, Apple touch icon (180x180), iOS web app capability metadata, and responsive viewport.

<details>
<summary>View Solution</summary>

```html
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover" />
  <title>QuickNotes PWA</title>

  <!-- Web App Manifest -->
  <link rel="manifest" href="/manifest.webmanifest" />

  <!-- iOS Safari Compatibility -->
  <link rel="apple-touch-icon" sizes="180x180" href="/icons/apple-touch-icon.png" />
  <meta name="apple-mobile-web-app-capable" content="yes" />
  <meta name="apple-mobile-web-app-status-bar-style" content="default" />
  <meta name="apple-mobile-web-app-title" content="QuickNotes" />

  <!-- Theme Color -->
  <meta name="theme-color" content="#4f46e5" />
</head>
```
</details>

---

### Exercise 3: Full-Featured Enterprise Manifest with Shortcuts and Maskable Icons (Hard)
- **Task**: Build an enterprise manifest for "AeroMail" with:
  1. Name and short name.
  2. Scope set to `/app/` and start_url `/app/inbox`.
  3. Standalone display.
  4. Dual icon configuration: 512px standard (`any`) and 512px `maskable`.
  5. Two quick shortcuts: "Compose Email" (`/app/compose`) and "Search Mail" (`/app/search`).

<details>
<summary>View Solution</summary>

```json
{
  "name": "AeroMail Enterprise",
  "short_name": "AeroMail",
  "description": "Encrypted enterprise email and calendar suite.",
  "start_url": "/app/inbox?source=pwa",
  "scope": "/app/",
  "display": "standalone",
  "orientation": "any",
  "background_color": "#111827",
  "theme_color": "#3b82f6",
  "categories": ["business", "communication"],
  "icons": [
    {
      "src": "/icons/aeromail-512.png",
      "sizes": "512x512",
      "type": "image/png",
      "purpose": "any"
    },
    {
      "src": "/icons/aeromail-maskable-512.png",
      "sizes": "512x512",
      "type": "image/png",
      "purpose": "maskable"
    }
  ],
  "shortcuts": [
    {
      "name": "Compose New Email",
      "short_name": "Compose",
      "url": "/app/compose",
      "icons": [{ "src": "/icons/shortcut-compose.png", "sizes": "96x96" }]
    },
    {
      "name": "Search Inboxes",
      "short_name": "Search",
      "url": "/app/search",
      "icons": [{ "src": "/icons/shortcut-search.png", "sizes": "96x96" }]
    }
  ]
}
```
</details>

---

## 12. Summary / Cheat Sheet

```
+-------------------------------------------------------------------------------------------------+
|                                 PWA MANIFEST QUICK REFERENCE                                    |
+-------------------------------------------------------------------------------------------------+
| Property / Tag           | Value / Recommendation                  | Function                   |
+-------------------------------------------------------------------------------------------------+
| <link rel="manifest">    | href="/manifest.webmanifest"            | Links manifest in HTML     |
| MIME Type                | application/manifest+json               | Standard HTTP Content-Type |
| name / short_name        | Full title / < 12 char home screen label| App naming                 |
| start_url / scope        | "/?source=pwa" / "/"                    | Launch URL & app boundary  |
| display                  | "standalone"                            | Hides browser address bar  |
| purpose: "maskable"      | Central 80% safe zone logo              | Adaptive Android launcher  |
| apple-touch-icon         | 180x180 PNG in <head>                   | iOS home screen icon       |
| beforeinstallprompt      | window event in JS                      | Custom in-app install btn  |
+-------------------------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [W3C Web App Manifest Specification](https://www.w3.org/TR/appmanifest/)
- [MDN Web Docs: Web App Manifest](https://developer.mozilla.org/en-US/docs/Web/Manifest)
- [web.dev: Define app icons and installability criteria](https://web.dev/learn/pwa/installation)
- [Maskable.app: Maskable Icon Editor and Preview Tool](https://maskable.app/)

---

[← Previous: Robots, Sitemaps & hreflang](08-robots-sitemaps-and-hreflang.md) | [Section Index](README.md) | [Next: Content Security Policy in Meta →](10-content-security-policy-meta.md)
