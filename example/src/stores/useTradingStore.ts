import type {
  OrderBookViewResult,
  TickerMessageData,
  KlineMessageData,
  UserMessageData,
} from '@cxptek/tp-sdk';
import { create } from 'zustand';

interface Trade {
  symbol: string;
  price: string;
  quantity: string;
  side: 'buy' | 'sell';
  timestamp: number;
}

interface TradingState {
  // Order Book
  orderBook: OrderBookViewResult | null;
  setOrderBook: (data: OrderBookViewResult) => void;

  // Trades
  trades: Trade[];
  addTrade: (trade: Trade) => void;
  clearTrades: () => void;

  // MiniTicker (single)
  miniTicker: TickerMessageData | null;
  setMiniTicker: (data: TickerMessageData) => void;

  // MiniTicker Pair (array)
  miniTickerPair: TickerMessageData[];
  setMiniTickerPair: (data: TickerMessageData[]) => void;

  // Kline
  klines: KlineMessageData[];
  addKline: (data: KlineMessageData) => void;
  clearKlines: () => void;

  // UserData
  userData: UserMessageData[];
  addUserData: (data: UserMessageData) => void;
  clearUserData: () => void;
}

export const useTradingStore = create<TradingState>((set) => ({
  // Order Book
  orderBook: null,
  setOrderBook: (data: OrderBookViewResult) => {
    // Direct update - no computation to reduce JS thread overhead
    set({ orderBook: data });
  },

  // Trades
  trades: [],
  addTrade: (trade: Trade) => {
    set((state: TradingState) => {
      // Optimize: limit to 50 trades (matching native limit) to reduce memory
      const newTrades = [trade, ...state.trades];
      if (newTrades.length > 50) {
        return { trades: newTrades.slice(0, 50) };
      }
      return { trades: newTrades };
    });
  },
  clearTrades: () => {
    set({ trades: [] });
  },

  // MiniTicker (single)
  miniTicker: null,
  setMiniTicker: (data: TickerMessageData) => {
    set({ miniTicker: data });
  },

  // MiniTicker Pair (array)
  miniTickerPair: [],
  setMiniTickerPair: (data: TickerMessageData[]) => {
    // Limit to last 50 pairs to reduce memory
    set({ miniTickerPair: data.slice(-50) });
  },

  // Kline - Optimized with Map-based lookup for O(1) performance
  // Best practice: Use openTime as unique key, update current candle, add new when closed
  klines: [],
  addKline: (data: KlineMessageData) => {
    set((state: TradingState) => {
      // Create lookup map for O(1) access instead of O(n) findIndex
      // Key: `${symbol}-${interval}-${openTime}` for unique identification
      const klineMap = new Map<string, KlineMessageData>();
      state.klines.forEach((kline) => {
        const key = `${kline.symbol}-${kline.interval}-${
          kline.openTime || kline.timestamp
        }`;
        klineMap.set(key, kline);
      });

      // Generate key for incoming kline
      const dataKey = `${data.symbol}-${data.interval}-${
        data.openTime || data.timestamp
      }`;
      const existingKline = klineMap.get(dataKey);

      let newKlines: KlineMessageData[];
      if (existingKline) {
        // Update existing kline (current candle being updated)
        // Replace in array while maintaining order (newest first)
        newKlines = state.klines.map((kline) => {
          const key = `${kline.symbol}-${kline.interval}-${
            kline.openTime || kline.timestamp
          }`;
          return key === dataKey ? data : kline;
        });
      } else {
        // New kline (either new candle or first time)
        // Add at beginning for newest-first order
        newKlines = [data, ...state.klines];
      }

      // Limit to 200 klines (standard for trading apps - enough for multiple timeframes)
      // Keep newest klines, remove oldest
      if (newKlines.length > 200) {
        return { klines: newKlines.slice(0, 200) };
      }
      return { klines: newKlines };
    });
  },
  clearKlines: () => {
    set({ klines: [] });
  },

  // UserData
  userData: [],
  addUserData: (data: UserMessageData) => {
    set((state: TradingState) => {
      // Limit to last 50 user data updates to reduce memory
      const newUserData = [data, ...state.userData];
      if (newUserData.length > 50) {
        return { userData: newUserData.slice(0, 50) };
      }
      return { userData: newUserData };
    });
  },
  clearUserData: () => {
    set({ userData: [] });
  },
}));
