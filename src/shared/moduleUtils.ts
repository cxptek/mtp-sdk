/**
 * Module Utilities
 *
 * Common utilities shared across all SDK modules to reduce duplication.
 */

import { createSdkErrorWithCode, SdkErrorCode } from './errors';

/**
 * Check if SDK is initialized
 *
 * @returns true if SDK is initialized, false otherwise
 */
export function getIsInitialized(): boolean {
  try {
    const TpSdk = require('../sdk/index').TpSdk;
    return TpSdk.isInitialized?.() || false;
  } catch {
    return false;
  }
}

/**
 * Create a validation function for a specific module
 *
 * @param moduleName - Name of the module (e.g., 'orderbook', 'trades')
 * @returns A function that validates SDK is initialized, throws error if not
 *
 * @example
 * ```typescript
 * const validateInitialized = createModuleValidator('orderbook');
 * validateInitialized('setAggregation');
 * ```
 */
export function createModuleValidator(moduleName: string) {
  return (functionName: string): void => {
    if (!getIsInitialized()) {
      throw createSdkErrorWithCode(SdkErrorCode.NOT_INITIALIZED, {
        module: moduleName,
        function: functionName,
        message: 'SDK must be initialized first',
        required: ['TpSdk.init()'],
      });
    }
  };
}
