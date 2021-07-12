import { ipcMainInternal } from '@electron/internal/browser/ipc-main-internal';

type IPCHandler = (event: ElectronInternal.IpcMainInternalEvent, ...args: any[]) => any

export const handleSync = function <T extends IPCHandler> (channel: string, handler: T) {
  ipcMainInternal.on(channel, async (event, ...args) => {
    try {
      event.returnValue = [null, await handler(event, ...args)];
    } catch (error) {
      event.returnValue = [error];
    }
  });
};

let nextId = 0;

export function invokeInWebFrame<T> (senderFrame: Electron.WebFrameMain, command: string, ...args: any[]) {
  return new Promise<T>((resolve, reject) => {
    const requestId = ++nextId;
    const channel = `${command}_RESPONSE_${requestId}`;
    ipcMainInternal.on(channel, function handler (event, error: Error, result: any) {
      if (event.senderFrame !== senderFrame) {
        console.error(`Reply to ${command} sent by unexpected WebContents (${event.sender.id})`);
        return;
      }

      ipcMainInternal.removeListener(channel, handler);

      if (error) {
        reject(error);
      } else {
        resolve(result);
      }
    });

    senderFrame._sendInternal(command, requestId, ...args);
  });
}
