import { IpcMainImpl } from '@electron/internal/browser/ipc-main-impl';

const { ServiceWorkerMain, fromVersionId } = process._linkedBinding('electron_browser_service_worker_main');

Object.defineProperty(ServiceWorkerMain.prototype, 'ipc', {
  get () {
    const ipc = new IpcMainImpl();
    Object.defineProperty(this, 'ipc', { value: ipc });
    return ipc;
  }
});

export default {
  fromVersionId
};
