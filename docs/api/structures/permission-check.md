# PermissionCheck Object

* `permission` string
  * `clipboard-read-write` - Request access to read from the clipboard or write custom data.
  * `clipboard-sanitized-write` - Request access to write vetted data (such as plain text or sanitized images) to the clipboard.
  * `geolocation` - Access the user's geolocation data via the [Geolocation API](https://developer.mozilla.org/en-US/docs/Web/API/Geolocation_API)
  * `fullscreen` - Control of the app's fullscreen state via the [Fullscreen API](https://developer.mozilla.org/en-US/docs/Web/API/Fullscreen_API).
  * `hid` - Access the HID protocol to manipulate HID devices via the [WebHID API](https://developer.mozilla.org/en-US/docs/Web/API/WebHID_API).
  * `idle-detection` - Access the user's idle state via the [IdleDetector API](https://developer.mozilla.org/en-US/docs/Web/API/IdleDetector).
  * `media-key-system` - Access to DRM protected content.
  * `midi` - Enable MIDI access in the [Web MIDI API](https://developer.mozilla.org/en-US/docs/Web/API/Web_MIDI_API).
  * `midi-sysex` - Use system exclusive messages in the [Web MIDI API](https://developer.mozilla.org/en-US/docs/Web/API/Web_MIDI_API).
  * `notifications` - Configure and display desktop notifications to the user with the [Notifications API](https://developer.mozilla.org/en-US/docs/Web/API/notification).
  * `open-external` - Open links in external applications.
  * `pointer-lock` - Directly interpret mouse movements as an input method via the [Pointer Lock API](https://developer.mozilla.org/en-US/docs/Web/API/Pointer_Lock_API). These requests always appear to originate from the main frame.
  * `serial` - Read from and write to serial devices with the [Web Serial API](https://developer.mozilla.org/en-US/docs/Web/API/Web_Serial_API).
  * `storage-access` - Allows content loaded in a third-party context to request access to third-party cookies using the [Storage Access API](https://developer.mozilla.org/en-US/docs/Web/API/Storage_Access_API).
  * `top-level-storage-access` -  Allow top-level sites to request third-party cookie access on behalf of embedded content originating from another site in the same related website set using the [Storage Access API](https://developer.mozilla.org/en-US/docs/Web/API/Storage_Access_API).
  * `usb` - Expose non-standard Universal Serial Bus (USB) compatible devices services to the web with the [WebUSB API](https://developer.mozilla.org/en-US/docs/Web/API/WebUSB_API).
* `effectiveOrigin` string - The origin of the URL of the permission check, you should use this origin to perform security checks on whether to accept or deny this permission check. You may augment checks with additional information, but this is the primary source of truth you should rely on.
* `embeddingOrigin` string (optional) - The origin of the frame embedding the frame that made the permission check.  Only set for cross-origin sub frames making permission checks.
* `isMainFrame` boolean (optional) - Whether the frame making the request is the main frame. This value is `undefined` in cases where the request is coming from a background worker and therefore is not related to a specific frame.
