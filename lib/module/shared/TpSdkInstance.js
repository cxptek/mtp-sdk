"use strict";

import { NitroModules } from 'react-native-nitro-modules';
let cachedInstance = null;

/**
 * Get or create TpSdk instance with caching and hot-reload support
 */
export function getTpSdkInstance() {
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
  cachedInstance = NitroModules.createHybridObject('TpSdk');
  return cachedInstance;
}

/**
 * Proxy wrapper for TpSdk instance
 * Provides lazy initialization and automatic method binding
 */
export const TpSdkHybridObject = new Proxy({}, {
  get(_target, prop) {
    const instance = getTpSdkInstance();
    const value = instance[prop];
    if (typeof value === 'function') {
      return value.bind(instance);
    }
    return value;
  },
  set(_target, prop, value) {
    const instance = getTpSdkInstance();
    instance[prop] = value;
    return true;
  }
});
//# sourceMappingURL=TpSdkInstance.js.map