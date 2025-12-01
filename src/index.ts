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
 *     TpSdk.orderbook.setAggregation('0.01');
 *     TpSdk.orderbook.subscribe((data) => { ... });
 *   }
 * });
 * ```
 */

// ============================================================================
// Main SDK Instance
// ============================================================================
import type { TpSdkType } from './sdk';
import { TpSdk as TpSdkValue } from './sdk';

// Export the instance
export { TpSdkValue as TpSdk };
// Export the type as a type alias for better autocomplete support
export type TpSdk = TpSdkType;

// ============================================================================
// SDK Modules (Market Data)
// ============================================================================
export { kline } from './sdk/modules/kline';
export { orderbook } from './sdk/modules/orderbook';
export { ticker } from './sdk/modules/ticker';
export { tickerAll } from './sdk/modules/tickerAll';
export { trades } from './sdk/modules/trades';

// ============================================================================
// SDK Modules (User Data)
// ============================================================================
export { userData } from './sdk/modules/userData';

// ============================================================================
// Types (from TpSdk.nitro - Nitro-compatible types)
// ============================================================================
export type {
  KlineMessageData,
  OrderBookLevel,
  OrderBookMessageData,
  OrderBookViewItem,
  OrderBookViewResult,
  ProtocolMessageDataNitro,
  TickerMessageData,
  TradeMessageData,
  UserMessageData,
  WebSocketMessageType,
} from './TpSdk.nitro';

// ============================================================================
// Types (from types/index.ts - TypeScript types with 'any' fields)
// ============================================================================
export type { WebSocketMessageResult } from './types';

// ============================================================================
// Types (from types/index.ts - Extended types with 'any' fields)
// ============================================================================
export type {
  DepthData,
  DepthLevel,
  ProtocolMessageData,
  SocketOrderBook,
  TradeSide,
} from './types';

// ============================================================================
// Error Utilities
// ============================================================================
export {
  createSdkError,
  createSdkErrorWithCode,
  SdkErrorCode,
} from './shared/errors';
export type { SdkErrorOptions } from './shared/errors';
