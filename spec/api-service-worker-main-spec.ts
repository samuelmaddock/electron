import { once } from 'node:events';
import * as fs from 'node:fs';
import * as http from 'node:http';
import * as path from 'node:path';
import { session, webContents as webContentsModule, WebContents } from 'electron/main';
import { expect } from 'chai';
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

  before(async () => {
    ses = session.fromPartition(partition);
    serviceWorkers = ses.serviceWorkers;
    await ses.clearData();
  });

  beforeEach(async () => {
    const uuid = crypto.randomUUID();

    server = http.createServer((req, res) => {
      // /{uuid}/{file}
      const file = req.url!.split('/')[2]!;

      if (file.endsWith('.js')) {
        res.setHeader('Content-Type', 'application/javascript');
      }
      res.end(fs.readFileSync(path.resolve(fixtures, 'api', 'service-workers', file)));
    });
    const { port } = await listen(server);
    baseUrl = `http://localhost:${port}/${uuid}`;

    wc = webContentsInternal.create({ session: ses });
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
      expect(serviceWorker.versionId).to.equal(event.versionId);
    });
  });

  describe('ipc', () => {
    // TODO
  });
});
