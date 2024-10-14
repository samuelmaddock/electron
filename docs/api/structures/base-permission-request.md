# BasePermissionRequest Object

* `effectiveOrigin` String - The origin of the URL of the permission check, you should use this origin to perform security checks on whether to accept or deny this permission check. You may augment checks with additional information, but this is the primary source of truth you should rely on.
* `requestingUrl` string - The last URL the requesting frame loaded.
* `isMainFrame` boolean - Whether the frame making the request is the main frame.
