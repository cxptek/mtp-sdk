/**
 * SDK Error Utilities
 *
 * Provides consistent error formatting for TpSdk
 */

export interface SdkErrorOptions {
  module: string;
  function: string;
  message: string;
  required?: string[];
  current?: Record<string, unknown>;
}

/**
 * Create a standardized SDK error
 *
 * @example
 * ```typescript
 * throw createSdkError({
 *   module: 'tradingPair',
 *   function: 'setCurrent',
 *   message: 'Trading pair info not found',
 *   required: ['setInfo()'],
 *   current: { pairCode: 'BTC_USDT' }
 * });
 * ```
 */
export function createSdkError(options: SdkErrorOptions): Error {
  const { module, function: fn, message, required, current } = options;

  let errorMessage = `[TpSdk.${module}.${fn}] ${message}`;

  if (required && required.length > 0) {
    errorMessage += `\n  → REQUIRED: ${required.join(' → ')}`;
  }

  if (current && Object.keys(current).length > 0) {
    const currentInfo = Object.entries(current)
      .map(([key, value]) => `${key}: ${value}`)
      .join(', ');
    errorMessage += `\n  → Current: ${currentInfo}`;
  }

  return new Error(errorMessage);
}

/**
 * Error codes for different error types
 */
export enum SdkErrorCode {
  MISSING_PARAMETER = 'MISSING_PARAMETER',
  INVALID_FORMAT = 'INVALID_FORMAT',
  NOT_SETUP = 'NOT_SETUP',
  NOT_FOUND = 'NOT_FOUND',
  NOT_INITIALIZED = 'NOT_INITIALIZED',
}

/**
 * Create error with error code
 */
export function createSdkErrorWithCode(
  code: SdkErrorCode,
  options: Omit<SdkErrorOptions, 'message'> & { message?: string }
): Error {
  const defaultMessages: Record<SdkErrorCode, string> = {
    [SdkErrorCode.MISSING_PARAMETER]: 'Required parameter is missing',
    [SdkErrorCode.INVALID_FORMAT]: 'Invalid format',
    [SdkErrorCode.NOT_SETUP]: 'Required setup not completed',
    [SdkErrorCode.NOT_FOUND]: 'Resource not found',
    [SdkErrorCode.NOT_INITIALIZED]: 'SDK must be initialized first',
  };

  const message = options.message || defaultMessages[code];

  return createSdkError({
    ...options,
    message: `[${code}] ${message}`,
  });
}
