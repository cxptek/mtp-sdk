import { useEffect, useCallback, useRef, useState } from 'react';
import {
  StyleSheet,
  View,
  Text,
  FlatList,
  TouchableOpacity,
} from 'react-native';
import { SafeAreaView, StatusBar } from 'react-native';
import { TpSdk } from '@cxptek/tp-sdk';
import { useTradingStore } from '../stores/useTradingStore';

interface Trade {
  symbol: string;
  price: string;
  quantity: string;
  side: 'buy' | 'sell';
  timestamp: number;
}

interface TradesScreenProps {
  binanceBaseUrl: string;
  defaultSymbol: string;
}

export function TradesScreen({
  binanceBaseUrl,
  defaultSymbol,
}: TradesScreenProps) {
  const trades = useTradingStore((state) => state.trades);
  const addTrade = useTradingStore((state) => state.addTrade);
  const wsRef = useRef<WebSocket | null>(null);
  const [isConnected, setIsConnected] = useState(false);

  // Auto-detect platform (Binance or CXP) from symbol format
  const isCXP =
    defaultSymbol.includes('_') &&
    defaultSymbol === defaultSymbol.toUpperCase();
  const platform = isCXP ? 'CXP' : 'Binance';

  // WebSocket connection - supports both Binance and CXP
  useEffect(() => {
    console.log(
      `[TradesScreen] Connecting to ${platform} WebSocket:`,
      binanceBaseUrl
    );

    const ws = new WebSocket(binanceBaseUrl);
    wsRef.current = ws;

    ws.onopen = () => {
      console.log(`[TradesScreen] ${platform} WebSocket connected`);
      setIsConnected(true);

      // Send SUBSCRIBE message for both Binance and CXP
      // Binance: method="SUBSCRIBE", id=1 (number)
      // CXP: method="subscribe", id="g8fur6z" (string)
      const subscribeMessage = isCXP
        ? {
            method: 'subscribe',
            params: [`${defaultSymbol}@trade`],
            id: `trades_${Date.now()}`,
          }
        : {
            method: 'SUBSCRIBE',
            params: [`${defaultSymbol}@trade`],
            id: 1,
          };

      ws.send(JSON.stringify(subscribeMessage));
      console.log('[TradesScreen] Sent SUBSCRIBE:', subscribeMessage);
    };

    ws.onmessage = (event) => {
      if (!event.data) {
        return;
      }

      try {
        // Check if it's a response to SUBSCRIBE (both Binance and CXP send confirmation)
        if (typeof event.data === 'string') {
          const data = JSON.parse(event.data);
          // Binance: {result: null, id: 1}
          // CXP: {result: null, id: "g8fur6z"} or similar
          if (
            data.result === null &&
            (data.id === 1 || typeof data.id === 'string')
          ) {
            return; // SUBSCRIBE confirmation, ignore
          }
        }

        // SDK will auto-detect and parse format (Binance or CXP)
        TpSdk.parseMessage(event.data);
      } catch (error) {
        console.error('[TradesScreen] Error processing message:', error);
      }
    };

    ws.onerror = (error) => {
      console.error('[TradesScreen] WebSocket error:', error);
      setIsConnected(false);
    };

    ws.onclose = (event) => {
      console.log('[TradesScreen] WebSocket closed, code:', event.code);
      setIsConnected(false);
    };

    return () => {
      console.log('[TradesScreen] Cleaning up WebSocket');
      if (
        ws.readyState === WebSocket.OPEN ||
        ws.readyState === WebSocket.CONNECTING
      ) {
        ws.close();
      }
      wsRef.current = null;
      setIsConnected(false);
    };
  }, [binanceBaseUrl, defaultSymbol]);

  // Subscribe to trades updates
  useEffect(() => {
    try {
      // Subscribe to trades
      const subscriptionId = TpSdk.trades.subscribe((tradesData) => {
        // tradesData is an array of TradeMessageData
        tradesData.forEach((tradeData) => {
          tradeData.data.forEach((trade) => {
            const tradeItem = {
              symbol: trade.symbol,
              price: trade.price,
              quantity: trade.quantity,
              // TradeSide is determined by isBuyerMaker
              side: (trade.isBuyerMaker ? 'sell' : 'buy') as const,
              timestamp: Math.floor(trade.tradeTime),
            };
            addTrade(tradeItem);
          });
        });
      });

      return () => {
        try {
          TpSdk.trades.unsubscribe(subscriptionId);
        } catch {
          // Ignore cleanup errors
        }
      };
    } catch {
      // Ignore initialization errors
    }
  }, [addTrade]);

  const renderTrade = ({ item }: { item: Trade }) => (
    <View style={styles.tradeItem}>
      <View style={styles.tradeHeader}>
        <Text
          style={[
            styles.tradeSide,
            item.side === 'buy' ? styles.tradeBuy : styles.tradeSell,
          ]}
        >
          {item.side.toUpperCase()}
        </Text>
        <Text style={styles.tradeTime}>
          {new Date(item.timestamp).toLocaleTimeString()}
        </Text>
      </View>
      <View style={styles.tradeDetails}>
        <Text style={styles.tradePrice}>{item.price}</Text>
        <Text style={styles.tradeQuantity}>× {item.quantity}</Text>
      </View>
    </View>
  );

  const symbolDisplay = defaultSymbol.toUpperCase().replace('USDT', '/USDT');

  return (
    <SafeAreaView style={styles.container}>
      <StatusBar barStyle="light-content" />

      <View style={styles.topHeader}>
        <View style={styles.symbolContainer}>
          <Text style={styles.symbolText}>{symbolDisplay}</Text>
          <Text style={styles.marketTypeText}>Trades</Text>
          {isConnected ? (
            <View style={styles.connectedIndicator} />
          ) : (
            <View style={styles.disconnectedIndicator} />
          )}
        </View>
      </View>

      <View style={styles.tradesHeader}>
        <Text style={styles.tradesHeaderText}>Recent Trades</Text>
      </View>

      <FlatList
        data={trades}
        renderItem={renderTrade}
        keyExtractor={(item, index) => `${item.timestamp}-${index}`}
        contentContainerStyle={styles.tradesList}
        ListEmptyComponent={
          <Text style={styles.emptyText}>No trades yet...</Text>
        }
      />
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#0f0f10',
  },
  topHeader: {
    paddingHorizontal: 16,
    paddingVertical: 12,
    backgroundColor: '#1a1a1b',
    borderBottomWidth: 1,
    borderBottomColor: '#2a2e39',
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
  },
  symbolContainer: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 8,
  },
  symbolText: {
    color: '#ffffff',
    fontSize: 18,
    fontWeight: '700',
    fontFamily: 'monospace',
  },
  marketTypeText: {
    color: '#848e9c',
    fontSize: 11,
    fontWeight: '500',
    paddingHorizontal: 6,
    paddingVertical: 2,
    backgroundColor: '#2a2e39',
    borderRadius: 3,
    textTransform: 'uppercase',
  },
  connectedIndicator: {
    width: 8,
    height: 8,
    borderRadius: 4,
    backgroundColor: '#0ecb81',
  },
  disconnectedIndicator: {
    width: 8,
    height: 8,
    borderRadius: 4,
    backgroundColor: '#f6465d',
  },
  tradesHeader: {
    paddingHorizontal: 16,
    paddingVertical: 10,
    backgroundColor: '#1a1a1b',
    borderBottomWidth: 1,
    borderBottomColor: '#2a2e39',
  },
  tradesHeaderText: {
    color: '#0ecb81',
    fontSize: 12,
    fontWeight: '600',
    textTransform: 'uppercase',
    letterSpacing: 0.5,
  },
  tradesList: {
    padding: 16,
  },
  tradeItem: {
    backgroundColor: '#1a1a1b',
    padding: 12,
    marginBottom: 8,
    borderRadius: 4,
    borderWidth: 1,
    borderColor: '#2a2e39',
  },
  tradeHeader: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginBottom: 8,
  },
  tradeSide: {
    fontSize: 11,
    fontWeight: '700',
    fontFamily: 'monospace',
    paddingHorizontal: 6,
    paddingVertical: 2,
    borderRadius: 3,
    textTransform: 'uppercase',
  },
  tradeBuy: {
    color: '#0ecb81',
    backgroundColor: '#0ecb8120',
  },
  tradeSell: {
    color: '#f6465d',
    backgroundColor: '#f6465d20',
  },
  tradeTime: {
    color: '#848e9c',
    fontSize: 10,
    fontFamily: 'monospace',
  },
  tradeDetails: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
  },
  tradePrice: {
    color: '#ffffff',
    fontSize: 14,
    fontWeight: '600',
    fontFamily: 'monospace',
  },
  tradeQuantity: {
    color: '#b7bdc6',
    fontSize: 12,
    fontFamily: 'monospace',
  },
  emptyText: {
    color: '#848e9c',
    fontSize: 12,
    textAlign: 'center',
    paddingVertical: 40,
    fontStyle: 'italic',
  },
});
