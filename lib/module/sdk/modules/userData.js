"use strict";

import { SubscriptionManager } from "../../shared/SubscriptionManager.js";
import { TpSdkHybridObject } from "../../shared/TpSdkInstance.js";
import { createModuleValidator } from "../../shared/moduleUtils.js";
const validateInitialized = createModuleValidator('userData');
const subscriptionManager = new SubscriptionManager(callback => TpSdkHybridObject.userDataSubscribe(callback), () => TpSdkHybridObject.userDataUnsubscribe());
export const userData = {
  subscribe: callback => {
    validateInitialized('subscribe');
    return subscriptionManager.subscribe(callback);
  },
  unsubscribe: subscriptionId => {
    subscriptionManager.unsubscribe(subscriptionId);
  }
};
//# sourceMappingURL=userData.js.map