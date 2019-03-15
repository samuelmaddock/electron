import * as ipcRendererUtils from '@electron/internal/renderer/ipc-renderer-internal-utils'
import Event = require('@electron/internal/renderer/extensions/event')

type ChromeBrowserAction = typeof chrome.browserAction

class BrowserAction implements ChromeBrowserAction {
  onClicked: chrome.browserAction.BrowserClickedEvent = new Event() as any

  constructor (private extensionId: string) {
    // TODO: listen for browserAction clicked event
  }

  enable (tabId?: number) {}
  disable (tabId?: number) {}

  getTitle (details: chrome.browserAction.TabDetails, callback: (result: string) => void) {
    ipcRendererUtils.invoke<string>('CHROME_BROWSER_ACTION_GET_DETAIL', this.extensionId, 'title', details.tabId).then(callback)
  }
  setTitle (details: chrome.browserAction.TitleDetails) {
    ipcRendererUtils.invoke('CHROME_BROWSER_ACTION_SET_DETAILS', this.extensionId, details)
  }

  getBadgeText (details: chrome.browserAction.TabDetails, callback: (result: string) => void) {}
  setBadgeText (details: chrome.browserAction.BadgeTextDetails) {}

  getPopup (details: chrome.browserAction.TabDetails, callback: (result: string) => void) {}
  setPopup (details: chrome.browserAction.PopupDetails) {}

  getBadgeBackgroundColor (
    details: chrome.browserAction.TabDetails,
    callback: (result: chrome.browserAction.ColorArray) => void
  ) {}
  setBadgeBackgroundColor (details: chrome.browserAction.BadgeBackgroundColorDetails) {}

  setIcon (details: chrome.browserAction.TabIconDetails, callback?: Function) {}
}

export const setup = (extensionId: string) => {
  return new BrowserAction(extensionId)
}
