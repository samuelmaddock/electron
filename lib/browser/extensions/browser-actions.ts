import { app, webContents } from 'electron'
import { ipcMainInternal } from '@electron/internal/browser/ipc-main-internal'
import ipcMainUtils = require('@electron/internal/browser/ipc-main-internal-utils')

type Details =
  | chrome.browserAction.BadgeBackgroundColorDetails
  | chrome.browserAction.BadgeTextDetails
  | chrome.browserAction.PopupDetails
  | chrome.browserAction.TabDetails
  | chrome.browserAction.TabIconDetails
  | chrome.browserAction.TitleDetails

type ExtensionDetails = { [key: string]: any }

type ExtensionMap = {
  [extensionId: string]:
    | {
        /** Defaults */
        details: ExtensionDetails
        /** Tab-specific details. */
        tabs: { [tabId: number]: ExtensionDetails | undefined }
      }
    | undefined
}

// ublock: setIcon, setBadgeBackgroundColor, setBadgeText, setTitle, onClicked,

// https://github.com/brave/muon/blob/master/lib/browser/api/browser-actions.js
/**
 * Manages state for chrome.browserActions API.
 */
class BrowserActions {
  private extensions: ExtensionMap = {}

  private createTabDetail(extensionId: string, tabId: number) {
    const ext = this.extensions[extensionId]!
    const tab = (ext.tabs[tabId] = {})

    const wc = webContents.getAllWebContents().find(c => c.id === tabId)!
    wc.once('destroyed', () => {
      delete ext.tabs[tabId]
    })

    return tab
  }

  setupExtension(extensionId: string, manifest: chrome.runtime.Manifest) {
    const { browser_action } = manifest
    if (browser_action) {
      const defaults = {
        title: browser_action.default_title,
        icon: browser_action.default_icon,
        popup: browser_action.default_popup,
      }
      this.extensions[extensionId] = { details: defaults, tabs: {} }
    }
  }
  
  setDetails(extensionId: string, { tabId, ...details }: Details) {
    const ext = this.extensions[extensionId] || (this.extensions[extensionId] = { details: {}, tabs: {} })
    let target
    if (typeof tabId === 'number') {
      target = ext.tabs[tabId] || this.createTabDetail(extensionId, tabId)
    } else {
      target = ext.details
    }
    Object.assign(target, details)
  }

  getDetail(extensionId: string, detail: string, tabId?: number) {
    const ext = this.extensions[extensionId]
    if (!ext) return
    const target = typeof tabId === 'number' ? ext.tabs[tabId] : ext.details
    if (!target) return
    return target[detail]
  }
}

export function setup() {
  const browserActions = new BrowserActions()

  // TODO: types
  app.on('chrome-extension-ready' as any, (info: any) => {
    browserActions.setupExtension(info.id, info.manifest)
  })
  
  ipcMainInternal.on('CHROME_BROWSER_ACTION_SET_DETAILS', (event, extensionId, details) => {
    if (typeof extensionId === 'string' && typeof details === 'object') {
      browserActions.setDetails(extensionId, details)
    }
  })

  ipcMainUtils.handle('CHROME_BROWSER_ACTION_GET_DETAIL', (event, extensionId, detail, tabId) => {
    if (typeof extensionId === 'string' && typeof detail === 'string') {
      const value = browserActions.getDetail(extensionId, detail, tabId)
      return value
    }
  })

  ipcMainInternal.on('CHROME_BROWSER_ACTION_CLICKED', (event, extensionId) => {
    // TODO(samuelmaddock): ?
  })
  
  return browserActions
}
