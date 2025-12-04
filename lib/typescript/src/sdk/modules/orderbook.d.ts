import type { OrderBookMessageData } from '../../TpSdk.nitro';
export declare const orderbook: {
    subscribe: (callback: (data: OrderBookMessageData) => void) => string;
    unsubscribe: (subscriptionId: string) => void;
};
export type { OrderBookMessageData };
//# sourceMappingURL=orderbook.d.ts.map