"use strict";

/**
 * Types Index
 *
 * Central export point for all type definitions
 */

// ============================================================================
// Order Book Types
// ============================================================================

// Socket data types (from websocket)
// [price, quantity]

// API OrderBook format (from REST API)

// WebSocket DepthData format (from CXP WebSocket)

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

// WebSocket Message Types
// Note: Using numeric enum for Nitro C++ compatibility
export let WebSocketMessageType = /*#__PURE__*/function (WebSocketMessageType) {
  WebSocketMessageType[WebSocketMessageType["ORDER_BOOK_UPDATE"] = 0] = "ORDER_BOOK_UPDATE";
  WebSocketMessageType[WebSocketMessageType["ORDER_BOOK_SNAPSHOT"] = 1] = "ORDER_BOOK_SNAPSHOT";
  WebSocketMessageType[WebSocketMessageType["TRADE"] = 2] = "TRADE";
  WebSocketMessageType[WebSocketMessageType["TICKER"] = 3] = "TICKER";
  WebSocketMessageType[WebSocketMessageType["KLINE"] = 4] = "KLINE";
  WebSocketMessageType[WebSocketMessageType["USER_ORDER_UPDATE"] = 5] = "USER_ORDER_UPDATE";
  WebSocketMessageType[WebSocketMessageType["USER_TRADE_UPDATE"] = 6] = "USER_TRADE_UPDATE";
  WebSocketMessageType[WebSocketMessageType["USER_ACCOUNT_UPDATE"] = 7] = "USER_ACCOUNT_UPDATE";
  WebSocketMessageType[WebSocketMessageType["PROTOCOL_LOGIN"] = 8] = "PROTOCOL_LOGIN";
  WebSocketMessageType[WebSocketMessageType["PROTOCOL_SUBSCRIBE"] = 9] = "PROTOCOL_SUBSCRIBE";
  WebSocketMessageType[WebSocketMessageType["PROTOCOL_UNSUBSCRIBE"] = 10] = "PROTOCOL_UNSUBSCRIBE";
  WebSocketMessageType[WebSocketMessageType["PROTOCOL_ERROR"] = 11] = "PROTOCOL_ERROR";
  WebSocketMessageType[WebSocketMessageType["UNKNOWN"] = 12] = "UNKNOWN";
  return WebSocketMessageType;
}({});

// Helper to get string representation of message type (for logging/debugging)
export const WebSocketMessageTypeString = {
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
  [WebSocketMessageType.UNKNOWN]: 'unknown'
};

// Order Book Message Data

// Trade Message Data

// Ticker Message Data

// Kline Message Data

// Protocol Message Data (login, subscribe, etc.) - with 'any' for TypeScript usage

// Protocol Message Data (Nitro-compatible, no 'any' fields)

// Generic WebSocket Message Result (with 'any' for TypeScript usage)

// Generic WebSocket Message Result (Nitro-compatible, no 'any' fields)
//# sourceMappingURL=index.js.map