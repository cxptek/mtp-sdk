"use strict";

import { SubscriptionManager } from "../../shared/SubscriptionManager.js";
import { TpSdkHybridObject } from "../../shared/TpSdkInstance.js";
import { createModuleValidator } from "../../shared/moduleUtils.js";
const validateInitialized = createModuleValidator('kline');
const subscriptionManager = new SubscriptionManager(callback => TpSdkHybridObject.klineSubscribe(callback), () => TpSdkHybridObject.klineUnsubscribe());
export const kline = {
  subscribe: callback => {
    validateInitialized('subscribe');
    return subscriptionManager.subscribe(callback);
  },
  unsubscribe: subscriptionId => {
    subscriptionManager.unsubscribe(subscriptionId);
  }
};
//# sourceMappingURL=kline.js.map