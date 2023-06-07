/* global binding */
import * as events from 'events';

const { EventEmitter } = events;

process._linkedBinding = binding.get;

const v8Util = process._linkedBinding('electron_common_v8_util');
// Expose Buffer shim as a hidden value. This is used by C++ code to
// deserialize Buffer instances sent from browser process.
v8Util.setHiddenValue(global, 'Buffer', Buffer);
// The process object created by webpack is not an event emitter, fix it so
// the API is more compatible with non-sandboxed renderers.
for (const prop of Object.keys(EventEmitter.prototype) as (keyof typeof process)[]) {
  if (Object.prototype.hasOwnProperty.call(process, prop)) {
    delete process[prop];
  }
}
Object.setPrototypeOf(process, EventEmitter.prototype);

const electron = require('electron');

const loadedModules = new Map<string, any>([
  ['electron', electron],
  ['electron/common', electron],
  ['events', events]
]);

const loadableModules = new Map<string, Function>([
  ['timers', () => require('timers')],
  ['url', () => require('url')]
]);

Object.assign(process, binding.process);

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

(globalThis as any).require = preloadRequire;

console.log('Worker internal script done');
console.log(`Object.keys(globalThis)=${Object.keys(globalThis).join(', ')}`);
console.log(`globalThis.setTimeout=${globalThis.setTimeout}`);
console.log('electron=', require('electron'));
console.log('timers=', require('timers'));

console.log('***starting');
require('timers').setTimeout(() => {
  console.log('***ended');
}, 5000);
