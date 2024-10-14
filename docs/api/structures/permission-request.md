# PermissionRequest  Object extends `BasePermissionRequest`

* `permission` string
  * `clipboard-read-write` - Request access to read from the clipboard or write custom data.
  * `clipboard-sanitized-write` - Request access to write vetted data (such as plain text or sanitized images) to the clipboard.
  * `display-capture` - Request access to capture the screen via the [Screen Capture API](https://developer.mozilla.org/en-US/docs/Web/API/Screen_Capture_API).
  * `fullscreen` - Request control of the app's fullscreen state via the [Fullscreen API](https://developer.mozilla.org/en-US/docs/Web/API/Fullscreen_API).
  * `geolocation` - Request access to the user's location via the [Geolocation API](https://developer.mozilla.org/en-US/docs/Web/API/Geolocation_API)
  * `idle-detection` - Request access to the user's idle state via the [IdleDetector API](https://developer.mozilla.org/en-US/docs/Web/API/IdleDetector).
  * `media-key-system` - Request access to DRM protected content.
  * `midi` - Request MIDI access in the [Web MIDI API](https://developer.mozilla.org/en-US/docs/Web/API/Web_MIDI_API).
  * `midi-sysex` - Request the use of system exclusive messages in the [Web MIDI API](https://developer.mozilla.org/en-US/docs/Web/API/Web_MIDI_API).
  * `notifications` - Request notification creation and the ability to display them in the user's system tray using the [Notifications API](https://developer.mozilla.org/en-US/docs/Web/API/notification)
  * `pointer-lock` - Request to directly interpret mouse movements as an input method via the [Pointer Lock API](https://developer.mozilla.org/en-US/docs/Web/API/Pointer_Lock_API). These requests always appear to originate from the main frame.
  * `keyboard-lock` - Request capture of keypresses for any or all of the keys on the physical keyboard via the [Keyboard Lock API](https://developer.mozilla.org/en-US/docs/Web/API/Keyboard/lock). These requests always appear to originate from the main frame.
  * `speaker-selection` - Request to enumerate and select audio output devices via the [speaker-selection permissions policy](https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Permissions-Policy/speaker-selection).
  * `storage-access` - Allows content loaded in a third-party context to request access to third-party cookies using the [Storage Access API](https://developer.mozilla.org/en-US/docs/Web/API/Storage_Access_API).
  * `top-level-storage-access` -  Allow top-level sites to request third-party cookie access on behalf of embedded content originating from another site in the same related website set using the [Storage Access API](https://developer.mozilla.org/en-US/docs/Web/API/Storage_Access_API).
  * `window-management` - Request access to enumerate screens using the [`getScreenDetails`](https://developer.chrome.com/en/articles/multi-screen-window-placement/) API.
  * `unknown` - An unrecognized permission request.
