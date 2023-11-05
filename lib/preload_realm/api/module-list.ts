export const moduleList: ElectronInternal.ModuleEntry[] = [
  {
    name: 'nativeImage',
    loader: () => require('@electron/internal/common/api/native-image')
  }
];
