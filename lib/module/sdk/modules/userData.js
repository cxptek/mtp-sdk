"use strict";

/**
 * UserData Module
 *
 * Clean API for user data subscriptions (order updates).
 */

import { TpSdkHybridObject } from "../../shared/TpSdkInstance.js";
import { processFakeMessage } from "../../shared/callbackQueue.js";
import { SubscriptionManager } from "../../shared/SubscriptionManager.js";
import { createModuleValidator } from "../../shared/moduleUtils.js";
const validateInitialized = createModuleValidator('userData');

// Subscription manager for user data updates
const subscriptionManager = new SubscriptionManager(callback => TpSdkHybridObject.userDataSubscribe(callback), () => TpSdkHybridObject.userDataUnsubscribe());
export const userData = {
  /**
   * Subscribe to user data updates
   *
   * @param callback - Callback function to receive updates
   * @returns Subscription ID for unsubscribing
   *
   * @example
   * ```typescript
   * const subId = TpSdk.userData.subscribe((data) => {
   *   console.log('User data updated:', data);
   * });
   *
   * // Later, unsubscribe
   * TpSdk.userData.unsubscribe(subId);
   * ```
   */
  subscribe: callback => {
    validateInitialized('subscribe');
    return subscriptionManager.subscribe(callback);
  },
  /**
   * Unsubscribe from user data updates
   *
   * @param subscriptionId - Subscription ID returned from subscribe
   *
   * @example
   * ```typescript
   * TpSdk.userData.unsubscribe(subscriptionId);
   * ```
   */
  unsubscribe: subscriptionId => {
    subscriptionManager.unsubscribe(subscriptionId);
  },
  /**
   * Send fake user data for testing
   *
   * Useful when WebSocket is not available or for testing UI.
   *
   * @example
   * ```typescript
   * // Send fake order data
   * TpSdk.userData.sendFakeData();
   * ```
   */
  sendFakeData: () => {
    const timestamp = Date.now();
    const fakeMessage = JSON.stringify({
      e: 'orderUpdate',
      E: timestamp,
      id: Math.floor(Math.random() * 1000000).toString(),
      userId: '316039530501112832',
      symbolCode: 'ETH_USDT',
      quantity: '1',
      price: '3055.16',
      status: 'ACTIVE',
      type: 'LIMIT',
      action: 'BUY',
      baseFilled: '0',
      quoteFilled: '0',
      quoteQuantity: '3055.16',
      fee: '0',
      feeAsset: 'ETH',
      matchingPrice: '',
      avgPrice: '0',
      isCancelAll: false,
      canceledBy: '',
      createdAt: timestamp - 100,
      updatedAt: timestamp - 100,
      submittedAt: timestamp - 50,
      dsTime: timestamp - 10,
      triggerPrice: '',
      conditionalOrderType: '',
      timeInForce: 'GTC',
      triggerStatus: '',
      triggerDirection: 0,
      placeOrderReason: 'NONE',
      contingencyType: 'SINGLE',
      refId: '0',
      reduceVolume: '',
      rejectedVolume: '',
      rejectedBudget: ''
    });

    // Send through C++ processing pipeline and process callbacks
    processFakeMessage(fakeMessage);
  }
};
//# sourceMappingURL=userData.js.map