# @cxptek/tp-sdk

High-performance Trading Platform SDK for React Native mobile apps. Handles order book, trades, and WebSocket messages with high performance.

## Why this SDK?

- ⚡ **High Performance**: Processes WebSocket messages in C++ background threads, doesn't block JS thread
- 📊 **Stateful Order Book**: Automatic order book state management with built-in aggregation and formatting
- 🔢 **Precision**: Uses double precision (~15-17 decimal digits) for trading calculations
- 🚀 **Non-blocking**: JS thread never blocks during message processing
- 📱 **React Native**: Native integration with Nitro Modules

## Installation

```sh
npm install @cxptek/tp-sdk react-native-nitro-modules
```

### iOS

```sh
cd ios && pod install
```

### Android

No additional setup required.

## Usage

### Order Book with WebSocket

```ts
import { TpSdk } from '@cxptek/tp-sdk';

// Configure order book
TpSdk.orderbook.setAggregation('0.01');
TpSdk.orderbook.setMaxRows(50);

// Subscribe to receive updates
TpSdk.orderbook.subscribe((data) => {
  console.log('Bids:', data.bids);
  console.log('Asks:', data.asks);
  // data.bids[0].priceStr      // "3,000.50" (formatted)
  // data.bids[0].amountStr     // "1.50000" (cumulative)
});

// Connect WebSocket
const ws = new WebSocket('wss://qc.cex-websocket.vcex.network/ws');

ws.onopen = () => {
  ws.send(
    JSON.stringify({
      method: 'subscribe',
      params: ['ETH_USDT@depth'],
      id: 'subscribe_id',
    })
  );
};

ws.onmessage = (event) => {
  // Parse message in C++ background thread (non-blocking)
  TpSdk.parseMessage(event.data);
  // Callbacks are automatically processed after 8ms (throttled)
};
```

### Trades

```ts
// Configure
TpSdk.trades.setLimit(50);

// Subscribe
TpSdk.trades.subscribe((trade) => {
  console.log('New trade:', {
    symbol: trade.symbol,
    price: trade.price,
    quantity: trade.quantity,
    side: trade.side, // 'buy' | 'sell'
  });
});

// Get current trades
const recentTrades = TpSdk.trades.getCurrent(20);
```

### Other Subscriptions

```ts
// Mini Ticker
TpSdk.miniTicker.subscribe((ticker) => {
  console.log('Ticker:', ticker);
});

// Mini Ticker Pair (array)
TpSdk.miniTickerPair.subscribe((tickers) => {
  console.log('Tickers:', tickers);
});

// Kline
TpSdk.kline.subscribe((kline) => {
  console.log('Kline:', kline);
});

// User Data (requires authentication)
TpSdk.userData.subscribe((userData) => {
  console.log('User data:', userData);
});
```

## Main API

### Order Book

```ts
TpSdk.orderbook.setAggregation('0.01'); // Aggregation level
TpSdk.orderbook.setMaxRows(50); // Maximum rows
TpSdk.orderbook.subscribe(callback);
TpSdk.orderbook.unsubscribe();
TpSdk.orderbook.getViewResult(); // Get current state
TpSdk.orderbook.reset(); // Reset state
```

### Trades

```ts
TpSdk.trades.setLimit(50);
TpSdk.trades.subscribe(callback);
TpSdk.trades.getCurrent(20); // Get last 20 trades
TpSdk.trades.reset();
```

### WebSocket Processing

```ts
// Automatically process callback queue (recommended)
TpSdk.parseMessage(messageJson);

// Or manual control
TpSdk.parseMessage(messageJson, { autoProcess: false });
TpSdk.processCallbacks(); // Call manually
```

## License

MIT
