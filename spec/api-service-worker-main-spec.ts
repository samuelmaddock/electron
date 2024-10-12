import { session, webContents as webContentsModule, WebContents } from 'electron/main';

import { expect } from 'chai';

import { once, on } from 'node:events';
import * as fs from 'node:fs';
import * as http from 'node:http';
import * as path from 'node:path';

import { listen } from './lib/spec-helpers';

const partition = 'service-worker-main-spec';

describe('ServiceWorkerMain module', () => {
  const fixtures = path.resolve(__dirname, 'fixtures');
  const webContentsInternal: typeof ElectronInternal.WebContents = webContentsModule as any;

  let ses: Electron.Session;
  let serviceWorkers: Electron.ServiceWorkers;
  let server: http.Server;
  let baseUrl: string;
  let wc: WebContents;

  beforeEach(async () => {
    ses = session.fromPartition(partition);
    serviceWorkers = ses.serviceWorkers;
    serviceWorkers.on('console-message', (_e, details) => {
      console.log(details.message);
    });

    const uuid = crypto.randomUUID();
    server = http.createServer((req, res) => {
      const url = new URL(req.url!, `http://${req.headers.host}`);
      // /{uuid}/{file}
      const file = url.pathname!.split('/')[2]!;

      if (file.endsWith('.js')) {
        res.setHeader('Content-Type', 'application/javascript');
      }
      res.end(fs.readFileSync(path.resolve(fixtures, 'api', 'service-workers', file)));
    });
    const { port } = await listen(server);
    baseUrl = `http://localhost:${port}/${uuid}`;

    wc = webContentsInternal.create({ session: ses });
    wc.on('console-message', (_e, _l, message) => {
      console.log(message);
    });
  });

  afterEach(async () => {
    wc.destroy();
    server.close();
    await ses.clearData();
    ses.setPreloadScripts([]);
  });

  describe('serviceWorkers.fromVersionID', () => {
    it('returns undefined for non-live service worker', () => {
      const serviceWorker = serviceWorkers.fromVersionID(-1);
      expect(serviceWorker).to.be.undefined();
    });

    it('returns instance for live service worker', async () => {
      const versionUpdated = once(serviceWorkers, 'version-updated');
      await wc.loadURL(`${baseUrl}/index.html`);
      const [event] = await versionUpdated;
      expect(event).to.have.property('versionId').that.is.a('number');
      const serviceWorker = serviceWorkers.fromVersionID(event.versionId);
      expect(serviceWorker).to.not.be.undefined();
      expect(serviceWorker).to.have.property('versionId').that.is.a('number');
      if (!serviceWorker) return;
      expect(serviceWorker.versionId).to.equal(event.versionId);
      expect(serviceWorker.scope).to.equal(`${baseUrl}/`);
    });

    it('should not crash on script error', async () => {
      wc.loadURL(`${baseUrl}/index.html?scriptUrl=sw-script-error.js`);
      let serviceWorker;
      const actualStatuses = [];
      for await (const [{ versionId, runningStatus }] of on(serviceWorkers, 'version-updated')) {
        if (!serviceWorker) {
          serviceWorker = serviceWorkers.fromVersionID(versionId);
        }
        actualStatuses.push(runningStatus);
        if (runningStatus === 'stopping') {
          break;
        }
      }
      expect(actualStatuses).to.deep.equal(['starting', 'stopping']);
      expect(serviceWorker).to.not.be.undefined();
    });
  });

  describe('ipc', () => {
    // TODO
  });
});
