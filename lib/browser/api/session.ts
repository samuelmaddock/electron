import { fetchWithSession } from '@electron/internal/browser/api/net-fetch';
import { net } from 'electron/main';
import * as deprecate from '@electron/internal/common/deprecate';
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
};

// Add an extra warning to inform developers of permission string change.
const clipboardReadDeprecated = deprecate.warnOnceMessage('"clipboard-read" permission has been deprecated. Please use \'session.setPermissionHandlers\' instead.');

/** Rewrite new permission to old permission. */
const maybeRewritePermission = (permission: any): any => {
  if (permission === 'clipboard-read-write') {
    clipboardReadDeprecated();
    return 'clipboard-read';
  }
  return permission;
};

const setPermissionCheckHandlerDeprecated = deprecate.warnOnce('session.setPermissionCheckHandler', 'session.setPermissionHandlers');
Session.prototype.setPermissionCheckHandler = function (handler) {
  setPermissionCheckHandlerDeprecated();
  if (!handler) return this._setPermissionCheckHandler(handler);

  return this._setPermissionCheckHandler((webContents, permission, ...args) => {
    permission = maybeRewritePermission(permission);
    if (handler(webContents, permission, ...args)) return 'granted';
    return 'denied';
  });
};

const setPermissionRequestHandlerDeprecated = deprecate.warnOnce('session.setPermissionRequestHandlerDeprecated', 'session.setPermissionHandlers');
Session.prototype.setPermissionRequestHandler = function (handler) {
  setPermissionRequestHandlerDeprecated();
  if (!handler) return this._setPermissionRequestHandler(handler);

  return this._setPermissionRequestHandler((wc, permission, cb: any, d, eo) => {
    permission = maybeRewritePermission(permission);
    return handler(wc, permission, (granted) => cb(granted ? 'granted' : 'denied'), d, eo);
  });
};

Session.prototype.setPermissionHandlers = function (handlers) {
  if (!handlers) {
    this._setPermissionCheckHandler(null);
    this._setPermissionRequestHandler(null);
    return;
  }

  if (typeof handlers !== 'object') {
    throw new TypeError('Missing required handlers argument');
  } else if (typeof handlers.isGranted !== 'function') {
    throw new TypeError('Expected handlers object to contain a \'isGranted\' function property');
  } else if (typeof handlers.onRequest !== 'function') {
    throw new TypeError('Expected handlers object to contain a \'onRequest\' function property');
  }

  this._setPermissionCheckHandler((_, permission: any, effectiveOrigin, details) => {
    return handlers.isGranted(permission, effectiveOrigin, details).status;
  });

  this._setPermissionRequestHandler((_, permission: any, callback, details, effectiveOrigin) => {
    Promise.resolve(handlers.onRequest(permission, effectiveOrigin, details))
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
