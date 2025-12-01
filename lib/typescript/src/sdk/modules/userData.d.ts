/**
 * UserData Module
 *
 * Clean API for user data subscriptions (order updates).
 */
import type { UserMessageData } from '../../TpSdk.nitro';
export declare const userData: {
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
    subscribe: (callback: (data: UserMessageData) => void) => string;
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
    unsubscribe: (subscriptionId: string) => void;
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
    sendFakeData: () => void;
};
//# sourceMappingURL=userData.d.ts.map