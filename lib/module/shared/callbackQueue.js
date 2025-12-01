"use strict";

/**
 * Callback Queue Processing Utilities
 *
 * Centralized utilities for processing callback queues.
 */

import { TpSdkHybridObject } from "./TpSdkInstance.js";
let callbackQueueRafId = null;

/**
 * Process callback queue
 *
 * @param delayMs - Optional delay in milliseconds. If undefined or 0, processes immediately (default: 0)
 *                  If delayMs > 0, uses requestAnimationFrame for better FPS synchronization
 */
export function processCallbacks(delayMs) {
  if (delayMs === undefined || delayMs === 0) {
    TpSdkHybridObject.processCallbackQueue();
  } else {
    // Use requestAnimationFrame for better FPS synchronization
    requestAnimationFrame(() => {
      TpSdkHybridObject.processCallbackQueue();
    });
  }
}

/**
 * Process callback queue with RAF throttling
 * Ensures only one RAF callback is scheduled at a time
 *
 * @returns void
 */
export function processCallbacksThrottled() {
  if (!callbackQueueRafId) {
    callbackQueueRafId = requestAnimationFrame(() => {
      try {
        console.log('[callbackQueue] Processing callback queue');
        TpSdkHybridObject.processCallbackQueue();
      } catch (err) {
        console.error('[TpSdk] Error processing callback queue:', err);
      } finally {
        callbackQueueRafId = null;
      }
    });
  }
}

/**
 * Process a fake WebSocket message and trigger callbacks
 *
 * @param fakeMessage - JSON string of the fake message
 * @param delayMs - Delay before processing callbacks (default: 0, uses requestAnimationFrame if > 0)
 */
export function processFakeMessage(fakeMessage, delayMs = 0) {
  // Send through C++ processing pipeline (non-blocking, processes in background thread)
  TpSdkHybridObject.processWebSocketMessage(fakeMessage);

  // Process callback queue using requestAnimationFrame for better FPS
  processCallbacks(delayMs);
}
//# sourceMappingURL=callbackQueue.js.map