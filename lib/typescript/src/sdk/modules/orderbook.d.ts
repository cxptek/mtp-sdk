/**
 * OrderBook Module
 *
 * Clean API for order book management and subscriptions.
 * Single-symbol state - only one symbol at a time.
 */
import type { OrderBookViewResult } from '../../TpSdk.nitro';
export declare const orderbook: {
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
    subscribe: (callback: (data: OrderBookViewResult) => void, options?: {
        aggregation?: string;
        decimals?: {
            base?: number;
            quote?: number;
        };
        snapshot?: {
            bids: [string, string][];
            asks: [string, string][];
        };
    }) => string;
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
    unsubscribe: (subscriptionId: string) => void;
    /**
     * Reset order book state
     *
     * @example
     * ```typescript
     * TpSdk.orderbook.reset();
     * ```
     */
    reset: () => void;
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
    setAggregation: (aggregation: string) => void;
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
    sendFakeData: (options?: {
        symbol?: string;
        basePrice?: number;
        numLevels?: number;
        priceStep?: number;
        aggregation?: string;
    }) => void;
};
export type { OrderBookViewResult };
//# sourceMappingURL=orderbook.d.ts.map