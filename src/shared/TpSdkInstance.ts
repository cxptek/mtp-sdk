import { NitroModules } from 'react-native-nitro-modules';
import type { TpSdk } from '../TpSdk.nitro';

let cachedInstance: TpSdk | null = null;

/**
 * Get or create TpSdk instance with caching and hot-reload support
 */
export function getTpSdkInstance(): TpSdk {
  if (cachedInstance) {
    try {
      // Validate instance is still valid (handle app reload/hot reload)
      cachedInstance.isInitialized?.();
      return cachedInstance;
    } catch {
      // Instance invalid (app reload), clear cache and create new
      cachedInstance = null;
    }
  }

  cachedInstance = NitroModules.createHybridObject<TpSdk>('TpSdk');
  return cachedInstance;
}

/**
 * Proxy wrapper for TpSdk instance
 * Provides lazy initialization and automatic method binding
 */
export const TpSdkHybridObject = new Proxy({} as TpSdk, {
  get(_target, prop) {
    const instance = getTpSdkInstance();
    const value = (instance as any)[prop];
    if (typeof value === 'function') {
      return value.bind(instance);
    }
    return value;
  },
  set(_target, prop, value) {
    const instance = getTpSdkInstance();
    (instance as any)[prop] = value;
    return true;
  },
});
