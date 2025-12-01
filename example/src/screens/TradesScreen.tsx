import { useEffect, useCallback } from 'react';
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
import { generateCxpId } from '../utils/cxpId';

interface Trade {
  symbol: string;
  price: string;
  quantity: string;
  side: 'buy' | 'sell';
  timestamp: number;
}

interface TradesScreenProps {
  ws: WebSocket | null;
  processingStats: {
    totalProcessed: number;
    queueProcessed: number;
    lastMessageType: string;
    lastProcessTime: number;
  };
}

export function TradesScreen({ ws }: TradesScreenProps) {
  const trades = useTradingStore((state) => state.trades);
  const addTrade = useTradingStore((state) => state.addTrade);

  // Subscribe to trades WebSocket stream
  useEffect(() => {
    if (!ws || ws.readyState !== WebSocket.OPEN) return;

    try {
      // Subscribe to trade stream
      const tradeSubscribe = {
        method: 'subscribe',
        params: ['ETH_USDT@trade'],
        id: generateCxpId(),
      };
      ws.send(JSON.stringify(tradeSubscribe));
      console.log('[TradesScreen] Subscribed to ETH_USDT@trade');
    } catch (error) {
      console.error('[TradesScreen] Error subscribing:', error);
    }
  }, [ws]);

  // Subscribe to trades updates
  useEffect(() => {
    try {
      // Subscribe to trades
      const subscriptionId = TpSdk.trades.subscribe((data) => {
        const trade = {
          symbol: data.symbol,
          price: data.price,
          quantity: data.quantity,
          // TradeSide is serialized as string "buy" or "sell" in JS
          side: (data.side === 'buy' ? 'buy' : 'sell') as const,
          timestamp: Math.floor(data.timestamp),
        };
        addTrade(trade);
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

  // Function to send fake trade data
  const sendFakeData = useCallback(() => {
    try {
      TpSdk.trades.sendFakeData({
        symbol: 'ETH_USDT',
        basePrice: 3000,
        side: Math.random() > 0.5 ? 'buy' : 'sell',
      });
    } catch (error) {
      console.error('Error sending fake trade data:', error);
    }
  }, []);

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

  return (
    <SafeAreaView style={styles.container}>
      <StatusBar barStyle="light-content" />

      <View style={styles.topHeader}>
        <View style={styles.symbolContainer}>
          <Text style={styles.symbolText}>ETH/USDT</Text>
          <Text style={styles.marketTypeText}>Trades</Text>
        </View>
        <TouchableOpacity style={styles.fakeButton} onPress={sendFakeData}>
          <Text style={styles.fakeButtonText}>Fake Data</Text>
        </TouchableOpacity>
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
  statsContainer: {
    flexDirection: 'row',
    gap: 12,
  },
  statsText: {
    color: '#848e9c',
    fontSize: 10,
    fontFamily: 'monospace',
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
  fakeButton: {
    paddingHorizontal: 12,
    paddingVertical: 6,
    backgroundColor: '#f6465d',
    borderRadius: 4,
  },
  fakeButtonText: {
    color: '#ffffff',
    fontSize: 12,
    fontWeight: '600',
  },
});
