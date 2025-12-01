/**
 * Types Index
 *
 * Central export point for all type definitions
 */

// ============================================================================
// Order Book Types
// ============================================================================

export interface OrderBookLevel {
  price: number;
  quantity: number;
}

export interface OrderBookViewItem {
  priceStr: string | null;
  amountStr: string | null;
  cumulativeQuantity: string | null;
}

export interface OrderBookViewResult {
  bids: OrderBookViewItem[];
  asks: OrderBookViewItem[];
  maxCumulativeQuantity: string;
}

export interface UpsertOrderBookResult {
  bids: OrderBookLevel[];
  asks: OrderBookLevel[];
}

// Socket data types (from websocket)
export type DepthLevel = [string, string]; // [price, quantity]

// API OrderBook format (from REST API)
export interface SocketOrderBook {
  bids: DepthLevel[]; // sorted desc by price
  asks: DepthLevel[]; // sorted asc by price
  ts: number;
  wsTime?: number;
}

// WebSocket DepthData format (from CXP WebSocket)
export interface DepthData {
  e: 'depthUpdate';
  E: number; // Event time
  s: string; // Symbol, e.g., "ETH_USDT"
  U: string; // First update ID
  u: string; // Final update ID
  b: [string, string][]; // Bids: [price, quantity][]
  a: [string, string][]; // Asks: [price, quantity][]
  dsTime: number; // WebSocket time
}

// ============================================================================
// User Data Types (from userData WebSocket stream)
// ============================================================================

/**
 * User Data Types (from userData WebSocket stream)
 *
 * These types represent data structures received from the userData stream,
 * including order updates.
 */

// User Message Data (with 'any' fields for TypeScript usage)
export interface UserMessageData {
  id?: string;
  userId?: string;
  symbolCode?: string;
  action?: string;
  type?: string;
  status?: string;
  price?: string;
  quantity?: string;
  baseFilled?: string;
  quoteFilled?: string;
  quoteQuantity?: string;
  fee?: string;
  feeAsset?: string;
  matchingPrice?: string;
  avgPrice?: string;
  avrPrice?: string;
  isCancelAll?: boolean;
  canceledBy?: string;
  createdAt?: string;
  updatedAt?: string;
  submittedAt?: string;
  dsTime?: string;
  triggerPrice?: string;
  conditionalOrderType?: string;
  timeInForce?: string;
  triggerStatus?: string;
  triggerDirection?: string | number;
  placeOrderReason?: string;
  contingencyType?: string;
  refId?: string;
  reduceVolume?: string;
  rejectedVolume?: string;
  rejectedBudget?: string;
  e?: string;
  E?: string;
  eventType?: string;
  event?: string;
  stream?: string;
}

// ============================================================================
// WebSocket Message Types
// ============================================================================

/**
 * WebSocket Message Types
 *
 * Types for WebSocket message processing, including message types,
 * message results, and protocol data.
 */

// Trade Side Type (extracted for Nitro compatibility)
export type TradeSide = 'buy' | 'sell';

// WebSocket Message Types
// Note: Using numeric enum for Nitro C++ compatibility
export enum WebSocketMessageType {
  ORDER_BOOK_UPDATE = 0,
  ORDER_BOOK_SNAPSHOT = 1,
  TRADE = 2,
  TICKER = 3,
  KLINE = 4,
  USER_ORDER_UPDATE = 5,
  USER_TRADE_UPDATE = 6,
  USER_ACCOUNT_UPDATE = 7,
  PROTOCOL_LOGIN = 8,
  PROTOCOL_SUBSCRIBE = 9,
  PROTOCOL_UNSUBSCRIBE = 10,
  PROTOCOL_ERROR = 11,
  UNKNOWN = 12,
}

// Helper to get string representation of message type (for logging/debugging)
export const WebSocketMessageTypeString: Record<WebSocketMessageType, string> =
  {
    [WebSocketMessageType.ORDER_BOOK_UPDATE]: 'orderBookUpdate',
    [WebSocketMessageType.ORDER_BOOK_SNAPSHOT]: 'orderBookSnapshot',
    [WebSocketMessageType.TRADE]: 'trade',
    [WebSocketMessageType.TICKER]: 'ticker',
    [WebSocketMessageType.KLINE]: 'kline',
    [WebSocketMessageType.USER_ORDER_UPDATE]: 'userOrderUpdate',
    [WebSocketMessageType.USER_TRADE_UPDATE]: 'userTradeUpdate',
    [WebSocketMessageType.USER_ACCOUNT_UPDATE]: 'userAccountUpdate',
    [WebSocketMessageType.PROTOCOL_LOGIN]: 'protocolLogin',
    [WebSocketMessageType.PROTOCOL_SUBSCRIBE]: 'protocolSubscribe',
    [WebSocketMessageType.PROTOCOL_UNSUBSCRIBE]: 'protocolUnsubscribe',
    [WebSocketMessageType.PROTOCOL_ERROR]: 'protocolError',
    [WebSocketMessageType.UNKNOWN]: 'unknown',
  };

// Order Book Message Data
export interface OrderBookMessageData {
  symbol: string;
  bids: OrderBookLevel[];
  asks: OrderBookLevel[];
  timestamp?: number;
  isSnapshot: boolean; // true for snapshot, false for update
  // CXP specific fields
  dsTime?: number; // CXP depth update timestamp
  U?: string; // First update ID
  u?: string; // Last update ID
  // For merging with previous state
  mergedResult?: UpsertOrderBookResult; // Only if previous state provided
}

// Trade Message Data
export interface TradeMessageData {
  symbol: string;
  price: string;
  quantity: string;
  side: TradeSide;
  timestamp: number;
  tradeId?: string;
}

// Ticker Message Data
export interface TickerMessageData {
  symbol: string;
  currentPrice: string;
  openPrice: string;
  highPrice: string;
  lowPrice: string;
  volume: string;
  quoteVolume: string;
  priceChange: string;
  priceChangePercent: string;
  timestamp: number;
}

// Kline Message Data
export interface KlineMessageData {
  symbol: string;
  interval: string;
  open: string;
  high: string;
  low: string;
  close: string;
  volume: string;
  quoteVolume?: string;
  trades?: string;
  openTime?: string;
  closeTime?: string;
  isClosed?: string;
  timestamp: number;
  wsTime?: string;
}

// Protocol Message Data (login, subscribe, etc.) - with 'any' for TypeScript usage
export interface ProtocolMessageData {
  method: string;
  id?: string;
  result?: any; // Using 'any' instead of 'unknown' for Nitro compatibility
  error?: any; // Using 'any' instead of 'unknown' for Nitro compatibility
  params?: any[]; // Using 'any' instead of 'unknown' for Nitro compatibility
  stream?: string; // Stream name from response (e.g., "USDT_KDG@depth")
}

// Protocol Message Data (Nitro-compatible, no 'any' fields)
export interface ProtocolMessageDataNitro {
  method: string;
  id?: string;
  stream?: string;
}

// Generic WebSocket Message Result (with 'any' for TypeScript usage)
export interface WebSocketMessageResult {
  type: WebSocketMessageType;
  raw: string; // Original JSON string
  parsed: Record<string, any>; // Parsed JSON object (using 'any' instead of 'unknown' for Nitro compatibility)
  // Type-specific data (only one will be populated based on type)
  orderBookData?: OrderBookMessageData;
  tradeData?: TradeMessageData;
  tickerData?: TickerMessageData;
  klineData?: KlineMessageData;
  userData?: UserMessageData;
  protocolData?: ProtocolMessageData;
}

// Generic WebSocket Message Result (Nitro-compatible, no 'any' fields)
export interface WebSocketMessageResultNitro {
  type: WebSocketMessageType;
  raw: string;
  orderBookData?: OrderBookMessageData;
  orderBookViewData?: OrderBookViewResult;
  tradeData?: TradeMessageData;
  tickerData?: TickerMessageData;
  klineData?: KlineMessageData;
  userData?: UserMessageData;
  protocolData?: ProtocolMessageDataNitro;
}
