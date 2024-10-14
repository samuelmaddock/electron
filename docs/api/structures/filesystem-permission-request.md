# FilesystemPermissionRequest Object extends `BasePermissionRequest`

* `permission` string
  * `file-system` - Request access to read, write, and file management capabilities using the [File System API](https://developer.mozilla.org/en-US/docs/Web/API/File_System_API).
* `filePath` string (optional) - The path of the `fileSystem` request.
* `isDirectory` boolean (optional) - Whether the `fileSystem` request is a directory.
* `fileAccessType` string (optional) - The access type of the `fileSystem` request. Can be `writable` or `readable`.
