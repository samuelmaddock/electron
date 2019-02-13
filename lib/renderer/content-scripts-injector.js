'use strict'

const ipcRenderer = require('@electron/internal/renderer/ipc-renderer-internal')
// const { runInThisContext } = require('vm')
const { webFrame } = require('electron')

// https://cs.chromium.org/chromium/src/content/public/common/isolated_world_ids.h?q=ISOLATED_WORLD&sq=package:chromium&l=13&dr=CSs
const IsolatedWorldIDs = {
  // Lowest isolated world ID
  ISOLATED_WORLD_ID_EXTENSIONS: 10
}

let isolatedWorldIds = IsolatedWorldIDs.ISOLATED_WORLD_ID_EXTENSIONS

// https://cs.chromium.org/chromium/src/extensions/renderer/script_injection.cc?type=cs&sq=package:chromium&g=0&l=52
const getIsolatedWorldIdForInstance = () => {
  // TODO(samuelmaddock): allocate and cleanup IDs
  return isolatedWorldIds++
}

// Check whether pattern matches.
// https://developer.chrome.com/extensions/match_patterns
const matchesPattern = function (pattern) {
  if (pattern === '<all_urls>') return true
  const regexp = new RegExp(`^${pattern.replace(/\*/g, '.*')}$`)
  const url = `${location.protocol}//${location.host}${location.pathname}`
  return url.match(regexp)
}

// Run the code with chrome API integrated.
const runContentScript = function (extensionId, url, code) {
  // const context = {}
  // require('@electron/internal/renderer/chrome-api').injectTo(extensionId, false, context)
  const wrapper = `((chrome) => {\n  ${code}\n  })`
  // const compiledWrapper = runInThisContext(wrapper, {
  //   filename: url,
  //   lineOffset: 1,
  //   displayErrors: true
  // })
  // return compiledWrapper.call(this, context.chrome)
  const sources = [
    {
      code: wrapper,
      url
    }
  ]

  const worldId = getIsolatedWorldIdForInstance()
  webFrame.setIsolatedWorldHumanReadableName(worldId, extensionId)
  // webFrame.setIsolatedWorldContentSecurityPolicy(worldId, csp)
  webFrame.executeJavaScriptInIsolatedWorld(worldId, sources)
}

const runAllContentScript = function (scripts, extensionId) {
  for (const { url, code } of scripts) {
    runContentScript.call(window, extensionId, url, code)
  }
}

const runStylesheet = function (url, code) {
  // const wrapper = `((code) => {
  //   function init() {
  //     const styleElement = document.createElement('style');
  //     styleElement.textContent = code;
  //     document.head.append(styleElement);
  //   }
  //   document.addEventListener('DOMContentLoaded', init);
  // })`

  // try {
  //   const compiledWrapper = runInThisContext(wrapper, {
  //     filename: url,
  //     lineOffset: 1,
  //     displayErrors: true
  //   })
  //   return compiledWrapper.call(this, code)
  // } catch (error) {
  //   // TODO(samuelmaddock): Insert stylesheet directly into document, see chromium script_injection.cc
  //   console.error(`Error inserting content script stylesheet ${url}`)
  //   console.error(error)
  // }
}

const runAllStylesheet = function (css) {
  for (const { url, code } of css) {
    runStylesheet.call(window, url, code)
  }
}

// Run injected scripts.
// https://developer.chrome.com/extensions/content_scripts
const injectContentScript = function (extensionId, script) {
  if (!script.matches.some(matchesPattern)) return

  if (script.js) {
    const fire = runAllContentScript.bind(window, script.js, extensionId)
    if (script.runAt === 'document_start') {
      process.once('document-start', fire)
    } else if (script.runAt === 'document_end') {
      process.once('document-end', fire)
    } else {
      document.addEventListener('DOMContentLoaded', fire)
    }
  }

  if (script.css) {
    const fire = runAllStylesheet.bind(window, script.css)
    if (script.runAt === 'document_start') {
      process.once('document-start', fire)
    } else if (script.runAt === 'document_end') {
      process.once('document-end', fire)
    } else {
      document.addEventListener('DOMContentLoaded', fire)
    }
  }
}

// Handle the request of chrome.tabs.executeJavaScript.
ipcRenderer.on('CHROME_TABS_EXECUTESCRIPT', function (event, senderWebContentsId, requestId, extensionId, url, code) {
  const result = runContentScript.call(window, extensionId, url, code)
  ipcRenderer.sendToAll(senderWebContentsId, `CHROME_TABS_EXECUTESCRIPT_RESULT_${requestId}`, result)
})

module.exports = (getRenderProcessPreferences) => {
  // Read the renderer process preferences.
  const preferences = getRenderProcessPreferences()
  if (preferences) {
    for (const pref of preferences) {
      if (pref.contentScripts) {
        for (const script of pref.contentScripts) {
          injectContentScript(pref.extensionId, script)
        }
      }
    }
  }
}
