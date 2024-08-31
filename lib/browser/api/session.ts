import { fetchWithSession } from '@electron/internal/browser/api/net-fetch';
import { net } from 'electron/main';
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
}

Session.prototype.setPermissionCheckHandler = function (handler) {
  if (!handler) return this._setPermissionCheckHandler(handler);

  return this._setPermissionCheckHandler((...args) => {
    if (handler(...args)) return 'granted';
    return 'denied';
  });
};

Session.prototype.setPermissionRequestHandler = function (handler) {
  if (!handler) return this._setPermissionRequestHandler(handler);

  return this._setPermissionRequestHandler((wc, perm, cb: any, d, eo) => {
    return handler(wc, perm, (granted) => cb(granted ? 'granted' : 'denied'), d, eo);
  });
};

Session.prototype.setPermissionHandlers = function (handlers) {
  if (!handlers) {
    this._setPermissionCheckHandler(null);
    this._setPermissionRequestHandler(null);
    return;
  }

  this._setPermissionCheckHandler((_, permission, effectiveOrigin, details) => {
    return handlers.isGranted(permission, effectiveOrigin, details).status;
  });

  this._setPermissionRequestHandler((_, permission, callback, details, effectiveOrigin) => {
    handlers.onRequest(permission, effectiveOrigin, details)
      .then((result) => callback(result.status === 'granted'))
      .catch((err) => {
        this.emit('error', err);
        callback(false);
      });
  });
};

export default {
  fromPartition,
  fromPath,
  get defaultSession () {
    return fromPartition('');
  }
};
