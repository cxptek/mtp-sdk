import type { HybridObject } from 'react-native-nitro-modules';
import type {
  KlineMessageData,
  OrderBookViewResult,
  TickerMessageData,
  TradeMessageData,
  UserMessageData,
  WebSocketMessageResultNitro,
} from './types';

// ============================================================================
// TpSdk Interface
// ============================================================================

export interface TpSdk extends HybridObject<{ ios: 'c++'; android: 'c++' }> {
  // ============================================================================
  // Core Methods
  // ============================================================================

  processWebSocketMessage(
    messageJson: string
  ): WebSocketMessageResultNitro | null;
  processCallbackQueue(): void;

  isInitialized(): boolean;
  markInitialized(callback?: () => void): void;

  // ============================================================================
  // OrderBook Methods
  // ============================================================================

  orderbookReset(): void;
  orderbookSubscribe(callback: (data: OrderBookViewResult) => void): void;
  orderbookUnsubscribe(): void;
  orderbookConfigSetAggregation(aggregationStr: string): void;
  orderbookConfigSetDecimals(
    baseDecimals?: number,
    quoteDecimals?: number
  ): void;
  orderbookDataSetSnapshot(
    bids: [string, string][],
    asks: [string, string][],
    baseDecimals?: number,
    quoteDecimals?: number
  ): void;

  // ============================================================================
  // Trades Methods
  // ============================================================================

  tradesSubscribe(callback: (data: TradeMessageData) => void): void;
  tradesUnsubscribe(): void;
  tradesReset(): void;
  tradesConfigSetDecimals(
    priceDecimals?: number,
    quantityDecimals?: number
  ): void;

  // ============================================================================
  // Ticker Methods
  // ============================================================================

  miniTickerSubscribe(callback: (data: TickerMessageData) => void): void;
  miniTickerUnsubscribe(): void;
  miniTickerPairSubscribe(callback: (data: TickerMessageData[]) => void): void;
  miniTickerPairUnsubscribe(): void;
  tickerConfigSetDecimals(priceDecimals?: number): void;

  // ============================================================================
  // Kline Methods
  // ============================================================================

  klineSubscribe(callback: (data: KlineMessageData) => void): void;
  klineUnsubscribe(): void;

  // ============================================================================
  // UserData Methods
  // ============================================================================

  userDataSubscribe(callback: (data: UserMessageData) => void): void;
  userDataUnsubscribe(): void;
}

// ============================================================================
// Re-exports
// ============================================================================

export type {
  DepthData,
  DepthLevel,
  KlineMessageData,
  OrderBookLevel,
  OrderBookMessageData,
  OrderBookViewItem,
  OrderBookViewResult,
  ProtocolMessageDataNitro,
  SocketOrderBook,
  TickerMessageData,
  TradeMessageData,
  TradeSide,
  UpsertOrderBookResult,
  UserMessageData,
  WebSocketMessageResultNitro,
} from './types';
export { WebSocketMessageType } from './types';
