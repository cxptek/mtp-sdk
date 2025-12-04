// Type aliases and helpers for example app
import type {
  OrderBookMessageData,
  TickerMessageData,
  KlineMessageData,
} from '@cxptek/tp-sdk';

// OrderBookViewResult - flattened version for UI components
export interface OrderBookViewResult {
  bids: Array<{
    priceStr: string;
    amountStr: string;
    cumulativeQuantity?: string;
  }>;
  asks: Array<{
    priceStr: string;
    amountStr: string;
    cumulativeQuantity?: string;
  }>;
  symbol?: string;
  maxCumulativeQuantity?: string;
}

// Helper to convert OrderBookMessageData to OrderBookViewResult
export function convertOrderBookData(
  data: OrderBookMessageData
): OrderBookViewResult {
  // Calculate cumulative quantities for animation background
  let bidCumulative = 0;
  let askCumulative = 0;
  const maxBidCumulative: number[] = [];
  const maxAskCumulative: number[] = [];

  const bids = data.data.bids.map(([price, quantity]) => {
    const qty = parseFloat(quantity) || 0;
    bidCumulative += qty;
    maxBidCumulative.push(bidCumulative);
    return {
      priceStr: price,
      amountStr: quantity,
      cumulativeQuantity: bidCumulative.toString(),
    };
  });

  const asks = data.data.asks.map(([price, quantity]) => {
    const qty = parseFloat(quantity) || 0;
    askCumulative += qty;
    maxAskCumulative.push(askCumulative);
    return {
      priceStr: price,
      amountStr: quantity,
      cumulativeQuantity: askCumulative.toString(),
    };
  });

  // Find max cumulative quantity for animation scaling
  const maxBid = Math.max(...maxBidCumulative, 0);
  const maxAsk = Math.max(...maxAskCumulative, 0);
  const maxCumulative = Math.max(maxBid, maxAsk);

  return {
    bids,
    asks,
    symbol: data.data.symbol,
    maxCumulativeQuantity:
      maxCumulative > 0 ? maxCumulative.toString() : undefined,
  };
}

// Helper to access kline properties
export function getKlineProperties(kline: KlineMessageData) {
  return {
    symbol: kline.data.symbol,
    interval: kline.data.kline.interval,
    open: kline.data.kline.openPrice,
    high: kline.data.kline.highPrice,
    low: kline.data.kline.lowPrice,
    close: kline.data.kline.closePrice,
    volume: kline.data.kline.volume,
    quoteVolume: kline.data.kline.quoteVolume,
    timestamp: kline.wsTime,
    openTime: kline.data.kline.openTime,
    closeTime: kline.data.kline.closeTime,
    isClosed: kline.data.kline.isClosed ? 'true' : 'false',
    trades: kline.data.kline.numberOfTrades?.toString(),
  };
}

// Helper to access ticker properties
export function getTickerProperties(ticker: TickerMessageData) {
  return {
    symbol: ticker.data.symbol,
    currentPrice: ticker.data.closePrice,
    openPrice: ticker.data.openPrice,
    highPrice: ticker.data.highPrice,
    lowPrice: ticker.data.lowPrice,
    priceChange: (
      parseFloat(ticker.data.closePrice) - parseFloat(ticker.data.openPrice)
    ).toString(),
    priceChangePercent: (
      ((parseFloat(ticker.data.closePrice) -
        parseFloat(ticker.data.openPrice)) /
        parseFloat(ticker.data.openPrice)) *
      100
    ).toString(),
    timestamp: ticker.wsTime,
  };
}
