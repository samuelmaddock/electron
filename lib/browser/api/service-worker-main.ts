import { IpcMainImpl } from '@electron/internal/browser/ipc-main-impl';

const { ServiceWorkerMain } = process._linkedBinding('electron_browser_service_worker_main');

Object.defineProperty(ServiceWorkerMain.prototype, 'ipc', {
  get () {
    const ipc = new IpcMainImpl();
    Object.defineProperty(this, 'ipc', { value: ipc });
    return ipc;
  }
});

ServiceWorkerMain.prototype.send = function (channel, ...args) {
  if (typeof channel !== 'string') {
    throw new TypeError('Missing required channel argument');
  }

  try {
    return this._send(false /* internal */, channel, args);
  } catch (e) {
    console.error('Error sending from ServiceWorkerMain: ', e);
  }
};

ServiceWorkerMain.prototype.startTask = function () {
  // TODO(samuelmaddock): make timeout configurable
  const hasTimeout = false;
  const { id, ok } = this._startExternalRequest(hasTimeout);

  // TODO(samuelmaddock): refactor to use Promise.withResolvers after upgrading
  // types.
  let onResolve: Function;
  let onReject: Function;
  const promise = new Promise<void>((resolve, reject) => {
    onResolve = resolve;
    onReject = reject;

    if (!ok) {
      onReject(new Error('Unable to start service worker task.'));
    }
  });

  promise.finally(() => {
    this._finishExternalRequest(id);
  });

  return {
    complete () { onResolve(); }
  };
};

module.exports = ServiceWorkerMain;
