// import { expect } from 'chai';
import { BrowserWindow } from 'electron/main';
import * as electron from 'electron/main';
import { emittedOnce } from './events-helpers';

// eslint-disable-next-line
describe.only('devTools module', () => {
  const { devTools } = electron as any;

  it('can see all targets', async () => {
    const w = new BrowserWindow();
    // const intId = setInterval(() => {
    //   console.log('targets', devTools.getAllTargets());
    // }, 500);
    const swPromise = emittedOnce(w.webContents.session.serviceWorkers, 'registration-completed');
    // .on('registration-completed', () => {
    //   console.log('reg targets', devTools.getAllTargets());
    // });
    await w.webContents.session.clearCache();
    w.loadURL('https://www.reddit.com/');
    // await new Promise(resolve => setTimeout(resolve, 10e3));
    // clearInterval(intId);
    const [, details] = await swPromise;
    console.log(details);
    const swTarget = devTools.getAllTargets().find((t: any) => t.targetType === 'service_worker');
    console.log('sw target', swTarget);
    const dbg = devTools.getDebuggerByTargetId(swTarget.targetId) as Electron.Debugger;
    console.log('debugger', dbg);
    dbg.attach();
    // await new Promise(resolve => setTimeout(resolve, 10e3));
    console.log('targets', devTools.getAllTargets());
  });
});
