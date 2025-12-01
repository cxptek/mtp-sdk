# SDK Modules

This directory contains all SDK modules organized by feature domain.

## Module Organization

### Market Data Modules
- **orderbook** - Order book management and aggregation
- **trades** - Trade history and real-time trades
- **kline** - Candlestick/OHLCV data
- **ticker** - 24h ticker data (single pair)
- **miniTickerPair** - 24h ticker data (multiple pairs)

### User Data Module
- **userData** - User account data and order management

### Note on Trading Pairs
Trading pairs are managed by the app (e.g., using Zustand). Decimals are passed as optional parameters to orderbook methods when needed.

## Module Pattern

Each module follows this pattern:

```typescript
export const moduleName = {
  // Core functionality
  method1: () => { ... },
  method2: () => { ... },
  
  // Subscriptions (if applicable)
  subscribe: (callback) => { ... },
  unsubscribe: () => { ... },
};
```

## Error Handling

All modules use the standardized error pattern from `shared/errors.ts`:
- `createSdkError()` - Standard error with context
- `createSdkErrorWithCode()` - Error with error code

## Usage

```typescript
import { TpSdk } from '@cxptek/tp-sdk';

// Initialize SDK
TpSdk.init((response) => {
  if (response === 'success') {
    // SDK ready
  }
});

// Market data
TpSdk.orderbook.config.setAggregation('BTC_USDT', '0.01');
TpSdk.orderbook.subscribe.onUpdate({ symbol: 'BTC_USDT' }, (data) => { ... });

// Orderbook with decimals (from app state)
const decimals = getDecimalsFromAppState('BTC_USDT');
TpSdk.orderbook.data.setSnapshot('BTC_USDT', snapshot, {
  decimals: decimals ? { base: decimals.base, quote: decimals.quote } : undefined
});

// User data
TpSdk.userData.subscribe((data) => { ... });
```





