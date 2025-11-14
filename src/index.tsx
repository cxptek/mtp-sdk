import { NitroModules } from 'react-native-nitro-modules';
import type { TpSdk } from './TpSdk.nitro';

const TpSdkHybridObject =
  NitroModules.createHybridObject<TpSdk>('TpSdk');

export function multiply(a: number, b: number): number {
  return TpSdkHybridObject.multiply(a, b);
}
