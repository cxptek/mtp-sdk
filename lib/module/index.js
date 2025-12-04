"use strict";

/**
 * Trading Platform SDK - Main Entry Point
 *
 * This is the main entry point for the Trading Platform SDK.
 * All public APIs are exported from here.
 *
 * @example
 * ```typescript
 * import { TpSdk } from '@cxptek/tp-sdk';
 *
 * // Initialize SDK
 * TpSdk.init((response) => {
 *   if (response === 'success') {
 *     // Use SDK modules
 *     TpSdk.orderbook.subscribe((data) => { ... });
 *   }
 * });
 * ```
 */

// ============================================================================
// Main SDK Instance
// ============================================================================

import { TpSdk as TpSdkValue } from "./sdk/index.js";

// Export the instance
export { TpSdkValue as TpSdk };
// Export the type as a type alias for better autocomplete support

// ============================================================================
// SDK Modules (Market Data)
// ============================================================================
export { kline } from "./sdk/modules/kline.js";
export { orderbook } from "./sdk/modules/orderbook.js";
export { ticker } from "./sdk/modules/ticker.js";
export { tickerAll } from "./sdk/modules/tickerAll.js";
export { trades } from "./sdk/modules/trades.js";

// ============================================================================
// SDK Modules (User Data)
// ============================================================================
export { userData } from "./sdk/modules/userData.js";

// ============================================================================
// Types (from TpSdk.nitro - Nitro-compatible types)
// ============================================================================

// ============================================================================
// Types (from types/index.ts - TypeScript types with 'any' fields)
// ============================================================================

// ============================================================================
// Types (from types/index.ts - Extended types with 'any' fields)
// ============================================================================

// ============================================================================
// Error Utilities
// ============================================================================
export { createSdkError, createSdkErrorWithCode, SdkErrorCode } from "./shared/errors.js";
//# sourceMappingURL=index.js.map