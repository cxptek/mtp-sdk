import { TpSdkHybridObject } from '../shared/TpSdkInstance';
declare const modules: {
    orderbook: {
        subscribe: (callback: (data: import("..").OrderBookMessageData) => void) => string;
        unsubscribe: (subscriptionId: string) => void;
    };
    trades: {
        subscribe: (callback: (trades: import("..").TradeMessageData[]) => void) => string;
        unsubscribe: (subscriptionId: string) => void;
    };
    ticker: {
        subscribe: (callback: (data: import("..").TickerMessageData) => void) => string;
        unsubscribe: (subscriptionId: string) => void;
    };
    tickerAll: {
        subscribe: (callback: (data: import("..").TickerMessageData[]) => void) => string;
        unsubscribe: (subscriptionId: string) => void;
    };
    kline: {
        subscribe: (callback: (data: import("..").KlineMessageData) => void) => string;
        unsubscribe: (subscriptionId: string) => void;
    };
    userData: {
        subscribe: (callback: (data: import("..").UserMessageData) => void) => string;
        unsubscribe: (subscriptionId: string) => void;
    };
    init: (callback?: (response: "success" | "fail") => void) => void;
    parseMessage: (messageJson: string) => void;
};
export type TpSdkType = typeof TpSdkHybridObject & typeof modules;
declare const TpSdkInstance: TpSdkType;
export { TpSdkInstance as TpSdk };
//# sourceMappingURL=index.d.ts.map