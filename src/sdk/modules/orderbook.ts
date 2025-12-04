import type { OrderBookMessageData } from '../../TpSdk.nitro';
import { SubscriptionManager } from '../../shared/SubscriptionManager';
import { TpSdkHybridObject } from '../../shared/TpSdkInstance';
import { createModuleValidator } from '../../shared/moduleUtils';

const validateInitialized = createModuleValidator('orderbook');

const subscriptionManager = new SubscriptionManager<OrderBookMessageData>(
  (callback) => TpSdkHybridObject.orderbookSubscribe(callback),
  () => TpSdkHybridObject.orderbookUnsubscribe()
);

export const orderbook = {
  subscribe: (callback: (data: OrderBookMessageData) => void): string => {
    validateInitialized('subscribe');
    return subscriptionManager.subscribe(callback);
  },

  unsubscribe: (subscriptionId: string): void => {
    subscriptionManager.unsubscribe(subscriptionId);
  },
};

export type { OrderBookMessageData };
