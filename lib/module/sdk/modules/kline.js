"use strict";

/**
 * Kline Module
 *
 * Clean API for kline/candlestick management and subscriptions.
 * Single-symbol state - only one symbol at a time.
 */

import { TpSdkHybridObject } from "../../shared/TpSdkInstance.js";
import { processCallbacksThrottled } from "../../shared/callbackQueue.js";
import { SubscriptionManager } from "../../shared/SubscriptionManager.js";
import { createModuleValidator } from "../../shared/moduleUtils.js";
import { createSdkErrorWithCode, SdkErrorCode } from "../../shared/errors.js";
const validateInitialized = createModuleValidator('kline');

// Subscription manager for kline updates
const subscriptionManager = new SubscriptionManager(callback => TpSdkHybridObject.klineSubscribe(callback), () => TpSdkHybridObject.klineUnsubscribe());
// Internal function to set snapshot
function setSnapshotInternal(snapshot) {
  if (!snapshot.interval) {
    throw createSdkErrorWithCode(SdkErrorCode.MISSING_PARAMETER, {
      module: 'kline',
      function: 'subscribe',
      message: 'Interval is required in snapshot data'
    });
  }

  // Process as WebSocket message to populate state
  const klineMessage = JSON.stringify({
    e: 'kline',
    E: Date.now(),
    s: snapshot.symbol || '',
    k: {
      t: snapshot.openTime || Date.now(),
      T: snapshot.timestamp || Date.now(),
      s: snapshot.symbol || '',
      i: snapshot.interval || '',
      o: snapshot.open || '0',
      h: snapshot.high || '0',
      l: snapshot.low || '0',
      c: snapshot.close || '0',
      v: snapshot.volume || '0',
      q: snapshot.quoteVolume || '0',
      n: snapshot.trades || '0'
    }
  });
  TpSdkHybridObject.processWebSocketMessage(klineMessage);
  // Note: processWebSocketMessage queues callbacks, process them using throttled version
  processCallbacksThrottled();
}
export const kline = {
  /**
   * Subscribe to kline updates
   *
   * @param callback - Callback function to receive updates
   * @param options - Optional subscription options
   * @param options.snapshot - Initial kline data (optional, will be set if provided)
   * @returns Subscription ID for unsubscribing
   *
   * @example
   * ```typescript
   * const subId = TpSdk.kline.subscribe((data) => {
   *   console.log('Kline update:', data);
   * }, {
   *   snapshot: initialKline
   * });
   *
   * // Later, unsubscribe
   * TpSdk.kline.unsubscribe(subId);
   * ```
   */
  subscribe: (callback, options) => {
    validateInitialized('subscribe');
    const {
      snapshot
    } = options || {};
    if (snapshot) {
      setSnapshotInternal(snapshot);
    }
    const subscriptionId = subscriptionManager.subscribe(callback);

    // Process callbacks if snapshot was set
    // Note: When data comes from WebSocket later, parseMessage() will automatically process callbacks
    // via processCallbacksThrottled(), so callbacks from socket data will also be processed correctly
    if (snapshot) {
      processCallbacksThrottled();
      requestAnimationFrame(() => {
        try {
          processCallbacksThrottled();
          callback(snapshot);
        } catch {
          // Silent fail - stream updates will trigger callback
        }
      });
    }
    return subscriptionId;
  },
  /**
   * Unsubscribe from kline updates
   *
   * @param subscriptionId - Subscription ID returned from subscribe
   *
   * @example
   * ```typescript
   * TpSdk.kline.unsubscribe(subscriptionId);
   * ```
   */
  unsubscribe: subscriptionId => {
    subscriptionManager.unsubscribe(subscriptionId);
  }
};
//# sourceMappingURL=kline.js.map