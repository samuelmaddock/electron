import { ipcMain, session, webContents as webContentsModule, WebContents } from 'electron/main';

import { expect } from 'chai';

import { once, on } from 'node:events';
import * as fs from 'node:fs';
import * as http from 'node:http';
import * as path from 'node:path';

import { listen } from './lib/spec-helpers';

// Toggle to add extra debug output
const DEBUG = false;

describe('ServiceWorkerMain module', () => {
  const fixtures = path.resolve(__dirname, 'fixtures');
  const preloadRealmFixtures = path.resolve(fixtures, 'api/preload-realm');
  const webContentsInternal: typeof ElectronInternal.WebContents = webContentsModule as any;

  let ses: Electron.Session;
  let serviceWorkers: Electron.ServiceWorkers;
  let server: http.Server;
  let baseUrl: string;
  let wc: WebContents;

  beforeEach(async () => {
    ses = session.fromPartition(`service-worker-main-spec-${crypto.randomUUID()}`);
    serviceWorkers = ses.serviceWorkers;

    if (DEBUG) {
      serviceWorkers.on('console-message', (_e, details) => {
        console.log(details.message);
      });
    }

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

    if (DEBUG) {
      wc.on('console-message', (_e, _l, message) => {
        console.log(message);
      });
    }
  });

  afterEach(async () => {
    wc.destroy();
    server.close();
    ses.getPreloadScripts().map(({ id }) => ses.unregisterPreloadScript(id));
  });

  async function loadWorkerScript (scriptUrl?: string) {
    const scriptParams = scriptUrl ? `?scriptUrl=${scriptUrl}` : '';
    return wc.loadURL(`${baseUrl}/index.html${scriptParams}`);
  }

  async function unregisterAllServiceWorkers () {
    await wc.executeJavaScript(`(${async function () {
      const registrations = await navigator.serviceWorker.getRegistrations();
      for (const registration of registrations) {
        registration.unregister();
      }
    }}())`);
  }

  async function waitForServiceWorker (expectedRunningStatus: Electron.ServiceWorkersRunningStatusChangedEventParams['runningStatus'] = 'starting') {
    const serviceWorkerPromise = new Promise<Electron.ServiceWorkerMain>((resolve) => {
      function onRunningStatusChanged ({ versionId, runningStatus }: Electron.ServiceWorkersRunningStatusChangedEventParams) {
        if (runningStatus === expectedRunningStatus) {
          const serviceWorker = serviceWorkers.fromVersionID(versionId)!;
          serviceWorkers.off('running-status-changed', onRunningStatusChanged);
          resolve(serviceWorker);
        }
      }
      serviceWorkers.on('running-status-changed', onRunningStatusChanged);
    });
    const serviceWorker = await serviceWorkerPromise;
    expect(serviceWorker).to.not.be.undefined();
    return serviceWorker!;
  }

  describe('serviceWorkers.fromVersionID', () => {
    it('returns undefined for non-live service worker', () => {
      expect(serviceWorkers.fromVersionID(-1)).to.be.undefined();
      expect(serviceWorkers._fromVersionIDIfExists(-1)).to.be.undefined();
    });

    it('returns instance for live service worker', async () => {
      const runningStatusChanged = once(serviceWorkers, 'running-status-changed');
      loadWorkerScript();
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

    it.skip('does not find unregistered service worker', async () => {
      // TODO: register, lookup, unregister, lookup
    });
  });

  describe('isDestroyed()', () => {
    it('is not destroyed after being created', async () => {
      loadWorkerScript();
      const serviceWorker = await waitForServiceWorker();
      expect(serviceWorker.isDestroyed()).to.be.false();
    });

    // TODO: hook into ServiceWorkerLive OnDestroyed
    it.skip('is destroyed after being unregistered', async () => {
      loadWorkerScript();
      const stoppedServiceWorkerPromise = waitForServiceWorker('stopped');
      const serviceWorker = await waitForServiceWorker();
      expect(serviceWorker.isDestroyed()).to.be.false();
      await unregisterAllServiceWorkers();
      await stoppedServiceWorkerPromise;
    });
  });

  describe('startWorker()', () => {
    it('resolves with running workers', async () => {
      loadWorkerScript();
      const serviceWorker = await waitForServiceWorker('running');
      const startWorkerPromise = serviceWorker.startWorker();
      await expect(startWorkerPromise).to.eventually.be.fulfilled();
    });

    // TODO: crashes
    it.skip('resolves with starting workers', async () => {
      loadWorkerScript();
      const serviceWorker = await waitForServiceWorker('starting');
      const startWorkerPromise = serviceWorker.startWorker();
      await expect(startWorkerPromise).to.eventually.be.fulfilled();
    });

    it.skip('starts previously stopped worker', () => {
      // TODO
    });
  });

  describe('startTask()', () => {
    it('has no tasks in-flight initially', async () => {
      loadWorkerScript();
      const serviceWorker = await waitForServiceWorker();
      expect(serviceWorker._countExternalRequests()).to.equal(0);
    });

    it('can start and end a task', async () => {
      loadWorkerScript();

      // Internally, ServiceWorkerVersion buckets tasks into requests made
      // during and after startup.
      // ServiceWorkerContext::CountExternalRequestsForTest only considers
      // requests made while SW is in running status so we need to wait for that
      // to read an accurate count.
      const serviceWorker = await waitForServiceWorker('running');

      const task = serviceWorker.startTask();
      expect(task).to.be.an('object');
      expect(task).to.have.property('end').that.is.a('function');
      expect(serviceWorker._countExternalRequests()).to.equal(1);

      task.end();

      // Count will decrement after Promise.finally callback
      await new Promise<void>(queueMicrotask);
      expect(serviceWorker._countExternalRequests()).to.equal(0);
    });

    it('can have more than one active task', async () => {
      loadWorkerScript();
      const serviceWorker = await waitForServiceWorker('running');

      const taskA = serviceWorker.startTask();
      const taskB = serviceWorker.startTask();
      expect(serviceWorker._countExternalRequests()).to.equal(2);
      taskB.end();
      taskA.end();

      // Count will decrement after Promise.finally callback
      await new Promise<void>(queueMicrotask);
      expect(serviceWorker._countExternalRequests()).to.equal(0);
    });
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
      loadWorkerScript();
      const serviceWorker = await waitForServiceWorker();
      expect(serviceWorker).to.not.be.undefined();
      if (!serviceWorker) return;
      expect(serviceWorker).to.have.property('scope').that.is.a('string');
      expect(serviceWorker.scope).to.equal(`${baseUrl}/`);
    });
  });

  describe('ipc', () => {
    describe('sw -> main', () => {
      it('receives a message', async () => {
        ses.registerPreloadScript({
          id: crypto.randomUUID(),
          type: 'service-worker',
          filePath: path.resolve(preloadRealmFixtures, 'preload-send-ping.js')
        });

        loadWorkerScript();
        const serviceWorker = await waitForServiceWorker();
        const pingPromise = once(serviceWorker.ipc, 'ping');
        await pingPromise;
      });

      it('does not receive message on ipcMain', async () => {
        ses.registerPreloadScript({
          id: crypto.randomUUID(),
          type: 'service-worker',
          filePath: path.resolve(preloadRealmFixtures, 'preload-send-ping.js')
        });

        loadWorkerScript();
        await waitForServiceWorker();
        const abortController = new AbortController();
        try {
          let pingReceived = false;
          once(ipcMain, 'ping', { signal: abortController.signal }).then(() => {
            pingReceived = true;
          });
          await once(ses, '-ipc-message');
          await new Promise<void>(queueMicrotask);
          expect(pingReceived).to.be.false();
        } finally {
          abortController.abort();
        }
      });
    });
  });
});
