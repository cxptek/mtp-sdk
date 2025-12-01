/**
 * Trades Module
 *
 * Clean API for trades management and subscriptions.
 * Single-symbol state - only one symbol at a time.
 */

import type { TradeMessageData } from '../../TpSdk.nitro';
import { TpSdkHybridObject } from '../../shared/TpSdkInstance';
import { processCallbacksThrottled } from '../../shared/callbackQueue';
import { SubscriptionManager } from '../../shared/SubscriptionManager';
import { createModuleValidator } from '../../shared/moduleUtils';

const validateInitialized = createModuleValidator('trades');

// Subscription manager for trades updates
const subscriptionManager = new SubscriptionManager<TradeMessageData>(
  (callback) => TpSdkHybridObject.tradesSubscribe(callback),
  () => TpSdkHybridObject.tradesUnsubscribe()
);

// Internal function to set snapshot
function setSnapshotInternal(snapshot: TradeMessageData[]): void {
  // Reset first
  TpSdkHybridObject.tradesReset();

  // Process each trade as WebSocket message to populate state
  snapshot.forEach((trade) => {
    const tradeMessage = JSON.stringify({
      e: 'trade',
      E: Date.now(),
      s: trade.symbol || '',
      t: trade.tradeId || '',
      p: trade.price || '0',
      q: trade.quantity || '0',
      m: (trade as any).isBuyerMaker || false,
      T: trade.timestamp || Date.now(),
    });

    TpSdkHybridObject.processWebSocketMessage(tradeMessage);
  });

  // Note: processWebSocketMessage queues callbacks, process them using throttled version
  processCallbacksThrottled();
}

export const trades = {
  /**
   * Subscribe to trade updates
   *
   * @param callback - Callback function to receive updates
   * @param options - Optional subscription options
   * @param options.decimals - Optional decimals configuration
   * @param options.decimals.price - Price decimals (optional)
   * @param options.decimals.quantity - Quantity decimals (optional)
   * @param options.snapshot - Initial trades data (optional, will be set if provided)
   * @returns Subscription ID for unsubscribing
   *
   * @example
   * ```typescript
   * const subId = TpSdk.trades.subscribe((trade) => {
   *   console.log('New trade:', trade);
   * }, {
   *   decimals: { price: 2, quantity: 8 },
   *   snapshot: initialTrades
   * });
   *
   * // Later, unsubscribe
   * TpSdk.trades.unsubscribe(subId);
   * ```
   */
  subscribe: (
    callback: (trade: TradeMessageData) => void,
    options?: {
      decimals?: {
        price?: number;
        quantity?: number;
      };
      snapshot?: TradeMessageData[];
    }
  ): string => {
    validateInitialized('subscribe');

    const { decimals, snapshot } = options || {};

    // Set decimals if provided
    if (decimals) {
      TpSdkHybridObject.tradesConfigSetDecimals(
        decimals.price,
        decimals.quantity
      );
    }

    if (snapshot) {
      setSnapshotInternal(snapshot);
    }

    const subscriptionId = subscriptionManager.subscribe(callback);

    // Process callbacks if decimals or snapshot was set
    // Note: When data comes from WebSocket later, parseMessage() will automatically process callbacks
    // via processCallbacksThrottled(), so callbacks from socket data will also be processed correctly
    if (decimals || snapshot) {
      processCallbacksThrottled();
    }

    // If snapshot provided, also trigger callbacks with initial data after processing
    if (snapshot && snapshot.length > 0) {
      requestAnimationFrame(() => {
        try {
          processCallbacksThrottled();
          snapshot.forEach((trade) => {
            callback(trade);
          });
        } catch {
          // Silent fail - stream updates will trigger callback
        }
      });
    }

    return subscriptionId;
  },

  /**
   * Unsubscribe from trade updates
   *
   * @param subscriptionId - Subscription ID returned from subscribe
   *
   * @example
   * ```typescript
   * TpSdk.trades.unsubscribe(subscriptionId);
   * ```
   */
  unsubscribe: (subscriptionId: string): void => {
    subscriptionManager.unsubscribe(subscriptionId);
  },

  /**
   * Reset trades state
   *
   * @example
   * ```typescript
   * TpSdk.trades.reset();
   * ```
   */
  reset: (): void => {
    validateInitialized('reset');
    TpSdkHybridObject.tradesReset();
  },
};
