import type { HybridObject } from 'react-native-nitro-modules';
import type { KlineMessageData, OrderBookViewResult, TickerMessageData, TradeMessageData, UserMessageData, WebSocketMessageResultNitro } from './types';
export interface TpSdk extends HybridObject<{
    ios: 'c++';
    android: 'c++';
}> {
    processWebSocketMessage(messageJson: string): WebSocketMessageResultNitro | null;
    processCallbackQueue(): void;
    isInitialized(): boolean;
    markInitialized(callback?: () => void): void;
    orderbookReset(): void;
    orderbookSubscribe(callback: (data: OrderBookViewResult) => void): void;
    orderbookUnsubscribe(): void;
    orderbookConfigSetAggregation(aggregationStr: string): void;
    orderbookConfigSetDecimals(baseDecimals?: number, quoteDecimals?: number): void;
    orderbookDataSetSnapshot(bids: [string, string][], asks: [string, string][], baseDecimals?: number, quoteDecimals?: number): void;
    tradesSubscribe(callback: (data: TradeMessageData) => void): void;
    tradesUnsubscribe(): void;
    tradesReset(): void;
    tradesConfigSetDecimals(priceDecimals?: number, quantityDecimals?: number): void;
    miniTickerSubscribe(callback: (data: TickerMessageData) => void): void;
    miniTickerUnsubscribe(): void;
    miniTickerPairSubscribe(callback: (data: TickerMessageData[]) => void): void;
    miniTickerPairUnsubscribe(): void;
    tickerConfigSetDecimals(priceDecimals?: number): void;
    klineSubscribe(callback: (data: KlineMessageData) => void): void;
    klineUnsubscribe(): void;
    userDataSubscribe(callback: (data: UserMessageData) => void): void;
    userDataUnsubscribe(): void;
}
export type { DepthData, DepthLevel, KlineMessageData, OrderBookLevel, OrderBookMessageData, OrderBookViewItem, OrderBookViewResult, ProtocolMessageDataNitro, SocketOrderBook, TickerMessageData, TradeMessageData, TradeSide, UpsertOrderBookResult, UserMessageData, WebSocketMessageResultNitro, } from './types';
export { WebSocketMessageType } from './types';
//# sourceMappingURL=TpSdk.nitro.d.ts.map