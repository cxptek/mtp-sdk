/**
 * TickerAll Module
 *
 * Clean API for all tickers (array) subscriptions.
 * Handles !miniTicker@arr stream.
 */

import type { TickerMessageData } from '../../TpSdk.nitro';
import { TpSdkHybridObject } from '../../shared/TpSdkInstance';
import { processFakeMessage } from '../../shared/callbackQueue';
import { SubscriptionManager } from '../../shared/SubscriptionManager';
import { createModuleValidator } from '../../shared/moduleUtils';

const validateInitialized = createModuleValidator('tickerAll');

// Subscription manager for all tickers (array) updates
const subscriptionManager = new SubscriptionManager<TickerMessageData[]>(
  (callback) => TpSdkHybridObject.miniTickerPairSubscribe(callback),
  () => TpSdkHybridObject.miniTickerPairUnsubscribe()
);

export const tickerAll = {
  /**
   * Subscribe to all ticker updates (array)
   *
   * @param callback - Callback function to receive updates
   * @returns Subscription ID for unsubscribing
   *
   * @example
   * ```typescript
   * const subId = TpSdk.tickerAll.subscribe((tickers) => {
   *   console.log('All tickers updated:', tickers);
   * });
   *
   * // Later, unsubscribe
   * TpSdk.tickerAll.unsubscribe(subId);
   * ```
   */
  subscribe: (callback: (data: TickerMessageData[]) => void): string => {
    validateInitialized('subscribe');
    return subscriptionManager.subscribe(callback);
  },

  /**
   * Unsubscribe from all ticker updates
   *
   * @param subscriptionId - Subscription ID returned from subscribe
   *
   * @example
   * ```typescript
   * TpSdk.tickerAll.unsubscribe(subscriptionId);
   * ```
   */
  unsubscribe: (subscriptionId: string): void => {
    subscriptionManager.unsubscribe(subscriptionId);
  },

  /**
   * Send fake ticker array data for testing
   *
   * Useful when WebSocket is not available or for testing UI.
   *
   * @param options - Configuration options
   * @param options.symbols - Array of trading pairs (default: ['ETH_USDT', 'BTC_USDT'])
   *
   * @example
   * ```typescript
   * // Send fake ticker array data
   * TpSdk.tickerAll.sendFakeData();
   *
   * // Custom configuration
   * TpSdk.tickerAll.sendFakeData({
   *   symbols: ['ETH_USDT', 'BTC_USDT', 'BNB_USDT'],
   * });
   * ```
   */
  sendFakeData: (options?: { symbols?: string[] }) => {
    const symbols = options?.symbols ?? ['ETH_USDT', 'BTC_USDT'];
    const basePrices: Record<string, number> = {
      ETH_USDT: 3000,
      BTC_USDT: 50000,
    };

    const timestamp = Date.now();
    const tickers = symbols.map((symbol) => {
      const basePrice = basePrices[symbol] ?? 1000;
      const priceVariation = (Math.random() - 0.5) * 0.01 * basePrice;
      const lastPrice = (basePrice + priceVariation).toFixed(2);
      const bidPrice = (parseFloat(lastPrice) - 0.5).toFixed(2);
      const askPrice = (parseFloat(lastPrice) + 0.5).toFixed(2);
      const volume = (Math.random() * 1000 + 100).toFixed(2);

      return {
        e: '24hrTicker',
        s: symbol,
        c: lastPrice,
        b: bidPrice,
        a: askPrice,
        v: volume,
        E: timestamp,
      };
    });

    // Create fake message in CXP format
    const fakeMessage = JSON.stringify({
      stream: '!miniTicker@arr',
      data: tickers,
    });

    // Send through C++ processing pipeline and process callbacks
    processFakeMessage(fakeMessage);
  },
};
