"use strict";

import { SubscriptionManager } from "../../shared/SubscriptionManager.js";
import { TpSdkHybridObject } from "../../shared/TpSdkInstance.js";
import { createModuleValidator } from "../../shared/moduleUtils.js";
const validateInitialized = createModuleValidator('trades');
const subscriptionManager = new SubscriptionManager(callback => TpSdkHybridObject.tradesSubscribe(callback), () => TpSdkHybridObject.tradesUnsubscribe());
export const trades = {
  subscribe: callback => {
    validateInitialized('subscribe');
    return subscriptionManager.subscribe(callback);
  },
  unsubscribe: subscriptionId => {
    subscriptionManager.unsubscribe(subscriptionId);
  }
};
//# sourceMappingURL=trades.js.map