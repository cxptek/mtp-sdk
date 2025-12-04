import { processCallbacksThrottled } from '../shared/callbackQueue';
import { TpSdkHybridObject } from '../shared/TpSdkInstance';
import { kline } from './modules/kline';
import { orderbook } from './modules/orderbook';
import { ticker } from './modules/ticker';
import { tickerAll } from './modules/tickerAll';
import { trades } from './modules/trades';
import { userData } from './modules/userData';

const initFunction = (callback?: (response: 'success' | 'fail') => void) => {
  try {
    const handleInitializationResult = () => {
      queueMicrotask(() => {
        const isInit = TpSdkHybridObject.isInitialized?.() ?? false;
        callback?.(isInit ? 'success' : 'fail');
      });
    };

    TpSdkHybridObject.markInitialized(
      callback ? handleInitializationResult : undefined
    );
  } catch (error) {
    callback?.('fail');
    throw error;
  }
};

const parseMessageFunction = (messageJson: string) => {
  console.log(
    '[TpSdk] parseMessage called, message length=',
    messageJson.length
  );
  TpSdkHybridObject.processWebSocketMessage(messageJson);
  // Automatically process callbacks with throttling
  processCallbacksThrottled();
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
};

// Type assertion needed because we're dynamically adding TypeScript modules
// to the C++ TpSdk instance. The base TpSdk type only includes C++ methods,
// but we extend it with TypeScript modules at runtime.
Object.assign(TpSdkHybridObject, modules);

// Create typed TpSdk instance with all modules
// Merge TpSdkHybridObject (C++ methods) with TypeScript modules
// Using explicit intersection type for better IDE autocomplete support
export type TpSdkType = typeof TpSdkHybridObject & typeof modules;

const TpSdkInstance = TpSdkHybridObject as TpSdkType;
Object.assign(TpSdkInstance, modules);

export { TpSdkInstance as TpSdk };
