# ServiceWorkerMain

> Service worker

Process: [Main](../glossary.md#main-process)

## Class: ServiceWorkerMain

Process: [Main](../glossary.md#main-process)<br />
_This class is not exported from the `'electron'` module. It is only available as a return value of other methods in the Electron API._

### Instance Methods

#### `worker.isDestroyed()`

Returns `boolean` - Whether the service worker has been destroyed.

### Instance Properties

#### `worker.ipc` _Readonly_

An [`IpcMain`](ipc-main.md) instance scoped to the service worker.

#### `worker.scope` _Readonly_

A `string` representing the scope URL of the service worker.
