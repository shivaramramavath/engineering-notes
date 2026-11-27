# Media Accessibility: WCAG 2.2 AA, Audio Descriptions, and Transcripts

| Metadata | Details |
| :--- | :--- |
| **Specification** | W3C WCAG 2.2 (Guidelines 1.2.1, 1.2.2, 1.2.3, 1.2.4, 1.2.5, 1.4.2, 2.2.2, 2.3.1) |
| **Prerequisites** | `04-media-and-embeds/01-audio-and-video.md`, `04-media-and-embeds/07-webvtt-and-subtitles.md` |
| **Estimated Reading Time** | 26 minutes |
| **Status** | Production Ready |
| **Baseline Status** | Widely Available (Captions, Transcripts, Accessible Controls, Pause Mechanisms) |
| **Browsers Tested** | Chrome 124+, Firefox 125+, Safari 17.4+, Edge 124+ |
| **Tested With** | W3C Nu HTML Validator, axe-core 4.9, NVDA 2024.1, JAWS 2024, VoiceOver macOS 14.4 |

---

## Table of Contents
1. [What & Why](#1-what--why)
2. [Core Concepts](#2-core-concepts)
3. [How It Works Internally](#3-how-it-works-internally)
4. [Syntax & Usage](#4-syntax--usage)
5. [Practical Examples](#5-practical-examples)
   - [Example 1: Audio Podcast with Synchronized Interactive Transcript](#example-1-audio-podcast-with-synchronized-interactive-transcript)
   - [Example 2: Video Player with Secondary Audio Description Track](#example-2-video-player-with-secondary-audio-description-track)
   - [Example 3: Autoplaying Ambient Video with Mandatory Accessible Pause Control](#example-3-autoplaying-ambient-video-with-mandatory-accessible-pause-control)
   - [Example 4: Sign Language Video Integration](#example-4-sign-language-video-integration)
   - [Example 5: Flashing and Seizure Safety Protection](#example-5-flashing-and-seizure-safety-protection)
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

Multimedia content—video demonstrations, recorded lectures, podcasts, webinars, and hero animations—is among the most engaging content on the modern web. However, without deliberate accessibility engineering, multimedia erects insurmountable barriers:
- **Blind and low-vision users** miss visual context: on-screen charts, gestures, silent physical demonstrations, and software UI movements.
- **Deaf and hard-of-hearing users** miss spoken dialogue, music, emotional intonation, and crucial sound effects.
- **Users with vestibular disorders or epilepsy** can suffer dizziness, nausea, or life-threatening seizures from uncontrolled motion and strobe flashing.
- **Neurodivergent users or non-native speakers** require multimodal reinforcement (reading while listening) to comprehend complex technical information.

Web Content Accessibility Guidelines (WCAG) 2.2 establishes unequivocal legal and technical requirements for time-based media. Achieving compliance is not simply about adding a caption track; it requires a holistic architecture encompassing synchronized closed captions, audio descriptions, full interactive transcripts, seizure-safe framerates, and user-controlled pause mechanisms.

---

## 2. Core Concepts

### The WCAG 2.2 Time-Based Media Matrix

| Criterion | Level | Requirement Summary | HTML / Web Implementation |
| :--- | :--- | :--- | :--- |
| **1.2.1 Audio-only and Video-only** | **A** | Prerecorded audio must have a text transcript. Prerecorded silent video must have audio narration or a text transcript. | Linked HTML transcript or `<track kind="descriptions">`. |
| **1.2.2 Captions (Prerecorded)** | **A** | Synchronized closed captions for all dialogue and non-speech sounds in video. | `<track kind="captions" default>`. |
| **1.2.3 Audio Description or Alternative** | **A** | Audio description of visual actions OR a complete text transcript of all dialogue and visuals. | Secondary audio track or comprehensive transcript. |
| **1.2.4 Captions (Live)** | **AA** | Real-time synchronized captions for live broadcasts. | WebRTC / HLS embedded 608/708 or WebVTT live streams. |
| **1.2.5 Audio Description (Prerecorded)** | **AA** | **Mandatory audio description** narrated during natural pauses in dialogue. | `<video>` with selectable descriptive audio track. |
| **1.4.2 Audio Control** | **A** | If audio plays automatically for > 3 seconds, provide a pause/stop/mute control. | Visible pause button independent of system volume. |
| **2.2.2 Pause, Stop, Hide** | **A** | Any moving, blinking, or scrolling content lasting > 5 seconds must have a pause control. | Pause/Play toggle button on background hero video. |
| **2.3.1 Three Flashes or Below** | **A** | Content must not flash more than 3 times per second. | Video editing compliance / animation threshold caps. |

### Audio Descriptions: What Are They?
Audio Description (AD) provides an additional narration track that describes vital visual elements that cannot be understood from the main soundtrack alone—such as character movements, facial expressions, scene changes, clothing, and on-screen text. The narrator speaks during natural pauses between spoken dialogue.

---

## 3. How It Works Internally

```
+-----------------------------------------------------------------------------------------+
|                  Accessible Multimedia Architecture Pipeline                            |
+-----------------------------------------------------------------------------------------+

  Video Playback Timeline
  00:00 ---------- 00:05 ---------- 00:10 ---------- 00:15 ---------- 00:20
  Dialogue:        "Look at that!"                   "We must run!"
  Visual Action:   [Actor points to dark storm cloud forming on mountain]
                          |
                          v
  ACCESSIBILITY INTEGRATION:
  
  1. Captions Track (<track kind="captions">):
     00:05: <v John>Look at that!
     00:07: [Ominous thunder rumbles in distance]
     00:15: <v Sarah>We must run!
  
  2. Audio Description Track (Secondary Audio / WebVTT Descriptions):
     00:07 - 00:12: (Spoken in pause): "A massive purple lightning storm envelops the peak."
  
  3. Interactive HTML Transcript:
     HTML document mirrors both dialogue and visual descriptions in synchronized text!
```

---

## 4. Syntax & Usage

```html
<figure class="accessible-video">
  <video controls width="1280" height="720" playsinline aria-describedby="video-transcript-summary">
    <source src="/media/training.mp4" type="video/mp4" />
    
    <!-- Synchronized Captions (WCAG 1.2.2) -->
    <track 
      kind="captions" 
      src="/media/training-captions-en.vtt" 
      srclang="en" 
      label="English [CC]" 
      default 
    />

    <!-- Synchronized Audio Descriptions (WCAG 1.2.5) -->
    <track 
      kind="descriptions" 
      src="/media/training-descriptions-en.vtt" 
      srclang="en" 
      label="Audio Descriptions" 
    />
  </video>

  <figcaption id="video-transcript-summary">
    Laboratory Safety Protocol: Chemical Disposal Procedures.
    <a href="#full-transcript">Jump to Interactive Text Transcript</a>.
  </figcaption>
</figure>
```

> **Cross-Browser & Baseline Status Box**
>
> - `<track kind="captions">`: **Baseline Widely Available** across all browsers.
> - Multiple selectable audio tracks (`AudioTrackList`): Supported natively in Safari; polyfilled via Web Audio API or streaming players (HLS.js, Video.js) in Chromium and Firefox.
> - Interactive HTML transcripts: **Baseline Widely Available** using standard DOM and `HTMLMediaElement.currentTime`.

---

## 5. Practical Examples

### Example 1: Audio Podcast with Synchronized Interactive Transcript
The gold standard for podcast accessibility: provides full audio playback linked to an interactive transcript where clicking any sentence seeks the player directly to that timestamp.

```html
<section class="podcast-widget" aria-labelledby="podcast-heading">
  <h2 id="podcast-heading">Episode 12: Building for Everyone</h2>

  <!-- Audio Player with No Preload to Save Mobile Data -->
  <audio id="podcast-player" controls preload="metadata" class="media-player">
    <source src="/audio/ep12.mp3" type="audio/mpeg" />
    <p>Your browser cannot play audio. <a href="/audio/ep12.mp3">Download MP3</a>.</p>
  </audio>

  <!-- Interactive Transcript satisfying WCAG 1.2.1 -->
  <div class="transcript-box">
    <h3>Interactive Transcript</h3>
    <p class="transcript-help">Click any paragraph to jump directly to that point in the audio.</p>

    <div id="transcript-content" class="transcript-body">
      <p data-start="0" tabindex="0" class="transcript-cue">
        <strong>Host (00:00):</strong> Welcome back to Engineering Horizons. Today we are joined by Dr. Maya Lin to discuss digital inclusion.
      </p>
      <p data-start="8.5" tabindex="0" class="transcript-cue">
        <strong>Dr. Lin (00:08):</strong> Thank you, happy to be here. Accessibility is often misunderstood as a checklist rather than a design principle.
      </p>
      <p data-start="18.0" tabindex="0" class="transcript-cue">
        <strong>Host (00:18):</strong> Exactly. When we design for extreme constraints, everyone benefits—like closed captions in noisy airports.
      </p>
    </div>
  </div>
</section>

<style>
  .transcript-box {
    margin-top: 1.5rem;
    padding: 1rem;
    border: 1px solid #cbd5e1;
    border-radius: 8px;
    background-color: #f8fafc;
  }
  .transcript-cue {
    padding: 0.5rem;
    border-radius: 4px;
    cursor: pointer;
  }
  .transcript-cue:hover, .transcript-cue:focus-visible {
    background-color: #e2e8f0;
    outline: 2px solid #0284c7;
  }
  .transcript-cue.active {
    background-color: #e0f2fe;
    border-left: 4px solid #0284c7;
  }
</style>

<script>
  const player = document.getElementById('podcast-player');
  const cues = document.querySelectorAll('.transcript-cue');

  // Click on transcript seeks player
  cues.forEach(cue => {
    cue.addEventListener('click', () => {
      player.currentTime = parseFloat(cue.dataset.start);
      player.play();
    });
    // Keyboard Enter/Space activation
    cue.addEventListener('keydown', (e) => {
      if (e.key === 'Enter' || e.key === ' ') {
        e.preventDefault();
        player.currentTime = parseFloat(cue.dataset.start);
        player.play();
      }
    });
  });

  // Highlight active cue as audio plays
  player.addEventListener('timeupdate', () => {
    const time = player.currentTime;
    cues.forEach((cue, index) => {
      const start = parseFloat(cue.dataset.start);
      const nextCue = cues[index + 1];
      const end = nextCue ? parseFloat(nextCue.dataset.start) : Infinity;

      if (time >= start && time < end) {
        cue.classList.add('active');
      } else {
        cue.classList.remove('active');
      }
    });
  });
</script>
```

---

### Example 2: Video Player with Secondary Audio Description Track
Providing two video tracks: standard mix vs. audio-described mix satisfying WCAG 1.2.5 (Level AA).

```html
<div class="described-player-container">
  <video id="feature-video" controls width="960" height="540" playsinline>
    <!-- Default Video Source -->
    <source id="video-source" src="/media/nature-standard.mp4" type="video/mp4" />
    
    <track 
      kind="captions" 
      src="/media/nature-captions.vtt" 
      srclang="en" 
      label="English [CC]" 
      default 
    />
  </video>

  <!-- Audio Description Toggle Control -->
  <div class="player-toolbar">
    <button type="button" id="toggle-ad-btn" aria-pressed="false">
      Turn Audio Description ON
    </button>
    <span id="ad-status" role="status" class="visually-hidden">Audio descriptions off</span>
  </div>
</div>

<script>
  const vid = document.getElementById('feature-video');
  const adBtn = document.getElementById('toggle-ad-btn');
  const adStatus = document.getElementById('ad-status');
  const source = document.getElementById('video-source');

  let isAdActive = false;

  adBtn.addEventListener('click', () => {
    isAdActive = !isAdActive;
    const currentTime = vid.currentTime;
    const isPaused = vid.paused;

    // Switch video source between standard and audio-described versions
    source.src = isAdActive ? '/media/nature-described.mp4' : '/media/nature-standard.mp4';
    vid.load();
    vid.currentTime = currentTime;

    if (!isPaused) vid.play();

    adBtn.setAttribute('aria-pressed', String(isAdActive));
    adBtn.textContent = isAdActive ? 'Turn Audio Description OFF' : 'Turn Audio Description ON';
    adStatus.textContent = isAdActive ? 'Audio descriptions active' : 'Audio descriptions off';
  });
</script>
```

---

### Example 3: Autoplaying Ambient Video with Mandatory Accessible Pause Control
WCAG Criterion 2.2.2 requires that any content that moves automatically for more than 5 seconds must have a mechanism for the user to pause, stop, or hide it.

```html
<section class="ambient-hero">
  <video 
    id="hero-ambient-vid" 
    autoplay 
    muted 
    loop 
    playsinline 
    class="bg-video" 
    aria-hidden="true" 
    tabindex="-1"
  >
    <source src="/media/clouds.mp4" type="video/mp4" />
  </video>

  <!-- MANDATORY WCAG 2.2.2 / 1.4.2 User Pause/Play Control -->
  <button 
    type="button" 
    id="pause-motion-btn" 
    class="motion-toggle-btn"
    aria-label="Pause background video animation"
    aria-pressed="false"
  >
    <span aria-hidden="true">⏸ Pause Motion</span>
  </button>

  <div class="hero-text">
    <h1>Sustainable Cloud Infrastructure</h1>
    <p>Zero carbon emission compute clusters.</p>
  </div>
</section>

<script>
  const ambientVid = document.getElementById('hero-ambient-vid');
  const pauseBtn = document.getElementById('pause-motion-btn');

  pauseBtn.addEventListener('click', () => {
    if (ambientVid.paused) {
      ambientVid.play();
      pauseBtn.setAttribute('aria-pressed', 'false');
      pauseBtn.setAttribute('aria-label', 'Pause background video animation');
      pauseBtn.firstElementChild.textContent = '⏸ Pause Motion';
    } else {
      ambientVid.pause();
      pauseBtn.setAttribute('aria-pressed', 'true');
      pauseBtn.setAttribute('aria-label', 'Resume background video animation');
      pauseBtn.firstElementChild.textContent = '▶ Resume Motion';
    }
  });

  // Respect OS-level reduced motion preferences automatically
  if (window.matchMedia('(prefers-reduced-motion: reduce)').matches) {
    ambientVid.pause();
    pauseBtn.setAttribute('aria-pressed', 'true');
    pauseBtn.firstElementChild.textContent = '▶ Resume Motion';
  }
</script>
```

---

### Example 4: Sign Language Video Integration
WCAG Criterion 1.2.6 (Level AAA, strongly recommended for public sector portals): embedding a synchronized Picture-in-Picture sign language interpreter.

```html
<div class="video-multiview">
  <div class="main-video-box">
    <video id="primary-video" controls width="800" height="450">
      <source src="/media/state-address.mp4" type="video/mp4" />
      <track kind="captions" src="/media/address-en.vtt" srclang="en" label="English" default />
    </video>
  </div>

  <!-- Synchronized Sign Language Interpretation Window -->
  <div class="sign-language-box" id="sign-box">
    <video id="sign-video" width="240" height="180" muted playsinline aria-label="American Sign Language Interpretation">
      <source src="/media/address-asl.mp4" type="video/mp4" />
    </video>
  </div>
</div>

<script>
  const primary = document.getElementById('primary-video');
  const sign = document.getElementById('sign-video');

  // Keep sign language video strictly locked in sync with primary video
  primary.addEventListener('play', () => sign.play());
  primary.addEventListener('pause', () => sign.pause());
  primary.addEventListener('seeking', () => {
    sign.currentTime = primary.currentTime;
  });
</script>
```

---

### Example 5: Flashing and Seizure Safety Protection
Preventing Photosensitive Seizures (WCAG 2.3.1): capping automated video animations and enforcing warnings.

```html
<!-- Seizure Warning for unavoidable archival or scientific footage -->
<div class="safety-advisory-box" role="alert">
  <h3>Photosensitivity Warning</h3>
  <p>
    The following historical video contains strobe lighting and rapid light-dark flashes 
    that may trigger seizures in individuals with photosensitive epilepsy.
  </p>
  <button type="button" id="consent-btn">I understand, reveal video player</button>
</div>

<div id="guarded-video-wrapper" hidden>
  <video id="flashing-archive-vid" controls width="800" height="450">
    <source src="/media/archival-lightning.mp4" type="video/mp4" />
  </video>
</div>

<script>
  document.getElementById('consent-btn').addEventListener('click', () => {
    document.getElementById('guarded-video-wrapper').hidden = false;
    document.querySelector('.safety-advisory-box').hidden = true;
  });
</script>
```

---

## 6. Best Practices

- [ ] **Always pair prerecorded audio with an HTML text transcript (WCAG 1.2.1)**: Transcripts benefit deaf users, people in noisy or quiet settings, and optimize search engine indexability.
- [ ] **Ensure captions include non-speech auditory cues (WCAG 1.2.2)**: Include background noises, sound effects (`[Explosion in distance]`), music descriptors (`[Upbeat jazz music]`), and speaker identification.
- [ ] **Provide Audio Descriptions for visual content (WCAG 1.2.5)**: Narrate visual information (charts, text on screen, gestures) during natural dialogue pauses.
- [ ] **Provide a visible pause control on all ambient/hero videos (WCAG 2.2.2)**: Autoplaying videos lasting over 5 seconds must have a pause button that keyboard and screen reader users can trigger.
- [ ] **Respect `prefers-reduced-motion`**: Automatically pause background videos or disable animations if the user has requested reduced motion in their operating system settings.
- [ ] **Ensure custom video controls are 100% keyboard operable (WCAG 2.1.1)**: Every play, pause, scrub, volume, and fullscreen toggle must be an accessible `<button>` operable via `Space` and `Enter`.

---

## 7. Common Mistakes & Pitfalls

| Mistake | Severity | Consequence | Correction |
| :--- | :--- | :--- | :--- |
| Providing subtitles instead of closed captions | **Critical** | Hearing-impaired users miss crucial sound effects and music context (violates WCAG 1.2.2). | Use `kind="captions"` and transcribe auditory cues. |
| Background video with no pause button | **Critical** | Users with ADHD, autism, or cognitive fatigue cannot read page content (violates WCAG 2.2.2). | Add an accessible Pause/Play toggle button. |
| Autoplaying audio with no independent volume/mute | **Critical** | Interferes with screen reader speech synthesis, stranding blind users (violates WCAG 1.4.2). | Do not autoplay audio; provide immediate pause controls. |
| Posting a podcast without a text transcript | **High** | Deaf users have zero access to the spoken content (violates WCAG 1.2.1). | Publish a full HTML text transcript alongside the player. |
| Omitting Audio Descriptions on instructional videos | **High** | Blind users cannot follow software tutorials or visual physical demonstrations (violates WCAG 1.2.5). | Provide an audio-described track or comprehensive transcript. |
| Videos with rapid strobe flashes (> 3 flashes/sec) | **Critical** | Can trigger seizures in individuals with photosensitive epilepsy (violates WCAG 2.3.1). | Edit footage to reduce flash frequency or filter luminance. |

---

## 8. Performance, Accessibility, SEO & Security Notes

### Accessibility (WCAG 2.2 AA)
- Media accessibility is legal compliance in many jurisdictions (ADA Title III, Section 508 in the US, European Accessibility Act EN 301 549). Missing captions or transcripts on government, education, or commercial portals can lead to legal liability.

### Search Engine Optimization (SEO)
- Search engines cannot "watch" video files directly. Providing full text transcripts and WebVTT captions enables Googlebot to index every spoken word, dramatically increasing organic search discovery and keyword reach.

---

## 9. Comparison / When to Use What

| Accessibility Asset | Primary Target Audience | Technical Format | When Required? |
| :--- | :--- | :--- | :--- |
| **Captions** | Deaf & hard of hearing, sound-off viewers | WebVTT (`kind="captions"`) | **Mandatory** for all pre-recorded video with speech (WCAG 1.2.2). |
| **Audio Descriptions** | Blind & low-vision viewers | Secondary audio track / WebVTT | **Mandatory** for pre-recorded video with visual actions (WCAG 1.2.5). |
| **Full Text Transcript** | All users, search crawlers | HTML document (`<p>`, `<table>`) | **Mandatory** for podcasts/audio (1.2.1); best practice for video. |
| **Sign Language Video** | Native Deaf signers | Secondary `<video>` stream | Recommended for government & public announcements (WCAG 1.2.6). |

---

## 10. Interview Questions

<details>
<summary><strong>1. What is the difference between closed captions and audio descriptions?</strong></summary>

- **Closed Captions** convert auditory information into text for users who cannot hear the soundtrack. They display spoken dialogue, speaker changes, and non-speech sound effects (`[Thunder cracks]`, `[Applause]`).
- **Audio Descriptions** convert visual information into spoken audio for users who cannot see the video. A narrator describes actions, scene changes, facial expressions, and text on screen during natural dialogue pauses.
</details>

<details>
<summary><strong>2. What does WCAG 2.2 Criterion 1.4.2 (Audio Control) mandate for web developers?</strong></summary>

If any audio plays automatically on a webpage for more than 3 seconds, the website must provide an easily accessible mechanism to pause or stop the audio, or to adjust the volume independently from the user's overall system volume. This prevents webpage audio from speaking over and drowning out screen reader voice synthesis (such as NVDA or VoiceOver).
</details>

<details>
<summary><strong>3. What does WCAG Criterion 2.2.2 (Pause, Stop, Hide) require for hero background videos?</strong></summary>

Any moving, blinking, or scrolling content that starts automatically, lasts more than 5 seconds, and is presented in parallel with other content must have a mechanism for the user to pause, stop, or hide it. Autoplaying hero background videos must include an accessible, visible Pause/Play toggle button.
</details>

<details>
<summary><strong>4. What is the Flash Threshold under WCAG Criterion 2.3.1 (Three Flashes or Below)?</strong></summary>

Web pages must not contain anything that flashes more than three times in any one-second period, or the flash must be below the general flash and red flash thresholds. Violating this threshold can trigger epileptic seizures in photosensitive individuals.
</details>

<details>
<summary><strong>5. How does an interactive transcript provide a better user experience than a static text file?</strong></summary>

An interactive transcript syncs with the media player: as the audio or video plays, JavaScript highlights the current spoken paragraph in real time. Users can also click on any sentence or keyword in the transcript to jump the media player directly to that exact timestamp, greatly assisting research, navigation, and cognitive comprehension.
</details>

<details>
<summary><strong>6. Does providing an interactive transcript satisfy both WCAG 1.2.1 and WCAG 1.2.3 at Level A?</strong></summary>

Yes. For audio-only media (like podcasts), an accurate text transcript completely satisfies Criterion 1.2.1. For prerecorded video, a comprehensive descriptive transcript (including all dialogue and descriptions of all visual actions) satisfies Criterion 1.2.3 (Level A alternative to audio descriptions). However, Level AA (Criterion 1.2.5) strictly requires synchronized audio descriptions.
</details>

<details>
<summary><strong>7. How can you programmatically respect a user's operating system motion settings for media?</strong></summary>

By querying the CSS media feature `prefers-reduced-motion` in JavaScript:
```javascript
if (window.matchMedia('(prefers-reduced-motion: reduce)').matches) {
  videoElement.pause();
}
```
If active, background videos should default to paused or display a static fallback image.
</details>

<details>
<summary><strong>8. Why should caption text tracks always be hosted or configured with proper CORS headers?</strong></summary>

Browser security rules enforce that `<track>` elements loaded across domains must pass CORS checks (`crossorigin="anonymous"` on `<video>` and `Access-Control-Allow-Origin: *` on the server). If CORS is misconfigured, the browser will refuse to load the text track, completely disabling captions for users who depend on them.
</details>

---

## 11. Practice Exercises

### Exercise 1: Accessible Ambient Hero with Pause Button (Easy)
- **Task**: Author an HTML snippet containing an autoplaying, muted background video with an accessible Pause/Play button labeled via `aria-label` and `aria-pressed`.

<details>
<summary>View Solution</summary>

```html
<div class="hero-media-box">
  <video id="ambient-video" autoplay muted loop playsinline aria-hidden="true" tabindex="-1">
    <source src="/media/atmosphere.mp4" type="video/mp4" />
  </video>

  <button 
    type="button" 
    id="media-pause-toggle" 
    class="motion-btn"
    aria-label="Pause background video animation"
    aria-pressed="false"
  >
    Pause Video
  </button>
</div>

<script>
  const video = document.getElementById('ambient-video');
  const btn = document.getElementById('media-pause-toggle');

  btn.addEventListener('click', () => {
    if (video.paused) {
      video.play();
      btn.setAttribute('aria-pressed', 'false');
      btn.setAttribute('aria-label', 'Pause background video animation');
      btn.textContent = 'Pause Video';
    } else {
      video.pause();
      btn.setAttribute('aria-pressed', 'true');
      btn.setAttribute('aria-label', 'Resume background video animation');
      btn.textContent = 'Play Video';
    }
  });
</script>
```
</details>

---

### Exercise 2: Interactive Transcript Seeking (Medium)
- **Task**: Write HTML and JavaScript for an audio player with 2 transcript sentences. Clicking a sentence or pressing Enter on it must update `audio.currentTime` and start playback.

<details>
<summary>View Solution</summary>

```html
<audio id="demo-audio" controls>
  <source src="/audio/interview.mp3" type="audio/mpeg" />
</audio>

<div class="transcript">
  <p class="transcript-line" data-time="0" tabindex="0">
    00:00 - Good morning and welcome to the team meeting.
  </p>
  <p class="transcript-line" data-time="5.5" tabindex="0">
    00:05 - Let's review our quarterly accessibility goals.
  </p>
</div>

<script>
  const audio = document.getElementById('demo-audio');
  document.querySelectorAll('.transcript-line').forEach(line => {
    function jump() {
      audio.currentTime = parseFloat(line.dataset.time);
      audio.play();
    }
    line.addEventListener('click', jump);
    line.addEventListener('keydown', (e) => {
      if (e.key === 'Enter') jump();
    });
  });
</script>
```
</details>

---

### Exercise 3: Complete WCAG 2.2 AA Compliance Audit (Hard)
- **Task**: Review the following markup and list all 4 WCAG accessibility failures. Provide the fully corrected production code:
  ```html
  <video autoplay loop width="600" height="400">
    <source src="announcement.mp4" type="video/mp4">
  </video>
  ```

<details>
<summary>View Solution</summary>

**Accessibility Failures Identified:**
1. **WCAG 1.2.2 Failure**: Missing `<track kind="captions">` for spoken dialogue and sound effects.
2. **WCAG 2.2.2 Failure**: Video autoplays and loops indefinitely without any mechanism to pause or stop it.
3. **WCAG 1.4.2 Failure**: If the video contains audio, unmuted autoplay interferes with screen readers.
4. **WCAG 2.1.1 Failure**: Missing native `controls` attribute or accessible keyboard buttons.

**Fully Corrected Production Markup:**
```html
<figure class="accessible-video-player">
  <video 
    id="main-announcement" 
    controls 
    preload="metadata" 
    width="600" 
    height="400" 
    playsinline
    aria-labelledby="announcement-title"
  >
    <source src="announcement.mp4" type="video/mp4" />
    
    <!-- Synchronized Captions (WCAG 1.2.2) -->
    <track 
      kind="captions" 
      src="announcement-captions.vtt" 
      srclang="en" 
      label="English [CC]" 
      default 
    />

    <p>
      Your browser cannot play video. 
      <a href="announcement.mp4">Download MP4</a> or read the 
      <a href="announcement-transcript.html">full transcript</a>.
    </p>
  </video>

  <figcaption id="announcement-title">
    Company Quarterly Address: Strategic Roadmap for 2026.
    <a href="announcement-transcript.html">View complete text transcript</a>.
  </figcaption>
</figure>
```
</details>

---

## 12. Summary / Cheat Sheet

```
+-------------------------------------------------------------------------------------------------+
|                                 MEDIA ACCESSIBILITY CHEAT SHEET                                 |
+-------------------------------------------------------------------------------------------------+
| WCAG Criterion           | Requirement                             | Implementation             |
+-------------------------------------------------------------------------------------------------+
| 1.2.1 Audio/Video Only   | Transcript for audio; narration for video| <p> transcript link        |
| 1.2.2 Captions (Pre)     | Dialogue + non-speech sounds in video   | <track kind="captions">    |
| 1.2.5 Audio Description  | Narrated visual action during pauses    | Secondary audio track      |
| 1.4.2 Audio Control      | Audio > 3s must have pause/volume       | Independent mute/pause btn |
| 2.2.2 Pause, Stop, Hide  | Movement > 5s must have pause button    | Accessible Pause/Play btn  |
| 2.3.1 Three Flashes      | No content flashing > 3 times/sec       | Capped animation frequency |
| prefers-reduced-motion   | Respect OS motion disable preferences   | matchMedia JS listener     |
+-------------------------------------------------------------------------------------------------+
```

---

## 13. Further Reading

- [W3C Web Content Accessibility Guidelines (WCAG) 2.2](https://www.w3.org/TR/WCAG22/)
- [W3C WAI: Making Audio and Video Media Accessible](https://www.w3.org/WAI/media/av/)
- [W3C Understanding WCAG 2.2: Guideline 1.2 Time-based Media](https://www.w3.org/WAI/WCAG22/Understanding/time-based-media.html)
- [WebAIM: Captions, Transcripts, and Audio Descriptions](https://webaim.org/techniques/captions/)

---

[← Previous: Object & Embed](08-object-and-embed.md) | [Section Index](README.md) | [Next: Image Performance & Formats →](10-image-performance-and-formats.md)
