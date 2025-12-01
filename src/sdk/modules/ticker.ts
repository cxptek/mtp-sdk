/**
 * Ticker Module
 *
 * Clean API for single ticker subscriptions.
 * Single-symbol state - only one symbol at a time.
 */

import type { TickerMessageData } from '../../TpSdk.nitro';
import { TpSdkHybridObject } from '../../shared/TpSdkInstance';
import { processFakeMessage } from '../../shared/callbackQueue';
import { SubscriptionManager } from '../../shared/SubscriptionManager';
import { createModuleValidator } from '../../shared/moduleUtils';

const validateInitialized = createModuleValidator('ticker');

// Subscription manager for single ticker updates
const subscriptionManager = new SubscriptionManager<TickerMessageData>(
  (callback) => TpSdkHybridObject.miniTickerSubscribe(callback),
  () => TpSdkHybridObject.miniTickerUnsubscribe()
);

export const ticker = {
  /**
   * Subscribe to single ticker updates
   *
   * @param callback - Callback function to receive updates
   * @param options - Optional subscription options
   * @param options.decimals - Optional decimals configuration
   * @param options.decimals.price - Price decimals (optional)
   * @returns Subscription ID for unsubscribing
   *
   * @example
   * ```typescript
   * const subId = TpSdk.ticker.subscribe((ticker) => {
   *   console.log('Ticker updated:', ticker);
   * }, {
   *   decimals: { price: 2 }
   * });
   *
   * // Later, unsubscribe
   * TpSdk.ticker.unsubscribe(subId);
   * ```
   */
  subscribe: (
    callback: (data: TickerMessageData) => void,
    options?: {
      decimals?: {
        price?: number;
      };
    }
  ): string => {
    validateInitialized('subscribe');

    const { decimals } = options || {};

    // Set decimals if provided
    // Note: C++ only sets decimals value, doesn't queue callback
    // Callbacks will be triggered when new data arrives from WebSocket
    if (decimals) {
      TpSdkHybridObject.tickerConfigSetDecimals(decimals.price);
    }

    const subscriptionId = subscriptionManager.subscribe(callback);

    // Note: When data comes from WebSocket later, parseMessage() will automatically process callbacks
    // via processCallbacksThrottled(), so callbacks from socket data will be processed correctly
    // Decimals change doesn't trigger callback, so no need to process here

    return subscriptionId;
  },

  /**
   * Unsubscribe from single ticker updates
   *
   * @param subscriptionId - Subscription ID returned from subscribe
   *
   * @example
   * ```typescript
   * TpSdk.ticker.unsubscribe(subscriptionId);
   * ```
   */
  unsubscribe: (subscriptionId: string): void => {
    subscriptionManager.unsubscribe(subscriptionId);
  },

  /**
   * Send fake single ticker data for testing
   *
   * Useful when WebSocket is not available or for testing UI.
   *
   * @param options - Configuration options
   * @param options.symbol - Trading pair symbol (default: 'ETH_USDT')
   * @param options.basePrice - Base price (default: 3000)
   *
   * @example
   * ```typescript
   * // Send fake ticker data
   * TpSdk.ticker.sendFakeData();
   *
   * // Custom configuration
   * TpSdk.ticker.sendFakeData({
   *   symbol: 'BTC_USDT',
   *   basePrice: 50000,
   * });
   * ```
   */
  sendFakeData: (options?: { symbol?: string; basePrice?: number }) => {
    const symbol = options?.symbol ?? 'ETH_USDT';
    const basePrice = options?.basePrice ?? 3000;

    // Generate price variations
    const priceVariation = (Math.random() - 0.5) * 0.01 * basePrice;
    const lastPrice = (basePrice + priceVariation).toFixed(2);
    const bidPrice = (parseFloat(lastPrice) - 0.5).toFixed(2);
    const askPrice = (parseFloat(lastPrice) + 0.5).toFixed(2);
    const volume = (Math.random() * 1000 + 100).toFixed(2);
    const timestamp = Date.now();

    // Create fake message in CXP format
    const fakeMessage = JSON.stringify({
      stream: `${symbol}@miniTicker`,
      data: {
        e: '24hrTicker',
        s: symbol,
        c: lastPrice,
        b: bidPrice,
        a: askPrice,
        v: volume,
        E: timestamp,
      },
    });

    // Send through C++ processing pipeline and process callbacks
    processFakeMessage(fakeMessage);
  },
};
