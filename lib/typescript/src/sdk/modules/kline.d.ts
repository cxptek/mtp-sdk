/**
 * Kline Module
 *
 * Clean API for kline/candlestick management and subscriptions.
 * Single-symbol state - only one symbol at a time.
 */
import type { KlineMessageData } from '../../TpSdk.nitro';
export declare const kline: {
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
    subscribe: (callback: (data: KlineMessageData) => void, options?: {
        snapshot?: KlineMessageData;
    }) => string;
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
    unsubscribe: (subscriptionId: string) => void;
};
//# sourceMappingURL=kline.d.ts.map