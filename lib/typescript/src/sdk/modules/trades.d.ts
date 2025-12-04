import type { TradeMessageData } from '../../TpSdk.nitro';
export declare const trades: {
    subscribe: (callback: (trades: TradeMessageData[]) => void) => string;
    unsubscribe: (subscriptionId: string) => void;
};
//# sourceMappingURL=trades.d.ts.map