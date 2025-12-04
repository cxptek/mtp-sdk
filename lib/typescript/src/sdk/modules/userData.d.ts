import type { UserMessageData } from '../../TpSdk.nitro';
export declare const userData: {
    subscribe: (callback: (data: UserMessageData) => void) => string;
    unsubscribe: (subscriptionId: string) => void;
};
//# sourceMappingURL=userData.d.ts.map