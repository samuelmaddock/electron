import * as ipcMainUtils from '@electron/internal/browser/ipc-main-internal-utils';
import { MessagePortMain } from '@electron/internal/browser/message-port-main';
import { IPC_MESSAGES } from '@electron/internal/common/ipc-messages';
import { webFrameMethods } from '@electron/internal/common/web-frame-methods';

const { WebFrameMain, fromId } = process._linkedBinding('electron_browser_web_frame_main');

WebFrameMain.prototype.send = function (channel, ...args) {
  if (typeof channel !== 'string') {
    throw new Error('Missing required channel argument');
  }

  return this._send(false /* internal */, channel, args);
};

WebFrameMain.prototype._sendInternal = function (channel, ...args) {
  if (typeof channel !== 'string') {
    throw new Error('Missing required channel argument');
  }

  return this._send(true /* internal */, channel, args);
};

WebFrameMain.prototype.postMessage = function (...args) {
  if (Array.isArray(args[2])) {
    args[2] = args[2].map(o => o instanceof MessagePortMain ? o._internalPort : o);
  }
  this._postMessage(...args);
};

for (const method of webFrameMethods) {
  WebFrameMain.prototype[method] = function (...args: any[]): Promise<any> {
    return ipcMainUtils.invokeInWebFrame(this, IPC_MESSAGES.RENDERER_WEB_FRAME_METHOD, method, ...args);
  };
}

export default {
  fromId
};
