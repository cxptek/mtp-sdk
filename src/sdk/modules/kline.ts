import type { KlineMessageData } from '../../TpSdk.nitro';
import { SubscriptionManager } from '../../shared/SubscriptionManager';
import { TpSdkHybridObject } from '../../shared/TpSdkInstance';
import { createModuleValidator } from '../../shared/moduleUtils';

const validateInitialized = createModuleValidator('kline');

const subscriptionManager = new SubscriptionManager<KlineMessageData>(
  (callback) => TpSdkHybridObject.klineSubscribe(callback),
  () => TpSdkHybridObject.klineUnsubscribe()
);

export const kline = {
  subscribe: (callback: (data: KlineMessageData) => void): string => {
    validateInitialized('subscribe');
    return subscriptionManager.subscribe(callback);
  },

  unsubscribe: (subscriptionId: string): void => {
    subscriptionManager.unsubscribe(subscriptionId);
  },
};
