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

/** Map new permission to deprecated permission. */
type NewPermission = Parameters<Electron.PermissionHandlers['isGranted']>[0] | Parameters<Electron.PermissionHandlers['onRequest']>[0];
const deprecatedPermissionMap = new Map<NewPermission, string>([
  ['clipboard-read-write', 'clipboard-read'],
  ['keyboard-lock', 'keyboardLock'],
  ['midi-sysex', 'midiSysex'],
  ['pointer-lock', 'pointerLock'],
  ['media-key-system', 'mediaKeySystem'],
  ['open-external', 'openExternal'],
  ['file-system', 'fileSystem']
]);

/** Rewrite new permission to old permission. */
const maybeRewritePermission = (permission: any): any => {
  return deprecatedPermissionMap.get(permission) || permission;
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

const isHandlersObject = (handlers: unknown): handlers is Electron.PermissionHandlers => {
  if (typeof handlers !== 'object') {
    throw new TypeError('Missing required handlers argument');
  } else if (typeof (handlers as any).isGranted !== 'function') {
    throw new TypeError('Expected handlers object to contain a \'isGranted\' function property');
  } else if (typeof (handlers as any).onRequest !== 'function') {
    throw new TypeError('Expected handlers object to contain a \'onRequest\' function property');
  }
  return true;
};
const isPermissionResult = (result: unknown): result is Electron.PermissionCheckResult => {
  if (typeof result !== 'object' || !result || typeof (result as any).status !== 'string') {
    console.warn('Expected permission result to be an object with status string property. Assuming \'denied\' instead.');
    return false;
  }
  return true;
};

Session.prototype.setPermissionHandlers = function (handlers: unknown) {
  if (!handlers) {
    this._setPermissionCheckHandler(null);
    this._setPermissionRequestHandler(null);
    return;
  }

  if (!isHandlersObject(handlers)) return;

  this._setPermissionCheckHandler((_, permission: any, effectiveOrigin, details) => {
    const result: unknown = handlers.isGranted(permission, effectiveOrigin, details);
    if (!isPermissionResult(result)) return 'denied';
    return result.status;
  });

  this._setPermissionRequestHandler((_, permission: any, callback, details, effectiveOrigin) => {
    Promise.resolve(handlers.onRequest(permission, effectiveOrigin, details))
      .then((result: unknown) => {
        if (!isPermissionResult(result)) return callback(false);
        callback(result.status === 'granted');
      })
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
