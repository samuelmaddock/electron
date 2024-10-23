const { contextBridge, ipcRenderer } = require('electron');

let result;
try {
  result = contextBridge.evaluateInMainWorld(`({
    chromeType: typeof chrome,
    id: globalThis.chrome?.runtime.id,
    manifest: globalThis.chrome?.runtime.getManifest(),
  })`);
} catch (error) {
  console.error(error);
}

ipcRenderer.invoke('preload-extension-result', result);
