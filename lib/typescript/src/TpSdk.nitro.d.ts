import type { HybridObject } from 'react-native-nitro-modules';
import type { KlineMessageData, OrderBookMessageData, TickerMessageData, TradeMessageData, UserMessageData, WebSocketMessageResultNitro } from './types';
export interface TpSdk extends HybridObject<{
    ios: 'c++';
    android: 'c++';
}> {
    processWebSocketMessage(messageJson: string): WebSocketMessageResultNitro | null;
    processCallbackQueue(): void;
    isInitialized(): boolean;
    markInitialized(callback?: () => void): void;
    orderbookSubscribe(callback: (data: OrderBookMessageData) => void): void;
    orderbookUnsubscribe(): void;
    tradesSubscribe(callback: (data: TradeMessageData[]) => void): void;
    tradesUnsubscribe(): void;
    miniTickerSubscribe(callback: (data: TickerMessageData) => void): void;
    miniTickerUnsubscribe(): void;
    miniTickerPairSubscribe(callback: (data: TickerMessageData[]) => void): void;
    miniTickerPairUnsubscribe(): void;
    klineSubscribe(callback: (data: KlineMessageData) => void): void;
    klineUnsubscribe(): void;
    userDataSubscribe(callback: (data: UserMessageData) => void): void;
    userDataUnsubscribe(): void;
}
export { WebSocketMessageType } from './types';
export type { KlineMessageData, OrderBookMessageData, ProtocolMessageDataNitro, TickerMessageData, TradeMessageData, UserMessageData, WebSocketMessageResultNitro } from './types';
//# sourceMappingURL=TpSdk.nitro.d.ts.map