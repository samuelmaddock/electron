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
  const hasTimeout = true;
  const { id, ok } = this._startExternalRequest(hasTimeout);
  if (!ok) {
    throw new Error('Unable to start service worker request');
  }

  // TODO: Promise.withResolvers
  let onResolve: Function;
  const promise = new Promise<void>((resolve) => {
    onResolve = resolve;
  });

  promise.finally(() => {
    this._finishExternalRequest(id);
  });

  return {
    complete () { onResolve(); }
  };
};

module.exports = ServiceWorkerMain;
