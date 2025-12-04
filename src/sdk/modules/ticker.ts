import type { TickerMessageData } from '../../TpSdk.nitro';
import { SubscriptionManager } from '../../shared/SubscriptionManager';
import { TpSdkHybridObject } from '../../shared/TpSdkInstance';
import { createModuleValidator } from '../../shared/moduleUtils';

const validateInitialized = createModuleValidator('ticker');

const subscriptionManager = new SubscriptionManager<TickerMessageData>(
  (callback) => TpSdkHybridObject.miniTickerSubscribe(callback),
  () => TpSdkHybridObject.miniTickerUnsubscribe()
);

export const ticker = {
  subscribe: (callback: (data: TickerMessageData) => void): string => {
    validateInitialized('subscribe');
    return subscriptionManager.subscribe(callback);
  },

  unsubscribe: (subscriptionId: string): void => {
    subscriptionManager.unsubscribe(subscriptionId);
  },
};
