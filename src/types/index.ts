export interface DepthData {
  eventType: string;
  eventTime: number;
  symbol: string;
  firstUpdateId: string;
  finalUpdateId: string;
  bids: [string, string][];
  asks: [string, string][];
  dsTime: number;
}

export interface UserMessageData {
  stream: string; // "userData"
  wsTime: number; // WebSocket timestamp
  data: UserDataItem; // User data item
}

export interface UserDataItem {
  eventType: string;
  eventTime: number;
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
  createdAt?: number;
  updatedAt?: number;
  submittedAt?: number;
  dsTime?: number;
  triggerPrice?: string;
  conditionalOrderType?: string;
  timeInForce?: string;
  triggerStatus?: string;
  triggerDirection?: number;
  placeOrderReason?: string;
  contingencyType?: string;
  refId?: string;
  reduceVolume?: string;
  rejectedVolume?: string;
  rejectedBudget?: string;
}

export interface OrderBookMessageData {
  data: OrderBookDataItem;
  wsTime: number; // WebSocket timestamp
  stream: string; // Stream name, e.g., "ETH_USDT@depth"
}

export interface OrderBookDataItem {
  eventType: string;
  eventTime: number;
  symbol: string;
  firstUpdateId: string;
  finalUpdateId: string;
  bids: [string, string][];
  asks: [string, string][];
  dsTime: number;
}

export type TradeSide = 'buy' | 'sell';

export interface TradeMessageData {
  stream: string; // e.g., "ETH_USDT@trade"
  wsTime: number; // WebSocket timestamp
  dsTime: number; // Data source timestamp
  data: TradeDataItem[]; // Array of trade items
}

export interface TradeDataItem {
  eventType: string;
  eventTime: number;
  symbol: string;
  tradeId: string;
  price: string;
  quantity: string;
  tradeTime: number;
  isBuyerMaker: boolean;
}

export interface TickerMessageData {
  data: TickerDataItem;
  wsTime: number; // WebSocket timestamp
  stream: string; // e.g., "ETH_USDT@miniTicker"
}

export interface TickerDataItem {
  eventType: string;
  eventTime: number;
  symbol: string;
  closePrice: string;
  openPrice: string;
  highPrice: string;
  lowPrice: string;
  volume: string;
  quoteVolume: string;
  dsTime: number;
}

export interface AllTickerMessageData {
  stream: string; // "!miniTicker@arr"
  data: AllTickerItem[]; // Array of ticker items
}

export interface AllTickerItem {
  data: TickerDataItem;
  wsTime: number; // WebSocket timestamp
  stream: string; // e.g., "ETH_USDT@miniTicker"
}

export interface KlineMessageData {
  data: KlineDataWrapper;
  wsTime: number; // WebSocket timestamp
  stream: string; // e.g., "ETH_USDT@kline_1s"
}

export interface KlineDataWrapper {
  kline: KlineDataItem;
  eventType: string;
  eventTime: number;
  symbol: string;
  dsTime: number;
}

export interface KlineDataItem {
  openTime: number;
  closeTime: number;
  symbol: string;
  interval: string;
  firstTradeId: string;
  lastTradeId: string;
  openPrice: string;
  closePrice: string;
  highPrice: string;
  lowPrice: string;
  volume: string;
  quoteVolume: string;
  numberOfTrades: number;
  isClosed: boolean;
  takerBuyVolume: string;
  takerBuyQuoteVolume: string;
}

export interface ProtocolMessageData {
  method: string;
  id?: string;
  result?: any;
  error?: any;
  params?: any[];
  stream?: string;
}

export interface ProtocolMessageDataNitro {
  method: string;
  id?: string;
  stream?: string;
}

// ============================================================================
// WebSocket Message Type Enum
// ============================================================================

export enum WebSocketMessageType {
  // Market Data Types
  DEPTH = 0,
  TRADE = 1,
  TICKER = 2,
  KLINE = 3,

  // User Data Types
  USER_ORDER_UPDATE = 4,

  // Unknown/Fallback (includes protocol messages and unrecognized types)
  UNKNOWN = 7,
}

// ============================================================================
// WebSocket Message Type String Helper
// ============================================================================

export const WebSocketMessageTypeString: Record<WebSocketMessageType, string> =
  {
    // Market Data Types
    [WebSocketMessageType.DEPTH]: 'depth',
    [WebSocketMessageType.TRADE]: 'trade',
    [WebSocketMessageType.TICKER]: 'ticker',
    [WebSocketMessageType.KLINE]: 'kline',

    // User Data Types
    [WebSocketMessageType.USER_ORDER_UPDATE]: 'userOrderUpdate',

    // Unknown/Fallback (includes protocol messages and unrecognized types)
    [WebSocketMessageType.UNKNOWN]: 'unknown',
  };

export interface WebSocketMessageResult {
  type: WebSocketMessageType;
  raw: string;
  parsed: Record<string, any>;
  orderBookData?: OrderBookMessageData;
  tradeData?: TradeMessageData;
  tickerData?: TickerMessageData;
  klineData?: KlineMessageData;
  userData?: UserMessageData;
  protocolData?: ProtocolMessageData;
}

export interface WebSocketMessageResultNitro {
  type: WebSocketMessageType;
  raw: string;
  orderBookData?: OrderBookMessageData;
  tradeData?: TradeMessageData;
  tickerData?: TickerMessageData;
  klineData?: KlineMessageData;
  userData?: UserMessageData;
  protocolData?: ProtocolMessageDataNitro;
}
