/**
 * Callback Queue Processing Utilities
 *
 * Centralized utilities for processing callback queues.
 */
/**
 * Process callback queue
 *
 * @param delayMs - Optional delay in milliseconds. If undefined or 0, processes immediately (default: 0)
 *                  If delayMs > 0, uses requestAnimationFrame for better FPS synchronization
 */
export declare function processCallbacks(delayMs?: number): void;
/**
 * Process callback queue with RAF throttling
 * Ensures only one RAF callback is scheduled at a time
 *
 * @returns void
 */
export declare function processCallbacksThrottled(): void;
/**
 * Process a fake WebSocket message and trigger callbacks
 *
 * @param fakeMessage - JSON string of the fake message
 * @param delayMs - Delay before processing callbacks (default: 0, uses requestAnimationFrame if > 0)
 */
export declare function processFakeMessage(fakeMessage: string, delayMs?: number): void;
//# sourceMappingURL=callbackQueue.d.ts.map