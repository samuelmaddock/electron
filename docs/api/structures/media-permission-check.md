# MediaPermissionCheck Object

* `permission` string
  * `media` - Access to media devices such as camera, microphone and speakers.
* `effectiveOrigin` string - The origin of the URL of the permission check, you should use this origin to perform security checks on whether to accept or deny this permission check. You may augment checks with additional information, but this is the primary source of truth you should rely on.
* `embeddingOrigin` string (optional) - The origin of the frame embedding the frame that made the permission check.  Only set for cross-origin sub frames making permission checks.
* `isMainFrame` boolean (optional) - Whether the frame making the request is the main frame. This value is `undefined` in cases where the request is coming from a background worker and therefore is not related to a specific frame.
* `mediaType` string - The type of media access being requested, can be `video`, `audio` or `unknown`
