/**
 * Trades Module
 *
 * Clean API for trades management and subscriptions.
 * Single-symbol state - only one symbol at a time.
 */
import type { TradeMessageData } from '../../TpSdk.nitro';
export declare const trades: {
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
    subscribe: (callback: (trade: TradeMessageData) => void, options?: {
        decimals?: {
            price?: number;
            quantity?: number;
        };
        snapshot?: TradeMessageData[];
    }) => string;
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
    unsubscribe: (subscriptionId: string) => void;
    /**
     * Reset trades state
     *
     * @example
     * ```typescript
     * TpSdk.trades.reset();
     * ```
     */
    reset: () => void;
};
//# sourceMappingURL=trades.d.ts.map