import { TpSdkHybridObject } from '../shared/TpSdkInstance';
declare const modules: {
    orderbook: {
        subscribe: (callback: (data: import("..").OrderBookViewResult) => void, options?: {
            aggregation?: string;
            decimals?: {
                base?: number;
                quote?: number;
            };
            snapshot?: {
                bids: [string, string][];
                asks: [string, string][];
            };
        }) => string;
        unsubscribe: (subscriptionId: string) => void;
        reset: () => void;
        setAggregation: (aggregation: string) => void;
        sendFakeData: (options?: {
            symbol?: string;
            basePrice?: number;
            numLevels?: number;
            priceStep?: number;
            aggregation?: string;
        }) => void;
    };
    trades: {
        subscribe: (callback: (trade: import("..").TradeMessageData) => void, options?: {
            decimals?: {
                price?: number;
                quantity?: number;
            };
            snapshot?: import("..").TradeMessageData[];
        }) => string;
        unsubscribe: (subscriptionId: string) => void;
        reset: () => void;
    };
    ticker: {
        subscribe: (callback: (data: import("..").TickerMessageData) => void, options?: {
            decimals?: {
                price?: number;
            };
        }) => string;
        unsubscribe: (subscriptionId: string) => void;
        sendFakeData: (options?: {
            symbol?: string;
            basePrice?: number;
        }) => void;
    };
    tickerAll: {
        subscribe: (callback: (data: import("..").TickerMessageData[]) => void) => string;
        unsubscribe: (subscriptionId: string) => void;
        sendFakeData: (options?: {
            symbols?: string[];
        }) => void;
    };
    kline: {
        subscribe: (callback: (data: import("..").KlineMessageData) => void, options?: {
            snapshot?: import("..").KlineMessageData;
        }) => string;
        unsubscribe: (subscriptionId: string) => void;
    };
    userData: {
        subscribe: (callback: (data: import("..").UserMessageData) => void) => string;
        unsubscribe: (subscriptionId: string) => void;
        sendFakeData: () => void;
    };
    init: (callback?: (response: "success" | "fail") => void) => void;
    parseMessage: (messageJson: string, options?: {
        autoProcess?: boolean;
        throttleMs?: number;
    }) => void;
    processCallbacks: () => void;
};
export type TpSdkType = typeof TpSdkHybridObject & typeof modules;
declare const TpSdkInstance: TpSdkType;
export { TpSdkInstance as TpSdk };
//# sourceMappingURL=index.d.ts.map