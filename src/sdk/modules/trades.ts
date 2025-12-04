import type { TradeMessageData } from '../../TpSdk.nitro';
import { SubscriptionManager } from '../../shared/SubscriptionManager';
import { TpSdkHybridObject } from '../../shared/TpSdkInstance';
import { createModuleValidator } from '../../shared/moduleUtils';

const validateInitialized = createModuleValidator('trades');

const subscriptionManager = new SubscriptionManager<TradeMessageData[]>(
  (callback) => TpSdkHybridObject.tradesSubscribe(callback),
  () => TpSdkHybridObject.tradesUnsubscribe()
);

export const trades = {
  subscribe: (callback: (trades: TradeMessageData[]) => void): string => {
    validateInitialized('subscribe');
    return subscriptionManager.subscribe(callback);
  },

  unsubscribe: (subscriptionId: string): void => {
    subscriptionManager.unsubscribe(subscriptionId);
  },
};
