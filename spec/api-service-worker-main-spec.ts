import { session, webContents as webContentsModule, WebContents } from 'electron/main';

import { expect } from 'chai';

import { once, on } from 'node:events';
import * as fs from 'node:fs';
import * as http from 'node:http';
import * as path from 'node:path';

import { listen } from './lib/spec-helpers';

describe('ServiceWorkerMain module', () => {
  const fixtures = path.resolve(__dirname, 'fixtures');
  const webContentsInternal: typeof ElectronInternal.WebContents = webContentsModule as any;

  let ses: Electron.Session;
  let serviceWorkers: Electron.ServiceWorkers;
  let server: http.Server;
  let baseUrl: string;
  let wc: WebContents;

  beforeEach(async () => {
    ses = session.fromPartition(`service-worker-main-spec-${crypto.randomUUID()}`);
    serviceWorkers = ses.serviceWorkers;
    // serviceWorkers.on('console-message', (_e, details) => {
    //   console.log(details.message);
    // });

    const uuid = crypto.randomUUID();
    console.log(`new uuid ${uuid}`);
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
    // wc.on('console-message', (_e, _l, message) => {
    //   console.log(message);
    // });
  });

  afterEach(async () => {
    wc.destroy();
    server.close();
    ses.setPreloadScripts([]);
  });

  describe('serviceWorkers.fromVersionID', () => {
    it('returns undefined for non-live service worker', () => {
      expect(serviceWorkers.fromVersionID(-1)).to.be.undefined();
      expect(serviceWorkers._fromVersionIDIfExists(-1)).to.be.undefined();
    });

    it('returns instance for live service worker', async () => {
      const runningStatusChanged = once(serviceWorkers, 'running-status-changed');
      wc.loadURL(`${baseUrl}/index.html`);
      const [{ versionId }] = await runningStatusChanged;
      const serviceWorker = serviceWorkers.fromVersionID(versionId);
      expect(serviceWorker).to.not.be.undefined();
      expect(serviceWorkers._fromVersionIDIfExists(versionId)).to.not.be.undefined();
      // eslint-disable-next-line no-unused-expressions
      serviceWorker; // hold reference
    });

    it('should not crash on script error', async () => {
      wc.loadURL(`${baseUrl}/index.html?scriptUrl=sw-script-error.js`);
      let serviceWorker;
      const actualStatuses = [];
      for await (const [{ versionId, runningStatus }] of on(serviceWorkers, 'running-status-changed')) {
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

  describe('isDestroyed()', () => {
    // TODO
  });

  describe('startWorker()', () => {
    // TODO
  });

  describe('startTask()', () => {
    // TODO
  });

  describe("'versionId' property", () => {
    it('matches the expected value', async () => {
      const runningStatusChanged = once(serviceWorkers, 'running-status-changed');
      wc.loadURL(`${baseUrl}/index.html`);
      const [{ versionId }] = await runningStatusChanged;
      const serviceWorker = serviceWorkers.fromVersionID(versionId);
      expect(serviceWorker).to.not.be.undefined();
      if (!serviceWorker) return;
      expect(serviceWorker).to.have.property('versionId').that.is.a('number');
      expect(serviceWorker.versionId).to.equal(versionId);
    });
  });

  describe("'scope' property", () => {
    it('matches the expected value', async () => {
      const runningStatusChanged = once(serviceWorkers, 'running-status-changed');
      wc.loadURL(`${baseUrl}/index.html`);
      const [{ versionId }] = await runningStatusChanged;
      const serviceWorker = serviceWorkers.fromVersionID(versionId);
      expect(serviceWorker).to.not.be.undefined();
      if (!serviceWorker) return;
      expect(serviceWorker).to.have.property('scope').that.is.a('string');
      expect(serviceWorker.scope).to.equal(`${baseUrl}/`);
    });
  });

  describe('ipc', () => {
    // TODO
  });
});
