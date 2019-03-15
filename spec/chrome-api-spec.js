const fs = require('fs')
const path = require('path')

const { expect } = require('chai')
const { remote } = require('electron')

const { closeWindow } = require('./window-helpers')
const { emittedOnce } = require('./events-helpers')

const { BrowserWindow, webContents } = remote

describe('chrome api', () => {
  const fixtures = path.resolve(__dirname, 'fixtures')
  let w
  let bg

  before(() => {
    BrowserWindow.addExtension(path.join(fixtures, 'extensions/chrome-api'))
  })

  after(() => {
    BrowserWindow.removeExtension('chrome-api')
  })

  beforeEach(() => {
    w = new BrowserWindow({
      show: false
    })
    bg = webContents.getAllWebContents().find(wc => wc.getType() === 'backgroundPage')
  })

  afterEach(() => closeWindow(w).then(() => { w = bg = null }))

  const dispatchTest = (wc, name) => wc.executeJavaScript(`window.postMessage('${name}', '*')`)

  it('runtime.getManifest returns extension manifest', async () => {
    const actualManifest = (() => {
      const data = fs.readFileSync(path.join(fixtures, 'extensions/chrome-api/manifest.json'), 'utf-8')
      return JSON.parse(data)
    })()

    w.loadURL('about:blank')

    const p = emittedOnce(w.webContents, 'console-message')
    dispatchTest(w.webContents, 'getManifest')
    const [,, manifestString] = await p
    const manifest = JSON.parse(manifestString)

    expect(manifest.name).to.equal(actualManifest.name)
    expect(manifest.content_scripts.length).to.equal(actualManifest.content_scripts.length)
  })

  it.only('browserAction can set and get title', async () => {
    w.loadURL('about:blank')
    const p = emittedOnce(bg, 'console-message')
    dispatchTest(bg, 'browserActionTitle')
    const [,, title] = await p
    expect(title).to.equal('yeet')
  })
})
