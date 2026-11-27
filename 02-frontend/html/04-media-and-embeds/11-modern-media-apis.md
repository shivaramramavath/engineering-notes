# Modern Media APIs: Picture-in-Picture, Media Session, and Fullscreen

| Metadata | Details |
| :--- | :--- |
| **Specification** | W3C Picture-in-Picture / W3C Media Session Standard / WHATWG Fullscreen API |
| **Prerequisites** | `04-media-and-embeds/01-audio-and-video.md`, `04-media-and-embeds/07-webvtt-and-subtitles.md` |
| **Estimated Reading Time** | 25 minutes |
| **Status** | Production Ready |
| **Baseline Status** | Widely Available (Picture-in-Picture, Fullscreen API, Media Session API) |
| **Browsers Tested** | Chrome 124+, Firefox 125+, Safari 17.4+, Edge 124+ |
| **Tested With** | W3C Nu HTML Validator, Chrome DevTools, macOS / Android Lock Screen Media Controls |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
   - [Example 1: Toggleable Picture-in-Picture (PiP) Video Controller](#example-1-toggleable-picture-in-picture-pip-video-controller)
   - [Example 2: Media Session API Integration for OS Lock Screen and Wearable Controls](#example-2-media-session-api-integration-for-os-lock-screen-and-wearable-controls)
   - [Example 3: Accessible Cross-Browser Fullscreen Container](#example-3-accessible-cross-browser-fullscreen-container)
   - [Example 4: Document Picture-in-Picture API (Arbitrary HTML in Floating Window)](#example-4-document-picture-in-picture-api-arbitrary-html-in-floating-window)
   - [Example 5: Screen Capture and Live Stream Preview](#example-5-screen-capture-and-live-stream-preview)
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

Historically, video and audio playback on the web was confined entirely to the browser viewport. If a user switched tabs to write an email, navigated away from a video lecture, or locked their mobile device screen, playback either ceased or became completely uncontrollable.

Modern Web Media APIs bridge HTML multimedia elements with the underlying operating system:
- **Picture-in-Picture (PiP) API**: Allows users to pop a `<video>` out into a floating, pinned window that remains visible above all other OS applications.
- **Media Session API**: Connects web audio/video to the operating system's native hardware media controls (hardware keyboard play/pause keys, Bluetooth headset buttons, smartwatches, and lock screen media notifications).
- **Fullscreen API**: Expands any HTML element or video container into immersive, distraction-free full-screen mode with custom CSS `:fullscreen` styling.
- **Document Picture-in-Picture API**: Extends PiP beyond simple video, allowing arbitrary interactive HTML DOM trees (video call participants, interactive notes, custom playback controls) to float in an independent mini-window.

Integrating these modern APIs transforms a basic webpage media player into a native-feeling, OS-integrated multimedia application.

---

## 2. Core Concepts

### 1. Picture-in-Picture API (`HTMLVideoElement`)
Enables video playback in an always-on-top floating window.
- `video.requestPictureInPicture()`: Asynchronously requests PiP mode (requires a user gesture).
- `document.exitPictureInPicture()`: Closes the active floating window.
- `document.pictureInPictureElement`: Returns the currently floating `<video>` element, or `null`.
- `disablePictureInPicture`: HTML attribute that blocks users from popping the video into PiP.

### 2. Media Session API (`navigator.mediaSession`)
Exposes rich metadata and action handlers to the operating system notification shade and lock screen:
- **Metadata (`MediaMetadata`)**: Title, artist, album name, and an array of artwork icons (at various resolutions).
- **Action Handlers (`setActionHandler`)**: Intercepts hardware button events: `'play'`, `'pause'`, `'seekbackward'`, `'seekforward'`, `'previoustrack'`, `'nexttrack'`, `'seekto'`.

### 3. Fullscreen API
Expands any DOM element to fill the user's entire screen:
- `element.requestFullscreen()`: Promotes element to the top layer.
- `document.exitFullscreen()`: Exits fullscreen mode.
- `document.fullscreenElement`: Tracks which element is currently fullscreen.
- CSS pseudo-class `:fullscreen`: Styles elements specifically when in fullscreen mode.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------------+
|                  Modern Media APIs & OS Bridge Architecture                             |
+-----------------------------------------------------------------------------------------+

  Webpage (<video id="player">)
       |
       +--> User clicks "Watch in Mini-Player"
       |      video.requestPictureInPicture()
       |
       v
  [ Browser Engine Compositor ]
       |
       +--> Detaches video surface from DOM Layout Tree
       +--> Creates native OS desktop floating window (NSWindow / HWND)
       +--> Surfaces floating window ABOVE all native applications!
       |
  [ Operating System Event Bus ]
       |
       +--> User taps Play/Pause on Bluetooth Headset
       |      OS sends hardware media key event to Browser
       |      Browser routes to: navigator.mediaSession.setActionHandler('pause', fn)
       |      JavaScript executes fn() -> player.pause()
```

---

## 4. Syntax & Usage

```html
<video id="player" controls width="800" height="450">
  <source src="/media/podcast.mp4" type="video/mp4" />
</video>
<button type="button" id="pip-btn">Pop out to Mini-Player</button>

<script>
  const player = document.getElementById('player');
  const pipBtn = document.getElementById('pip-btn');

  // Check browser capability
  if (document.pictureInPictureEnabled) {
    pipBtn.addEventListener('click', async () => {
      if (document.pictureInPictureElement) {
        await document.exitPictureInPicture();
      } else {
        await player.requestPictureInPicture();
      }
    });
  } else {
    pipBtn.hidden = true;
  }
</script>
```

> **Cross-Browser & Baseline Status Box**
>
> - Picture-in-Picture API: **Baseline Widely Available** (Chrome 70+, Safari 13.1+, Firefox 71+, Edge 79+).
> - Media Session API: **Baseline Widely Available** across mobile and desktop browsers (Chrome 73+, Safari 15+, Firefox 82+).
> - Fullscreen API: **Baseline Widely Available** across all desktop and mobile browsers.

---

## 5. Practical Examples

### Example 1: Toggleable Picture-in-Picture (PiP) Video Controller
A robust PiP implementation that dynamically updates button state, handles user dismissals, and degrades gracefully if unsupported.

```html
<div class="video-widget">
  <video id="live-stream-vid" controls width="640" height="360" playsinline>
    <source src="/media/stream.mp4" type="video/mp4" />
  </video>

  <div class="toolbar">
    <button type="button" id="pip-toggle" aria-label="Toggle Picture in Picture">
      Watch in Mini-Player
    </button>
  </div>
</div>

<script>
  const video = document.getElementById('live-stream-vid');
  const pipToggle = document.getElementById('pip-toggle');

  if (!document.pictureInPictureEnabled) {
    pipToggle.disabled = true;
    pipToggle.textContent = 'Picture-in-Picture Unavailable';
  } else {
    pipToggle.addEventListener('click', async () => {
      try {
        if (video !== document.pictureInPictureElement) {
          await video.requestPictureInPicture();
        } else {
          await document.exitPictureInPicture();
        }
      } catch (err) {
        console.error('PiP request failed:', err);
      }
    });

    // Update button text when user enters or exits PiP
    video.addEventListener('enterpictureinpicture', () => {
      pipToggle.textContent = 'Return to Standard View';
    });

    video.addEventListener('leavepictureinpicture', () => {
      pipToggle.textContent = 'Watch in Mini-Player';
    });
  }
</script>
```

---

### Example 2: Media Session API Integration for OS Lock Screen and Wearable Controls
Enables full lock screen media controls, album artwork, and hardware keyboard media key bindings.

```html
<audio id="audio-app" controls preload="metadata">
  <source src="/music/track-01.mp3" type="audio/mpeg" />
</audio>

<script>
  const audio = document.getElementById('audio-app');

  function setupMediaSession() {
    if ('mediaSession' in navigator) {
      // 1. Populate OS Lock Screen / Smartwatch Metadata
      navigator.mediaSession.metadata = new MediaMetadata({
        title: 'Echoes of the Nebula',
        artist: 'Stellar Soundscape',
        album: 'Cosmic Journeys Vol. 2',
        artwork: [
          { src: '/images/album-96.png',   sizes: '96x96',   type: 'image/png' },
          { src: '/images/album-128.png', sizes: '128x128', type: 'image/png' },
          { src: '/images/album-512.png', sizes: '512x512', type: 'image/png' }
        ]
      });

      // 2. Bind Hardware Action Handlers
      navigator.mediaSession.setActionHandler('play', () => audio.play());
      navigator.mediaSession.setActionHandler('pause', () => audio.pause());

      navigator.mediaSession.setActionHandler('seekbackward', (details) => {
        audio.currentTime = Math.max(audio.currentTime - (details.seekOffset || 10), 0);
      });

      navigator.mediaSession.setActionHandler('seekforward', (details) => {
        audio.currentTime = Math.min(audio.currentTime + (details.seekOffset || 10), audio.duration);
      });

      navigator.mediaSession.setActionHandler('seekto', (details) => {
        if (details.fastSeek && 'fastSeek' in audio) {
          audio.fastSeek(details.seekTime);
        } else {
          audio.currentTime = details.seekTime;
        }
      });
    }
  }

  audio.addEventListener('play', setupMediaSession);
</script>
```

---

### Example 3: Accessible Cross-Browser Fullscreen Container
Expanding a custom video player container (including video, custom subtitles, and UI controls) into fullscreen mode with proper CSS pseudo-class styling.

```html
<div class="player-container" id="player-container">
  <video id="player-media" width="800" height="450" playsinline>
    <source src="/media/talk.mp4" type="video/mp4" />
  </video>

  <div class="custom-controls">
    <button type="button" id="fullscreen-btn" aria-label="Toggle Fullscreen">
      Expand Fullscreen
    </button>
  </div>
</div>

<style>
  .player-container {
    position: relative;
    max-width: 800px;
    background: #000;
  }
  .player-container video {
    width: 100%;
    height: auto;
    display: block;
  }

  /* Fullscreen Top Layer Styling */
  .player-container:fullscreen {
    width: 100vw;
    height: 100vh;
    display: flex;
    flex-direction: column;
    justify-content: center;
    background-color: #000000;
  }
  .player-container:fullscreen video {
    max-height: 100vh;
    object-fit: contain;
  }
</style>

<script>
  const container = document.getElementById('player-container');
  const fsBtn = document.getElementById('fullscreen-btn');

  fsBtn.addEventListener('click', async () => {
    if (!document.fullscreenElement) {
      try {
        await container.requestFullscreen();
        fsBtn.textContent = 'Exit Fullscreen';
      } catch (err) {
        console.error('Fullscreen request rejected:', err);
      }
    } else {
      await document.exitFullscreen();
      fsBtn.textContent = 'Expand Fullscreen';
    }
  });

  document.addEventListener('fullscreenchange', () => {
    if (!document.fullscreenElement) {
      fsBtn.textContent = 'Expand Fullscreen';
    }
  });
</script>
```

---

### Example 4: Document Picture-in-Picture API (Arbitrary HTML in Floating Window)
Extending PiP beyond single video files: popping out an entire interactive DOM subtree (e.g., video conferencing participant grid or notepad) into a floating native window.

```html
<div class="conference-widget">
  <div id="pip-content">
    <h3>Live Video Call (3 Participants)</h3>
    <p>Room: #engineering-sync</p>
    <button type="button" onclick="alert('Muted!')">Mute Mic</button>
  </div>

  <button type="button" id="doc-pip-btn">Pop Out Conference Window</button>
</div>

<script>
  const docPipBtn = document.getElementById('doc-pip-btn');
  const content = document.getElementById('pip-content');

  if ('documentPictureInPicture' in window) {
    docPipBtn.addEventListener('click', async () => {
      // 1. Open floating Document PiP window
      const pipWindow = await window.documentPictureInPicture.requestWindow({
        width: 320,
        height: 200
      });

      // 2. Move DOM subtree into floating window
      pipWindow.document.body.append(content);

      // 3. Return content back to parent document when PiP closes
      pipWindow.addEventListener('pagehide', () => {
        document.querySelector('.conference-widget').prepend(content);
      });
    });
  } else {
    docPipBtn.hidden = true;
  }
</script>
```

---

### Example 5: Screen Capture and Live Stream Preview
Using `getDisplayMedia` to capture the user's screen, window, or browser tab and pipe the live stream directly into an HTML5 `<video>` element.

```html
<div class="screen-recorder">
  <h2>Live Screen Share Preview</h2>
  <button type="button" id="start-share-btn">Start Screen Sharing</button>
  <button type="button" id="stop-share-btn" disabled>Stop Sharing</button>

  <video id="screen-preview" autoplay playsinline muted width="800" height="450" class="preview-box"></video>
</div>

<script>
  const startBtn = document.getElementById('start-share-btn');
  const stopBtn = document.getElementById('stop-share-btn');
  const preview = document.getElementById('screen-preview');
  let captureStream = null;

  startBtn.addEventListener('click', async () => {
    try {
      captureStream = await navigator.mediaDevices.getDisplayMedia({
        video: { cursor: 'always' },
        audio: false
      });

      preview.srcObject = captureStream;
      startBtn.disabled = true;
      stopBtn.disabled = false;

      // Handle user clicking "Stop Sharing" on native browser banner
      captureStream.getVideoTracks()[0].addEventListener('ended', () => {
        stopSharing();
      });
    } catch (err) {
      console.error('Screen capture canceled or denied:', err);
    }
  });

  function stopSharing() {
    if (captureStream) {
      captureStream.getTracks().forEach(track => track.stop());
      preview.srcObject = null;
      startBtn.disabled = false;
      stopBtn.disabled = true;
    }
  }

  stopBtn.addEventListener('click', stopSharing);
</script>
```

---

## 6. Best Practices

- [ ] **Always check for API feature support before rendering trigger buttons**: Check `document.pictureInPictureEnabled`, `'mediaSession' in navigator`, or `document.fullscreenEnabled`.
- [ ] **Ensure PiP and Fullscreen requests are triggered by explicit user gestures**: Browsers will reject `.requestFullscreen()` and `.requestPictureInPicture()` if called outside of a click or keydown event handler.
- [ ] **Populate `navigator.mediaSession.metadata` with multiple artwork resolutions**: Include icon sizes from `96x96` up to `512x512` so smartwatches, Android lock screens, and macOS Control Centers render crisp artwork.
- [ ] **Clean up event listeners and restore DOM when exiting Document PiP**: Listen to `pagehide` on the floating window to safely re-parent HTML content back into the main document.
- [ ] **Use the `:fullscreen` CSS pseudo-class to ensure responsive layout scaling**: Do not assume full-screen monitors share the aspect ratio of the video; use `object-fit: contain`.
- [ ] **Keep hardware action handlers in sync with actual playback state**: Update `navigator.mediaSession.playbackState = 'playing' | 'paused'` whenever playback toggles.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Severity | Consequence | Correction |
| :--- | :--- | :--- | :--- |
| Calling `.requestPictureInPicture()` without a user gesture | **Critical** | Browser throws `NotAllowedError`; request is rejected. | Trigger PiP only inside a `click` or `keydown` event listener. |
| Forgetting `document.pictureInPictureEnabled` capability check | **Moderate** | Errors out on browsers where PiP is disabled by user policy or unsupported. | Guard with `if (document.pictureInPictureEnabled)`. |
| Neglecting `document.fullscreenElement` tracking | **Moderate** | Button states fall out of sync if the user exits fullscreen using the hardware `Escape` key. | Listen to `fullscreenchange` events on `document`. |
| Forgetting to stop MediaStream tracks after screen sharing | **High** | Browser keeps screen recording indicator active in OS menu bar; privacy concern. | Call `track.stop()` on all tracks when finished. |
| Passing single low-resolution artwork to `MediaMetadata` | **Low** | Lock screens and smartwatches render blurry, pixelated album art. | Provide multiple sizes (96px, 128px, 256px, 512px). |
| Expanding `<video>` directly instead of its parent container | **Moderate** | Custom subtitles and overlay controls disappear when entering native video fullscreen. | Call `.requestFullscreen()` on the parent wrapper `<div>`. |

---

## 8. Performance, Accessibility, SEO & Security Notes

### Accessibility (WCAG 2.2 AA)
- **Guideline 2.1.1 Keyboard**: Ensure Fullscreen and PiP buttons are reachable via `Tab` and activated by `Space` or `Enter`.
- **Guideline 4.1.2 Name, Role, Value**: Update `aria-pressed` or button text dynamically (e.g., "Enter Fullscreen" &rarr; "Exit Fullscreen") to convey mode transitions to screen readers.

### Security Defenses
- **Transient User Activation**: Fullscreen and PiP strictly require transient activation (user interaction within the last few seconds) to prevent malicious sites from hijacking the user's screen or launching floating windows without consent.
- **Permissions Policy (`allow`)**: Embedded `<iframe>` elements must declare `allow="fullscreen; picture-in-picture"` to use these APIs.

---

## 9. Comparison / When to Use What

| API | Display Location | Content Supported | User Gesture Required? | Primary Use Case |
| :--- | :--- | :--- | :--- | :--- |
| **Picture-in-Picture (PiP)** | OS floating window | `<video>` element only | Yes | Multitasking while watching video tutorials, streams. |
| **Document PiP** | OS floating window | Arbitrary HTML / DOM | Yes | Video calls, interactive notes, floating timers. |
| **Fullscreen API** | Entire monitor display | Any HTML element | Yes | Immersive games, presentations, video theater mode. |
| **Media Session API** | OS lock screen, notifications | Metadata & Action Handlers | No | Syncing playback state with hardware media keys. |

---

## 10. Interview Questions

<details>
<summary><strong>1. Why must <code>video.requestPictureInPicture()</code> be called inside a user gesture handler?</strong></summary>

Browser security models require **transient user activation** (e.g., a physical click, tap, or keypress) before granting access to privileged OS-level windowing APIs. If user gestures were not required, malicious advertisements could automatically spawn persistent, un-closable floating windows across the user's operating system desktop.
</details>

<details>
<summary><strong>2. How does the Document Picture-in-Picture API differ from the traditional HTMLVideoElement PiP API?</strong></summary>

The standard Picture-in-Picture API is strictly limited to streaming the video frames of a single `<video>` element. The **Document Picture-in-Picture API** (`window.documentPictureInPicture.requestWindow()`) opens an entire blank floating browser window that can hold arbitrary HTML DOM trees, CSS stylesheets, inputs, and canvas elements. This allows developers to build floating interactive UIs, such as video conferencing tiles with mute/unmute buttons and chat feeds.
</details>

<details>
<summary><strong>3. How does the Media Session API interact with Bluetooth headsets and hardware keyboards?</strong></summary>

The Media Session API exposes `navigator.mediaSession.setActionHandler()`. When a user presses a physical media key on their keyboard (e.g., Play/Pause), clicks a button on their Bluetooth headphones, or taps a wearable watch display, the operating system dispatches that hardware event to the browser, which fires the registered action callback in JavaScript.
</details>

<details>
<summary><strong>4. Why should you call <code>container.requestFullscreen()</code> on a wrapper <code>&lt;div&gt;</code> rather than the <code>&lt;video&gt;</code> element directly?</strong></summary>

If you request fullscreen on the bare `<video>` element, the browser engine replaces the entire viewport with its own native, unstyled video player. Any custom branded UI controls, interactive chapter menus, watermarks, or custom WebVTT subtitle styling hosted inside the parent wrapper are completely obscured. Calling fullscreen on the parent container keeps your custom UI controls visible.
</details>

<details>
<summary><strong>5. What event fires when a user exits Fullscreen mode by pressing the physical <code>Escape</code> key?</strong></summary>

The `fullscreenchange` event fires on the `document` object. Developers should listen for `fullscreenchange` and verify `if (!document.fullscreenElement)` to reset UI button states, labels, and layout classes.
</details>

<details>
<summary><strong>6. What happens to a video playing in PiP mode when the parent webpage tab is closed?</strong></summary>

Because the video stream is powered by the JavaScript execution environment and DOM of the originating webpage tab, closing the browser tab immediately terminates the video playback and destroys the floating Picture-in-Picture window.
</details>

<details>
<summary><strong>7. How can an embedded <code>&lt;iframe&gt;</code> be authorized to use the Fullscreen and PiP APIs?</strong></summary>

The parent document must explicitly delegate these capabilities to the iframe via the `allow` Permissions Policy attribute:
`<iframe src="..." allow="fullscreen; picture-in-picture"></iframe>`. Without these tokens, calls to `.requestFullscreen()` or `.requestPictureInPicture()` inside the iframe will reject with a `SecurityError`.
</details>

<details>
<summary><strong>8. What is the purpose of <code>navigator.mediaSession.playbackState</code>?</strong></summary>

It tells the operating system whether the media is currently `'playing'`, `'paused'`, or `'none'`. This allows the OS lock screen to toggle its native button icon accurately between the "Play" triangle and the "Pause" bars without guessing.
</details>

---

## 11. Practice Exercises

### Exercise 1: Toggle Picture-in-Picture Button (Easy)
- **Task**: Author an HTML `<video>` and a `<button>` that toggles the video in and out of native Picture-in-Picture mode. Handle feature detection.

<details>
<summary>View Solution</summary>

```html
<video id="sample-vid" controls width="600" height="340">
  <source src="/media/clip.mp4" type="video/mp4" />
</video>
<button type="button" id="pip-btn">Pop Out Video</button>

<script>
  const vid = document.getElementById('sample-vid');
  const btn = document.getElementById('pip-btn');

  if (document.pictureInPictureEnabled) {
    btn.addEventListener('click', async () => {
      if (document.pictureInPictureElement) {
        await document.exitPictureInPicture();
      } else {
        await vid.requestPictureInPicture();
      }
    });
  } else {
    btn.hidden = true;
  }
</script>
```
</details>

---

### Exercise 2: OS Lock Screen Metadata Configurator (Medium)
- **Task**: Write a JavaScript function that hooks into an `<audio>` player's `play` event, configuring `navigator.mediaSession` with a track title, artist, album, and a 512x512 artwork icon.

<details>
<summary>View Solution</summary>

```html
<audio id="music-player" controls src="/tracks/song.mp3"></audio>

<script>
  const player = document.getElementById('music-player');

  player.addEventListener('play', () => {
    if ('mediaSession' in navigator) {
      navigator.mediaSession.metadata = new MediaMetadata({
        title: 'Midnight Resonance',
        artist: 'Solaris Duo',
        album: 'Ambient Waves (Deluxe)',
        artwork: [
          { src: '/artwork/cover-512.jpg', sizes: '512x512', type: 'image/jpeg' }
        ]
      });

      navigator.mediaSession.setActionHandler('play', () => player.play());
      navigator.mediaSession.setActionHandler('pause', () => player.pause());
    }
  });
</script>
```
</details>

---

### Exercise 3: Complete Fullscreen Player Component with UI State Sync (Hard)
- **Task**: Build an accessible video player component with a custom fullscreen toggle button. Ensure:
  1. Fullscreen targets the parent container.
  2. Button label updates dynamically.
  3. Physical `Escape` key exit is handled gracefully via `fullscreenchange`.
  4. Styles adapt cleanly via `:fullscreen`.

<details>
<summary>View Solution</summary>

```html
<div class="custom-media-shell" id="media-shell">
  <video id="video-core" width="720" height="400" playsinline>
    <source src="/media/documentary.mp4" type="video/mp4" />
  </video>

  <div class="shell-controls">
    <button type="button" id="fs-toggle-btn" aria-label="Toggle Fullscreen Mode">
      Enter Fullscreen
    </button>
  </div>
</div>

<style>
  .custom-media-shell {
    position: relative;
    max-width: 720px;
    background: #000;
  }
  .custom-media-shell video {
    width: 100%;
    height: auto;
    display: block;
  }
  .shell-controls {
    padding: 0.5rem;
    background: #1e293b;
    color: #fff;
  }

  /* Fullscreen Mode Styling */
  .custom-media-shell:fullscreen {
    width: 100vw;
    height: 100vh;
    display: flex;
    flex-direction: column;
    justify-content: center;
  }
</style>

<script>
  const shell = document.getElementById('media-shell');
  const fsBtn = document.getElementById('fs-toggle-btn');

  fsBtn.addEventListener('click', async () => {
    if (!document.fullscreenElement) {
      await shell.requestFullscreen();
    } else {
      await document.exitFullscreen();
    }
  });

  document.addEventListener('fullscreenchange', () => {
    if (document.fullscreenElement === shell) {
      fsBtn.textContent = 'Exit Fullscreen';
    } else {
      fsBtn.textContent = 'Enter Fullscreen';
    }
  });
</script>
```
</details>

---

## 12. Summary / Cheat Sheet

```
+-------------------------------------------------------------------------------------------------+
|                                 MODERN MEDIA APIS CHEAT SHEET                                   |
+-------------------------------------------------------------------------------------------------+
| API / Method             | Purpose / Mechanism                                                  |
+-------------------------------------------------------------------------------------------------+
| video.requestPictureInPicture() Pops video out into native floating desktop window             |
| document.pictureInPictureElement Returns current floating video element                        |
| navigator.mediaSession   | Bridges web playback with OS lock screen & hardware media keys       |
| MediaMetadata            | Injects Title, Artist, Album, and Artwork icons into OS UI           |
| element.requestFullscreen() Expands element to fill entire monitor screen (Top Layer)           |
| :fullscreen pseudo-class | CSS selector targeting elements active in fullscreen mode            |
| getDisplayMedia()        | Captures live user screen share video stream                         |
| User gesture requirement | MANDATORY for PiP and Fullscreen triggers (prevents popup abuse)     |
+-------------------------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [W3C Picture-in-Picture Specification](https://w3c.github.io/picture-in-picture/)
- [W3C Media Session Standard](https://w3c.github.io/mediasession/)
- [WHATWG Fullscreen API Living Standard](https://fullscreen.spec.whatwg.org/)
- [MDN Web Docs: Document Picture-in-Picture API](https://developer.mozilla.org/en-US/docs/Web/API/Document_Picture-in-Picture_API)

---

[← Previous: Image Performance & Formats](10-image-performance-and-formats.md) | [Section Index](README.md) | [Next: Module 05 Head & SEO →](../05-head-metadata-and-seo/README.md)
