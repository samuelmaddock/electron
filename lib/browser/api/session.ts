import { net } from 'electron/main';
import { fetchWithSession } from '@electron/internal/browser/api/net-fetch';
import { ipcMainInternal } from '@electron/internal/browser/ipc-main-internal';
import type { ServiceWorkerMain } from 'electron/main';
import { MessagePortMain } from '@electron/internal/browser/message-port-main';
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

const addReturnValueToEvent = (event: Electron.IpcMainEvent) => {
  Object.defineProperty(event, 'returnValue', {
    set: (value) => event._replyChannel.sendReply(value),
    get: () => {}
  });
};

Session.prototype._init = function () {
  const getWorkerFromEvent = (event: any): ServiceWorkerMain => {
    const worker = (this.serviceWorkers as any)._fromVersionIDIfExists((event as any).versionId);
    return worker;
  };

  this.on('-ipc-message' as any, function (event: Electron.IpcMainEvent, channel: string, args: any[]) {
    const internal = v8Util.getHiddenValue<boolean>(event, 'internal');

    if (internal) {
      ipcMainInternal.emit(channel, event, ...args);
    } else if ((event as any).type === 'service-worker') {
      getWorkerFromEvent(event)?.ipc.emit(channel, event, ...args);
    }
  } as any);

  this.on('-ipc-invoke' as any, async function (event: Electron.IpcMainInvokeEvent, channel: string, args: any[]) {
    const internal = v8Util.getHiddenValue<boolean>(event, 'internal');

    const replyWithResult = (result: any) => event._replyChannel.sendReply({ result });
    const replyWithError = (error: Error) => {
      console.error(`Error occurred in handler for '${channel}':`, error);
      event._replyChannel.sendReply({ error: error.toString() });
    };

    const workerIpc = getWorkerFromEvent(event)?.ipc;
    const targets: (ElectronInternal.IpcMainInternal| undefined)[] = internal ? [ipcMainInternal] : [workerIpc];
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

  this.on('-ipc-message-sync' as any, function (event: Electron.IpcMainEvent, channel: string, args: any[]) {
    const internal = v8Util.getHiddenValue<boolean>(event, 'internal');
    addReturnValueToEvent(event);
    if (internal) {
      ipcMainInternal.emit(channel, event, ...args);
    } else if ((event as any).type === 'service-worker') {
      getWorkerFromEvent(event)?.ipc.emit(channel, event, ...args);
    }
  } as any);

  this.on('-ipc-ports' as any, function (event: Electron.IpcMainEvent, channel: string, message: any, ports: any[]) {
    event.ports = ports.map(p => new MessagePortMain(p));
    if ((event as any).type === 'service-worker') {
      getWorkerFromEvent(event)?.ipc.emit(channel, event, message);
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

export default {
  fromPartition,
  fromPath,
  get defaultSession () {
    return fromPartition('');
  }
};
