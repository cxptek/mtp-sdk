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
import type { TpSdkType } from './sdk';
import { TpSdk as TpSdkValue } from './sdk';
export { TpSdkValue as TpSdk };
export type TpSdk = TpSdkType;
export { kline } from './sdk/modules/kline';
export { orderbook } from './sdk/modules/orderbook';
export { ticker } from './sdk/modules/ticker';
export { tickerAll } from './sdk/modules/tickerAll';
export { trades } from './sdk/modules/trades';
export { userData } from './sdk/modules/userData';
export type { KlineMessageData, OrderBookLevel, OrderBookMessageData, OrderBookViewItem, OrderBookViewResult, ProtocolMessageDataNitro, TickerMessageData, TradeMessageData, UserMessageData, WebSocketMessageType, } from './TpSdk.nitro';
export type { WebSocketMessageResult } from './types';
export type { DepthData, DepthLevel, ProtocolMessageData, SocketOrderBook, TradeSide, } from './types';
export { createSdkError, createSdkErrorWithCode, SdkErrorCode, } from './shared/errors';
export type { SdkErrorOptions } from './shared/errors';
//# sourceMappingURL=index.d.ts.map