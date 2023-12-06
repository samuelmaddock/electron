import * as events from 'events';

// import type * as ipcRendererInternalModule from '@electron/internal/renderer/ipc-renderer-internal';
// import { IPC_MESSAGES } from '../common/ipc-messages';

declare const binding: {
  get: (name: string) => any;
  process: NodeJS.Process;
  createPreloadScript: (src: string) => Function
};

const { EventEmitter } = events;

process._linkedBinding = binding.get;

const v8Util = process._linkedBinding('electron_common_v8_util');
// Expose Buffer shim as a hidden value. This is used by C++ code to
// deserialize Buffer instances sent from browser process.
v8Util.setHiddenValue(global, 'Buffer', Buffer);
// The process object created by webpack is not an event emitter, fix it so
// the API is more compatible with non-sandboxed renderers.
for (const prop of Object.keys(EventEmitter.prototype) as (keyof typeof process)[]) {
  if (Object.hasOwn(process, prop)) {
    delete process[prop];
  }
}
Object.setPrototypeOf(process, EventEmitter.prototype);

// const { ipcRendererInternal } = require('@electron/internal/renderer/ipc-renderer-internal') as typeof ipcRendererInternalModule;
// const ipcRendererUtils = require('@electron/internal/renderer/ipc-renderer-internal-utils') as typeof ipcRendererUtilsModule;

// Test message
// const {
//   preloadScripts,
//   process: processProps
// } = ipcRendererUtils.invokeSync<{
//   preloadScripts: {
//     preloadPath: string;
//     preloadSrc: string | null;
//     preloadError: null | Error;
//   }[];
//   process: NodeJS.Process;
// }>(IPC_MESSAGES.BROWSER_SANDBOX_LOAD);

// console.log('***preloadRealm', {
//   preloadScripts,
//   processProps,
// });

const electron = require('electron');

const loadedModules = new Map<string, any>([
  ['electron', electron],
  ['electron/common', electron],
  ['events', events],
  ['node:events', events]
]);

const loadableModules = new Map<string, Function>([
  // ['timers', () => require('timers')],
  // ['node:timers', () => require('timers')],
  ['url', () => require('url')],
  ['node:url', () => require('url')]
]);

// This is the `require` function that will be visible to the preload script
function preloadRequire (module: string) {
  if (loadedModules.has(module)) {
    return loadedModules.get(module);
  }
  if (loadableModules.has(module)) {
    const loadedModule = loadableModules.get(module)!();
    loadedModules.set(module, loadedModule);
    return loadedModule;
  }
  throw new Error(`module not found: ${module}`);
}

// TODO: remove this
(globalThis as any).test = {
  process,
  require: preloadRequire
};
