"use strict";

import { processCallbacks, processCallbacksThrottled } from "../shared/callbackQueue.js";
import { TpSdkHybridObject } from "../shared/TpSdkInstance.js";
import { kline } from "./modules/kline.js";
import { orderbook } from "./modules/orderbook.js";
import { ticker } from "./modules/ticker.js";
import { tickerAll } from "./modules/tickerAll.js";
import { trades } from "./modules/trades.js";
import { userData } from "./modules/userData.js";
const initFunction = callback => {
  try {
    const handleInitializationResult = () => {
      queueMicrotask(() => {
        const isInit = TpSdkHybridObject.isInitialized?.() ?? false;
        callback?.(isInit ? 'success' : 'fail');
      });
    };
    TpSdkHybridObject.markInitialized(callback ? handleInitializationResult : undefined);
  } catch (error) {
    callback?.('fail');
    throw error;
  }
};
const parseMessageFunction = (messageJson, options) => {
  console.log('[TpSdk] parseMessage called, message length=', messageJson.length);
  TpSdkHybridObject.processWebSocketMessage(messageJson);
  if (options?.autoProcess !== false) {
    processCallbacksThrottled();
  }
};
const processCallbacksFunction = () => {
  processCallbacks();
};
const modules = {
  orderbook,
  trades,
  ticker,
  tickerAll,
  kline,
  userData,
  init: initFunction,
  parseMessage: parseMessageFunction,
  processCallbacks: processCallbacksFunction
};

// Type assertion needed because we're dynamically adding TypeScript modules
// to the C++ TpSdk instance. The base TpSdk type only includes C++ methods,
// but we extend it with TypeScript modules at runtime.
Object.assign(TpSdkHybridObject, modules);

// Create typed TpSdk instance with all modules
// Merge TpSdkHybridObject (C++ methods) with TypeScript modules
// Using explicit intersection type for better IDE autocomplete support

const TpSdkInstance = TpSdkHybridObject;
Object.assign(TpSdkInstance, modules);
export { TpSdkInstance as TpSdk };
//# sourceMappingURL=index.js.map