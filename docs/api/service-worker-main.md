# ServiceWorkerMain

> Service worker

Process: [Main](../glossary.md#main-process)

## Class: ServiceWorkerMain

Process: [Main](../glossary.md#main-process)<br />
_This class is not exported from the `'electron'` module. It is only available as a return value of other methods in the Electron API._

### Instance Methods

#### `worker.isDestroyed()`

Returns `boolean` - Whether the service worker has been destroyed.

#### `worker.send(channel, ...args)`

* `channel` string
* `...args` any[]

Send an asynchronous message to the renderer process via `channel`, along with
arguments. Arguments will be serialized with the [Structured Clone Algorithm][SCA],
just like [`postMessage`][], so prototype chains will not be included.
Sending Functions, Promises, Symbols, WeakMaps, or WeakSets will throw an exception.

The renderer process can handle the message by listening to `channel` with the
[`ipcRenderer`](ipc-renderer.md) module.

### Instance Properties

#### `worker.ipc` _Readonly_

An [`IpcMainServiceWorker`](ipc-main-service-worker.md) instance scoped to the service worker.

#### `worker.scope` _Readonly_

A `string` representing the scope URL of the service worker.

[SCA]: https://developer.mozilla.org/en-US/docs/Web/API/Web_Workers_API/Structured_clone_algorithm
[`postMessage`]: https://developer.mozilla.org/en-US/docs/Web/API/Window/postMessage
