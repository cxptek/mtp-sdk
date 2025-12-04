"use strict";

import { SubscriptionManager } from "../../shared/SubscriptionManager.js";
import { TpSdkHybridObject } from "../../shared/TpSdkInstance.js";
import { createModuleValidator } from "../../shared/moduleUtils.js";
const validateInitialized = createModuleValidator('orderbook');
const subscriptionManager = new SubscriptionManager(callback => TpSdkHybridObject.orderbookSubscribe(callback), () => TpSdkHybridObject.orderbookUnsubscribe());
export const orderbook = {
  subscribe: callback => {
    validateInitialized('subscribe');
    return subscriptionManager.subscribe(callback);
  },
  unsubscribe: subscriptionId => {
    subscriptionManager.unsubscribe(subscriptionId);
  }
};
//# sourceMappingURL=orderbook.js.map