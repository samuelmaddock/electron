import { ipcMain, net } from 'electron/main';
import { fetchWithSession } from '@electron/internal/browser/api/net-fetch';
import { ipcMainInternal } from '@electron/internal/browser/ipc-main-internal';
const { fromPartition, fromPath, Session } = process._linkedBinding('electron_browser_session');
const { isDisplayMediaSystemPickerAvailable } = process._linkedBinding('electron_browser_desktop_capturer');

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

Session.prototype._init = function () {
  // Dispatch IPC messages to the ipc module.
  this.on('-ipc-message' as any, function (this: Electron.Session, event: Electron.IpcMainEvent, internal: boolean, channel: string, args: any[]) {
    // addSenderToEvent(event, this);
    if (internal) {
      ipcMainInternal.emit(channel, event, ...args);
    } else {
      // addReplyToEvent(event);
      this.emit('ipc-message', event, channel, ...args);
      // const maybeWebFrame = getWebFrameForEvent(event);
      // maybeWebFrame && maybeWebFrame.ipc.emit(channel, event, ...args);
      // ipc.emit(channel, event, ...args);
      ipcMain.emit(channel, event, ...args);
    }
  } as any);

  this.on('-ipc-invoke' as any, async function (this: Electron.WebContents, event: Electron.IpcMainInvokeEvent, internal: boolean, channel: string, args: any[]) {
    // addSenderToEvent(event, this);
    const replyWithResult = (result: any) => event._replyChannel.sendReply({ result });
    const replyWithError = (error: Error) => {
      console.error(`Error occurred in handler for '${channel}':`, error);
      event._replyChannel.sendReply({ error: error.toString() });
    };
    const targets: (ElectronInternal.IpcMainInternal| undefined)[] = internal ? [ipcMainInternal] : [ipcMain];
    const target = targets.find(target => target && (target as any)._invokeHandlers.has(channel));
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

  this.on('-ipc-message-sync' as any, function (this: Electron.WebContents, event: Electron.IpcMainEvent, internal: boolean, channel: string, args: any[]) {
    // addSenderToEvent(event, this);
    // addReturnValueToEvent(event);
    if (internal) {
      ipcMainInternal.emit(channel, event, ...args);
    } else {
      // addReplyToEvent(event);
      // this.emit('ipc-message-sync', event, channel, ...args);
      // maybeWebFrame && maybeWebFrame.ipc.emit(channel, event, ...args);
      // ipc.emit(channel, event, ...args);
      ipcMain.emit(channel, event, ...args);
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
