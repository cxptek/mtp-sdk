# @cxptek/tp-sdk

High-performance Trading Platform SDK for React Native mobile apps. Handles order book, trades, and WebSocket messages with optimized C++ backend for maximum performance.

## Why this SDK?

- ⚡ **High Performance**: Optimized C++ backend with simdjson, lock-free ring buffers, and object pools
- 📊 **Real-time Data**: Process WebSocket messages in C++ background threads, doesn't block JS thread
- 🔢 **Precision**: Uses double precision (~15-17 decimal digits) for trading calculations
- 🚀 **Non-blocking**: JS thread never blocks during message processing
- 📱 **React Native**: Native integration with Nitro Modules
- 💾 **Memory Efficient**: Zero-copy parsing, object pooling, and lock-free data structures
- 🔄 **Multi-Platform**: Supports both Binance and CXP exchange formats

## Architecture

### Optimized C++ Backend

The SDK uses a highly optimized C++ backend with:

- **simdjson**: Zero-copy JSON parsing using ondemand API
- **Lock-free Ring Buffers**: SPSC (Single Producer Single Consumer) ring buffers for each stream:
  - Depth: 4096 entries
  - Trades: 2048 entries
  - Ticker: 1024 entries
  - Mini Ticker: 1024 entries
  - User Data: 512 entries
  - Chart/Kline: 2048 entries
- **Object Pools**: Pre-allocated memory pools to eliminate dynamic allocations
- **Stream Processors**: Dedicated worker threads for each stream type
- **Memory Debug Tools**: Built-in memory tracking and leak detection

## Installation

```sh
npm install @cxptek/tp-sdk react-native-nitro-modules
```

### iOS

```sh
cd ios && pod install
```

### Android

No additional setup required. CMake automatically includes all necessary C++ sources.

## Usage

### Initialize SDK

```ts
import { TpSdk } from '@cxptek/tp-sdk';

// Initialize SDK first
TpSdk.init((response) => {
  if (response === 'success') {
    // SDK is ready to use
    console.log('SDK initialized');
  }
});
```

### Order Book with WebSocket

```ts
// Subscribe to receive order book updates
const subscriptionId = TpSdk.orderbook.subscribe((data) => {
  console.log('Bids:', data.bids);
  console.log('Asks:', data.asks);
  // data.bids[0].price      // Price as number
  // data.bids[0].quantity   // Quantity as number
});

// Connect WebSocket (supports both Binance and CXP)
const ws = new WebSocket('wss://api.cxptek.com/ws'); // or Binance URL

ws.onopen = () => {
  // CXP format
  ws.send(
    JSON.stringify({
      method: 'subscribe',
      params: ['ETH_USDT@depth'],
      id: 'subscribe_id',
    })
  );

  // Or Binance format
  // ws.send(
  //   JSON.stringify({
  //     method: 'SUBSCRIBE',
  //     params: ['ethusdt@depth50@100ms'],
  //     id: 1,
  //   })
  // );
};

ws.onmessage = (event) => {
  // Parse message in C++ background thread (non-blocking)
  TpSdk.parseMessage(event.data);
  // Callbacks are automatically processed
};

// Unsubscribe when done
TpSdk.orderbook.unsubscribe(subscriptionId);
```

### Trades

```ts
// Subscribe to trades
const subscriptionId = TpSdk.trades.subscribe((trades) => {
  // trades is an array of TradeMessageData
  trades.forEach((trade) => {
    console.log('New trade:', {
      symbol: trade.symbol,
      price: trade.price,
      quantity: trade.quantity,
      side: trade.side, // 'buy' | 'sell'
    });
  });
});

// Unsubscribe when done
TpSdk.trades.unsubscribe(subscriptionId);
```

### Other Subscriptions

```ts
// Mini Ticker
const tickerId = TpSdk.ticker.subscribe((ticker) => {
  console.log('Ticker:', ticker);
});
TpSdk.ticker.unsubscribe(tickerId);

// Mini Ticker Pair (array)
const pairId = TpSdk.tickerAll.subscribe((tickers) => {
  console.log('Tickers:', tickers); // Array of tickers
});
TpSdk.tickerAll.unsubscribe(pairId);

// Kline
const klineId = TpSdk.kline.subscribe((kline) => {
  console.log('Kline:', kline);
});
TpSdk.kline.unsubscribe(klineId);

// User Data (requires authentication)
const userDataId = TpSdk.userData.subscribe((userData) => {
  console.log('User data:', userData);
});
TpSdk.userData.unsubscribe(userDataId);
```

## Main API

### Initialization

```ts
TpSdk.init(callback: (response: 'success' | 'error') => void): void
```

### Order Book

```ts
TpSdk.orderbook.subscribe(callback: (data: OrderBookMessageData) => void): string
TpSdk.orderbook.unsubscribe(subscriptionId: string): void
```

### Trades

```ts
TpSdk.trades.subscribe(callback: (trades: TradeMessageData[]) => void): string
TpSdk.trades.unsubscribe(subscriptionId: string): void
```

### Ticker

```ts
TpSdk.ticker.subscribe(callback: (data: TickerMessageData) => void): string
TpSdk.ticker.unsubscribe(subscriptionId: string): void
```

### Ticker All (Pair)

```ts
TpSdk.tickerAll.subscribe(callback: (tickers: TickerMessageData[]) => void): string
TpSdk.tickerAll.unsubscribe(subscriptionId: string): void
```

### Kline

```ts
TpSdk.kline.subscribe(callback: (data: KlineMessageData) => void): string
TpSdk.kline.unsubscribe(subscriptionId: string): void
```

### User Data

```ts
TpSdk.userData.subscribe(callback: (data: UserMessageData) => void): string
TpSdk.userData.unsubscribe(subscriptionId: string): void
```

### WebSocket Processing

```ts
// Parse WebSocket message (automatically processes callbacks)
TpSdk.parseMessage(messageJson: string): void
```

## Performance

The SDK is optimized for high-frequency trading data:

- **Zero-copy parsing**: Uses `simdjson` ondemand API to avoid string copies
- **Lock-free data structures**: SPSC ring buffers for maximum throughput
- **Object pooling**: Pre-allocated memory to eliminate `malloc`/`free` overhead
- **Dedicated threads**: Each stream type has its own worker thread
- **Batched callbacks**: Reduces JS thread overhead with intelligent batching

## Supported Exchanges

- **CXP**: Native support with underscore symbol format (e.g., `ETH_USDT`)
- **Binance**: Full support with lowercase symbol format (e.g., `ethusdt`)

The SDK automatically detects the exchange format based on symbol naming.

## License

MIT
