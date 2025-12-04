import type { UserMessageData } from '../../TpSdk.nitro';
import { SubscriptionManager } from '../../shared/SubscriptionManager';
import { TpSdkHybridObject } from '../../shared/TpSdkInstance';
import { createModuleValidator } from '../../shared/moduleUtils';

const validateInitialized = createModuleValidator('userData');

const subscriptionManager = new SubscriptionManager<UserMessageData>(
  (callback) => TpSdkHybridObject.userDataSubscribe(callback),
  () => TpSdkHybridObject.userDataUnsubscribe()
);

export const userData = {
  subscribe: (callback: (data: UserMessageData) => void): string => {
    validateInitialized('subscribe');
    return subscriptionManager.subscribe(callback);
  },

  unsubscribe: (subscriptionId: string): void => {
    subscriptionManager.unsubscribe(subscriptionId);
  },
};
