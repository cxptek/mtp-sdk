/**
 * Types Index
 *
 * Central export point for all type definitions
 */
export interface OrderBookLevel {
    price: number;
    quantity: number;
}
export interface OrderBookViewItem {
    priceStr: string | null;
    amountStr: string | null;
    cumulativeQuantity: string | null;
    cumulativeQuantityNum?: number;
    normalizedPercentage?: number;
}
export interface OrderBookViewResult {
    bids: OrderBookViewItem[];
    asks: OrderBookViewItem[];
    maxCumulativeQuantity: string;
    maxCumulativeQuantityNum?: number;
    maxCumulativeInv?: number;
}
export interface UpsertOrderBookResult {
    bids: OrderBookLevel[];
    asks: OrderBookLevel[];
}
export type DepthLevel = [string, string];
export interface SocketOrderBook {
    bids: DepthLevel[];
    asks: DepthLevel[];
    ts: number;
    wsTime?: number;
}
export interface DepthData {
    e: 'depthUpdate';
    E: number;
    s: string;
    U: string;
    u: string;
    b: [string, string][];
    a: [string, string][];
    dsTime: number;
}
/**
 * User Data Types (from userData WebSocket stream)
 *
 * These types represent data structures received from the userData stream,
 * including order updates.
 */
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
/**
 * WebSocket Message Types
 *
 * Types for WebSocket message processing, including message types,
 * message results, and protocol data.
 */
export type TradeSide = 'buy' | 'sell';
export declare enum WebSocketMessageType {
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
    UNKNOWN = 12
}
export declare const WebSocketMessageTypeString: Record<WebSocketMessageType, string>;
export interface OrderBookMessageData {
    symbol: string;
    bids: OrderBookLevel[];
    asks: OrderBookLevel[];
    timestamp?: number;
    isSnapshot: boolean;
    dsTime?: number;
    U?: string;
    u?: string;
    mergedResult?: UpsertOrderBookResult;
}
export interface TradeMessageData {
    symbol: string;
    price: string;
    quantity: string;
    side: TradeSide;
    timestamp: number;
    tradeId?: string;
}
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
    orderBookViewData?: OrderBookViewResult;
    tradeData?: TradeMessageData;
    tickerData?: TickerMessageData;
    klineData?: KlineMessageData;
    userData?: UserMessageData;
    protocolData?: ProtocolMessageDataNitro;
}
//# sourceMappingURL=index.d.ts.map