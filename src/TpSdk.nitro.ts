import type { HybridObject } from 'react-native-nitro-modules';
import type {
  KlineMessageData,
  OrderBookMessageData,
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

  orderbookSubscribe(callback: (data: OrderBookMessageData) => void): void;
  orderbookUnsubscribe(): void;

  // ============================================================================
  // Trades Methods
  // ============================================================================

  tradesSubscribe(callback: (data: TradeMessageData[]) => void): void;
  tradesUnsubscribe(): void;

  // ============================================================================
  // Ticker Methods
  // ============================================================================

  miniTickerSubscribe(callback: (data: TickerMessageData) => void): void;
  miniTickerUnsubscribe(): void;
  miniTickerPairSubscribe(callback: (data: TickerMessageData[]) => void): void;
  miniTickerPairUnsubscribe(): void;

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

export { WebSocketMessageType } from './types';
export type {
  KlineMessageData,
  OrderBookMessageData,
  ProtocolMessageDataNitro,
  TickerMessageData,
  TradeMessageData,
  UserMessageData,
  WebSocketMessageResultNitro,
} from './types';
