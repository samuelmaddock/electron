/* global chrome */

const testMap = {
  browserActionTitle () {
    chrome.browserAction.setTitle('yeet')
    chrome.browserAction.getTitle({}, console.log)
  }
}

const dispatchTest = event => {
  const testName = event.data
  const test = testMap[testName]
  test()
}
window.addEventListener('message', dispatchTest, false)
