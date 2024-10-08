import { net } from 'electron/main';
import { fetchWithSession } from '@electron/internal/browser/api/net-fetch';
import { ipcMainInternal } from '@electron/internal/browser/ipc-main-internal';
import type { ServiceWorkerMain } from 'electron/main';
import { MessagePortMain } from '@electron/internal/browser/message-port-main';
import * as deprecate from '@electron/internal/common/deprecate';
const { fromPartition, fromPath, Session } = process._linkedBinding('electron_browser_session');
const { isDisplayMediaSystemPickerAvailable } = process._linkedBinding('electron_browser_desktop_capturer');

const v8Util = process._linkedBinding('electron_common_v8_util');

// Fake video window that activates the native system picker
// This is used to get around the need for a screen/window
// id in Chrome's desktopCapturer.
let fakeVideoWindowId = -1;
// See content/public/browser/desktop_media_id.h
const kMacOsNativePickerId = -4;
const systemPickerVideoSource = Object.create(null);
Object.defineProperty(systemPickerVideoSource, 'id', {
  get () {
    return `window:${kMacOsNativePickerId}:${fakeVideoWindowId--}`;
  }
});
systemPickerVideoSource.name = '';
Object.freeze(systemPickerVideoSource);

const addReturnValueToEvent = (event: Electron.IpcMainEvent | Electron.IpcMainServiceWorkerEvent) => {
  Object.defineProperty(event, 'returnValue', {
    set: (value) => event._replyChannel.sendReply(value),
    get: () => {}
  });
};

Session.prototype._init = function () {
  const getServiceWorkerFromEvent = (event: Electron.IpcMainServiceWorkerEvent | Electron.IpcMainServiceWorkerInvokeEvent): ServiceWorkerMain | null => {
    return this.serviceWorkers._fromVersionIDIfExists(event.versionId);
  };
  const addServiceWorkerPropertyToEvent = (event: Electron.IpcMainServiceWorkerEvent | Electron.IpcMainServiceWorkerInvokeEvent) => {
    Object.defineProperty(event, 'serviceWorker', {
      get: () => this.serviceWorkers.fromVersionID(event.versionId)
    });
  };

  this.on('-ipc-message' as any, function (event: Electron.IpcMainEvent | Electron.IpcMainServiceWorkerEvent, channel: string, args: any[]) {
    const internal = v8Util.getHiddenValue<boolean>(event, 'internal');

    if (internal) {
      ipcMainInternal.emit(channel, event, ...args);
    } else if (event.type === 'service-worker') {
      addServiceWorkerPropertyToEvent(event);
      getServiceWorkerFromEvent(event)?.ipc.emit(channel, event, ...args);
    }
  } as any);

  this.on('-ipc-invoke' as any, async function (event: Electron.IpcMainInvokeEvent | Electron.IpcMainServiceWorkerInvokeEvent, channel: string, args: any[]) {
    const internal = v8Util.getHiddenValue<boolean>(event, 'internal');

    const replyWithResult = (result: any) => event._replyChannel.sendReply({ result });
    const replyWithError = (error: Error) => {
      console.error(`Error occurred in handler for '${channel}':`, error);
      event._replyChannel.sendReply({ error: error.toString() });
    };

    const targets: (Electron.IpcMainServiceWorker | ElectronInternal.IpcMainInternal | undefined)[] = [];

    if (internal) {
      targets.push(ipcMainInternal);
    } else if (event.type === 'service-worker') {
      addServiceWorkerPropertyToEvent(event);
      const workerIpc = getServiceWorkerFromEvent(event)?.ipc;
      targets.push(workerIpc);
    }

    const target = targets.find(target => (target as any)?._invokeHandlers.has(channel));
    if (target) {
      const handler = (target as any)._invokeHandlers.get(channel);
      try {
        replyWithResult(await Promise.resolve(handler(event, ...args)));
      } catch (err) {
        replyWithError(err as Error);
      }
    } else {
      replyWithError(new Error(`No handler registered for '${channel}'`));
    }
  } as any);

  this.on('-ipc-message-sync' as any, function (event: Electron.IpcMainEvent | Electron.IpcMainServiceWorkerEvent, channel: string, args: any[]) {
    const internal = v8Util.getHiddenValue<boolean>(event, 'internal');
    addReturnValueToEvent(event);
    if (internal) {
      ipcMainInternal.emit(channel, event, ...args);
    } else if (event.type === 'service-worker') {
      addServiceWorkerPropertyToEvent(event);
      getServiceWorkerFromEvent(event)?.ipc.emit(channel, event, ...args);
    }
  } as any);

  this.on('-ipc-ports' as any, function (event: Electron.IpcMainEvent | Electron.IpcMainServiceWorkerEvent, channel: string, message: any, ports: any[]) {
    event.ports = ports.map(p => new MessagePortMain(p));
    if (event.type === 'service-worker') {
      addServiceWorkerPropertyToEvent(event);
      getServiceWorkerFromEvent(event)?.ipc.emit(channel, event, message);
    }
  } as any);
};

Session.prototype.fetch = function (input: RequestInfo, init?: RequestInit) {
  return fetchWithSession(input, init, this, net.request);
};

Session.prototype.setDisplayMediaRequestHandler = function (handler, opts) {
  if (!handler) return this._setDisplayMediaRequestHandler(handler, opts);

  this._setDisplayMediaRequestHandler(async (req, callback) => {
    if (opts && opts.useSystemPicker && isDisplayMediaSystemPickerAvailable()) {
      return callback({ video: systemPickerVideoSource });
    }

    return handler(req, callback);
  }, opts);
};

const getPreloadsDeprecated = deprecate.warnOnce('session.getPreloads', 'session.getPreloadScripts');
Session.prototype.getPreloads = function () {
  getPreloadsDeprecated();
  return this.getPreloadScripts().filter(script => script.type === 'frame').map(script => script.filePath);
};

const setPreloadsDeprecated = deprecate.warnOnce('session.setPreloads', 'session.setPreloadScripts');
Session.prototype.setPreloads = function (preloads) {
  setPreloadsDeprecated();
  this.setPreloadScripts(preloads.map(filePath => ({ type: 'frame', filePath })));
};

export default {
  fromPartition,
  fromPath,
  get defaultSession () {
    return fromPartition('');
  }
};
