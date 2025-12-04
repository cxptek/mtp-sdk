"use strict";

// ============================================================================
// WebSocket Message Type Enum
// ============================================================================

export let WebSocketMessageType = /*#__PURE__*/function (WebSocketMessageType) {
  // Market Data Types
  WebSocketMessageType[WebSocketMessageType["DEPTH"] = 0] = "DEPTH";
  WebSocketMessageType[WebSocketMessageType["TRADE"] = 1] = "TRADE";
  WebSocketMessageType[WebSocketMessageType["TICKER"] = 2] = "TICKER";
  WebSocketMessageType[WebSocketMessageType["KLINE"] = 3] = "KLINE";
  // User Data Types
  WebSocketMessageType[WebSocketMessageType["USER_ORDER_UPDATE"] = 4] = "USER_ORDER_UPDATE";
  // Unknown/Fallback (includes protocol messages and unrecognized types)
  WebSocketMessageType[WebSocketMessageType["UNKNOWN"] = 7] = "UNKNOWN";
  return WebSocketMessageType;
}({});

// ============================================================================
// WebSocket Message Type String Helper
// ============================================================================

export const WebSocketMessageTypeString = {
  // Market Data Types
  [WebSocketMessageType.DEPTH]: 'depth',
  [WebSocketMessageType.TRADE]: 'trade',
  [WebSocketMessageType.TICKER]: 'ticker',
  [WebSocketMessageType.KLINE]: 'kline',
  // User Data Types
  [WebSocketMessageType.USER_ORDER_UPDATE]: 'userOrderUpdate',
  // Unknown/Fallback (includes protocol messages and unrecognized types)
  [WebSocketMessageType.UNKNOWN]: 'unknown'
};
//# sourceMappingURL=index.js.map