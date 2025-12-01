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
import type { TickerMessageData } from '@cxptek/tp-sdk';
import { generateCxpId } from '../utils/cxpId';

interface MiniTickerPairScreenProps {
  ws: WebSocket | null;
  processingStats: {
    totalProcessed: number;
    queueProcessed: number;
    lastMessageType: string;
    lastProcessTime: number;
  };
}

export function MiniTickerPairScreen({ ws }: MiniTickerPairScreenProps) {
  const miniTickerPair = useTradingStore((state) => state.miniTickerPair);
  const setMiniTickerPair = useTradingStore((state) => state.setMiniTickerPair);

  // Subscribe to miniTickerPair WebSocket stream
  useEffect(() => {
    if (!ws || ws.readyState !== WebSocket.OPEN) return;

    try {
      // Subscribe to miniTicker pair stream (array)
      const miniTickerPairSubscribe = {
        method: 'subscribe',
        params: ['!miniTicker@arr'],
        id: generateCxpId(),
      };
      ws.send(JSON.stringify(miniTickerPairSubscribe));
      console.log('[MiniTickerPairScreen] Subscribed to !miniTicker@arr');
    } catch (error) {
      console.error('[MiniTickerPairScreen] Error subscribing:', error);
    }
  }, [ws]);

  // Subscribe to miniTickerPair updates
  useEffect(() => {
    try {
      TpSdk.miniTickerPair.subscribe((data) => {
        setMiniTickerPair(data);
      });

      return () => {
        try {
          TpSdk.miniTickerPair.unsubscribe();
        } catch {
          // Ignore cleanup errors
        }
      };
    } catch {
      // Ignore initialization errors
    }
  }, [setMiniTickerPair]);

  // Function to send fake miniTicker pair data
  const sendFakeData = useCallback(() => {
    try {
      TpSdk.miniTickerPair.sendFakeData({
        symbols: ['ETH_USDT', 'BTC_USDT', 'BNB_USDT'],
      });
    } catch (error) {
      console.error('Error sending fake miniTicker pair data:', error);
    }
  }, []);

  const formatPrice = (price: string) => {
    if (!price) return 'N/A';
    return parseFloat(price).toLocaleString('en-US', {
      minimumFractionDigits: 2,
      maximumFractionDigits: 8,
    });
  };

  const renderTicker = ({ item }: { item: TickerMessageData }) => (
    <View style={styles.tickerItem}>
      <View style={styles.tickerHeader}>
        <Text style={styles.tickerSymbol}>{item.symbol || 'N/A'}</Text>
        <Text style={styles.tickerTimestamp}>
          {item.timestamp
            ? new Date(item.timestamp).toLocaleTimeString()
            : 'N/A'}
        </Text>
      </View>

      <View style={styles.priceRow}>
        <View style={styles.priceColumn}>
          <Text style={styles.priceLabel}>Price</Text>
          <Text style={[styles.priceValue, styles.currentPrice]}>
            {formatPrice(item.currentPrice)}
          </Text>
        </View>

        <View style={styles.priceColumn}>
          <Text style={styles.priceLabel}>High</Text>
          <Text style={[styles.priceValue, styles.highPrice]}>
            {formatPrice(item.highPrice)}
          </Text>
        </View>

        <View style={styles.priceColumn}>
          <Text style={styles.priceLabel}>Low</Text>
          <Text style={[styles.priceValue, styles.lowPrice]}>
            {formatPrice(item.lowPrice)}
          </Text>
        </View>

        <View style={styles.priceColumn}>
          <Text style={styles.priceLabel}>Change %</Text>
          <Text
            style={[
              styles.priceValue,
              parseFloat(item.priceChangePercent || '0') >= 0
                ? styles.positiveChange
                : styles.negativeChange,
            ]}
          >
            {formatPrice(item.priceChangePercent)}%
          </Text>
        </View>
      </View>
    </View>
  );

  return (
    <SafeAreaView style={styles.container}>
      <StatusBar barStyle="light-content" />

      <View style={styles.topHeader}>
        <View style={styles.symbolContainer}>
          <Text style={styles.symbolText}>All Pairs</Text>
          <Text style={styles.marketTypeText}>Mini Ticker Pair</Text>
        </View>
        <TouchableOpacity style={styles.fakeButton} onPress={sendFakeData}>
          <Text style={styles.fakeButtonText}>Fake Data</Text>
        </TouchableOpacity>
      </View>

      <View style={styles.listHeader}>
        <Text style={styles.listHeaderText}>Ticker Pairs</Text>
      </View>

      <FlatList
        data={miniTickerPair}
        renderItem={renderTicker}
        keyExtractor={(item, index) =>
          `${item.symbol}-${item.timestamp}-${index}`
        }
        contentContainerStyle={styles.listContent}
        ListEmptyComponent={
          <View style={styles.emptyContainer}>
            <Text style={styles.emptyText}>No ticker pairs yet...</Text>
            <Text style={styles.emptySubtext}>
              Waiting for !miniTicker@arr stream
            </Text>
          </View>
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
  listHeader: {
    paddingHorizontal: 16,
    paddingVertical: 10,
    backgroundColor: '#1a1a1b',
    borderBottomWidth: 1,
    borderBottomColor: '#2a2e39',
  },
  listHeaderText: {
    color: '#0ecb81',
    fontSize: 12,
    fontWeight: '600',
    textTransform: 'uppercase',
    letterSpacing: 0.5,
  },
  listContent: {
    padding: 16,
  },
  tickerItem: {
    backgroundColor: '#1a1a1b',
    padding: 12,
    marginBottom: 8,
    borderRadius: 4,
    borderWidth: 1,
    borderColor: '#2a2e39',
  },
  tickerHeader: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginBottom: 12,
  },
  tickerSymbol: {
    color: '#ffffff',
    fontSize: 14,
    fontWeight: '700',
    fontFamily: 'monospace',
  },
  tickerTimestamp: {
    color: '#848e9c',
    fontSize: 10,
    fontFamily: 'monospace',
  },
  priceRow: {
    flexDirection: 'row',
    gap: 12,
  },
  priceColumn: {
    flex: 1,
  },
  priceLabel: {
    color: '#848e9c',
    fontSize: 10,
    fontWeight: '500',
    marginBottom: 4,
    textTransform: 'uppercase',
  },
  priceValue: {
    fontSize: 13,
    fontWeight: '600',
    fontFamily: 'monospace',
  },
  currentPrice: {
    color: '#ffffff',
  },
  highPrice: {
    color: '#0ecb81',
  },
  lowPrice: {
    color: '#f6465d',
  },
  positiveChange: {
    color: '#0ecb81',
  },
  negativeChange: {
    color: '#f6465d',
  },
  emptyContainer: {
    paddingVertical: 60,
    alignItems: 'center',
  },
  emptyText: {
    color: '#848e9c',
    fontSize: 16,
    fontWeight: '500',
    marginBottom: 8,
  },
  emptySubtext: {
    color: '#5a5f6a',
    fontSize: 12,
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
