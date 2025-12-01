"use strict";

/**
 * OrderBook Module
 *
 * Clean API for order book management and subscriptions.
 * Single-symbol state - only one symbol at a time.
 */

import { TpSdkHybridObject } from "../../shared/TpSdkInstance.js";
import { processCallbacksThrottled, processFakeMessage } from "../../shared/callbackQueue.js";
import { SubscriptionManager } from "../../shared/SubscriptionManager.js";
import { createModuleValidator } from "../../shared/moduleUtils.js";
const validateInitialized = createModuleValidator('orderbook');

// Subscription manager for orderbook updates
const subscriptionManager = new SubscriptionManager(callback => TpSdkHybridObject.orderbookSubscribe(callback), () => TpSdkHybridObject.orderbookUnsubscribe());

// Internal wrapper functions
function setAggregationInternal(aggregation) {
  TpSdkHybridObject.orderbookConfigSetAggregation(aggregation);
}
function setDecimalsInternal(baseDecimals, quoteDecimals) {
  TpSdkHybridObject.orderbookConfigSetDecimals(baseDecimals, quoteDecimals);
}
function setSnapshotInternal(snapshot, baseDecimals, quoteDecimals) {
  TpSdkHybridObject.orderbookDataSetSnapshot(snapshot.bids, snapshot.asks, baseDecimals, quoteDecimals);
}
export const orderbook = {
  /**
   * Subscribe to order book updates
   *
   * @param callback - Callback function to receive updates
   * @param options - Optional subscription options
   * @param options.aggregation - Aggregation level (optional, will be set if provided)
   * @param options.decimals - Optional decimals configuration
   * @param options.decimals.base - Base asset decimals (optional)
   * @param options.decimals.quote - Quote asset decimals (optional)
   * @param options.snapshot - Initial snapshot data (optional, will be set if provided)
   * @returns Subscription ID for unsubscribing
   *
   * @example
   * ```typescript
   * const subId = TpSdk.orderbook.subscribe((data) => {
   *   // Handle order book updates
   * }, {
   *   aggregation: '0.01',
   *   decimals: { base: 8, quote: 2 },
   *   snapshot: { bids: [...], asks: [...] }
   * });
   *
   * // Later, unsubscribe
   * TpSdk.orderbook.unsubscribe(subId);
   * ```
   */
  subscribe: (callback, options) => {
    validateInitialized('subscribe');
    const {
      aggregation,
      decimals,
      snapshot
    } = options || {};

    // Subscribe callback first to ensure it's registered before callbacks are queued
    const subscriptionId = subscriptionManager.subscribe(callback);
    if (snapshot) {
      setSnapshotInternal(snapshot, decimals?.base, decimals?.quote);
      if (aggregation) {
        setAggregationInternal(aggregation);
      }
    } else {
      if (aggregation) {
        setAggregationInternal(aggregation);
      }
      if (decimals) {
        setDecimalsInternal(decimals.base, decimals.quote);
      }
    }

    // Process queued callbacks (C++ queues callbacks when snapshot/aggregation/decimals are set)
    if (snapshot || aggregation || decimals) {
      processCallbacksThrottled();
    }
    return subscriptionId;
  },
  /**
   * Unsubscribe from order book updates
   *
   * @param subscriptionId - Subscription ID returned from subscribe
   *
   * @example
   * ```typescript
   * TpSdk.orderbook.unsubscribe(subscriptionId);
   * ```
   */
  unsubscribe: subscriptionId => {
    subscriptionManager.unsubscribe(subscriptionId);
  },
  /**
   * Reset order book state
   *
   * @example
   * ```typescript
   * TpSdk.orderbook.reset();
   * ```
   */
  reset: () => {
    validateInitialized('reset');
    TpSdkHybridObject.orderbookReset();
  },
  /**
   * Set aggregation level for order book
   *
   * @param aggregation - Aggregation level (e.g., '0.01', '0.1', '1')
   *
   * @example
   * ```typescript
   * TpSdk.orderbook.setAggregation('0.01');
   * ```
   */
  setAggregation: aggregation => {
    validateInitialized('setAggregation');
    setAggregationInternal(aggregation);
    processCallbacksThrottled();
  },
  /**
   * Send fake orderbook data for testing
   *
   * Useful when WebSocket is not available or for testing UI.
   *
   * @param options - Configuration options
   * @param options.symbol - Trading pair symbol (default: 'ETH_USDT')
   * @param options.basePrice - Base price (default: 3000)
   * @param options.numLevels - Number of price levels (default: 20)
   * @param options.priceStep - Price step between levels (default: 0.5)
   * @param options.aggregation - Aggregation level (optional)
   *
   * @example
   * ```typescript
   * // Send fake orderbook data
   * TpSdk.orderbook.sendFakeData({
   *   symbol: 'ETH_USDT',
   *   basePrice: 3000,
   *   numLevels: 20,
   *   priceStep: 0.5,
   *   aggregation: '0.01',
   * });
   * ```
   */
  sendFakeData: options => {
    const symbol = options?.symbol ?? 'ETH_USDT';
    const basePrice = options?.basePrice ?? 3000;
    const numLevels = options?.numLevels ?? 20;
    const priceStep = options?.priceStep ?? 0.5;

    // Set aggregation if provided
    if (options?.aggregation) {
      setAggregationInternal(options.aggregation);
    }

    // Generate fake bids and asks
    const bids = [];
    const asks = [];
    for (let i = 1; i <= numLevels; i++) {
      const bidPrice = (basePrice - i * priceStep).toFixed(2);
      const askPrice = (basePrice + i * priceStep).toFixed(2);
      const quantity = (Math.random() * 10 + 1).toFixed(8);
      bids.push([bidPrice, quantity]);
      asks.push([askPrice, quantity]);
    }

    // Create fake depth update message in CXP format
    const timestamp = Date.now();
    const fakeMessage = JSON.stringify({
      stream: `${symbol}@depth`,
      data: {
        e: 'depthUpdate',
        E: timestamp,
        s: symbol,
        U: '1',
        u: '2',
        b: bids,
        a: asks
      }
    });

    // Send through C++ processing pipeline and process callbacks
    processFakeMessage(fakeMessage);
  }
};

// Re-export types for convenience
//# sourceMappingURL=orderbook.js.map