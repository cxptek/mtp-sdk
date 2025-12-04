import type { KlineMessageData } from '../../TpSdk.nitro';
export declare const kline: {
    subscribe: (callback: (data: KlineMessageData) => void) => string;
    unsubscribe: (subscriptionId: string) => void;
};
//# sourceMappingURL=kline.d.ts.map