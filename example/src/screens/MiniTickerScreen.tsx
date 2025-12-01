import { useEffect, useCallback } from 'react';
import {
  StyleSheet,
  View,
  Text,
  ScrollView,
  TouchableOpacity,
} from 'react-native';
import { SafeAreaView, StatusBar } from 'react-native';
import { TpSdk } from '@cxptek/tp-sdk';
import { useTradingStore } from '../stores/useTradingStore';
import { generateCxpId } from '../utils/cxpId';

interface MiniTickerScreenProps {
  ws: WebSocket | null;
  processingStats: {
    totalProcessed: number;
    queueProcessed: number;
    lastMessageType: string;
    lastProcessTime: number;
  };
}

export function MiniTickerScreen({ ws }: MiniTickerScreenProps) {
  const miniTicker = useTradingStore((state) => state.miniTicker);
  const setMiniTicker = useTradingStore((state) => state.setMiniTicker);

  // Subscribe to miniTicker WebSocket stream
  useEffect(() => {
    if (!ws || ws.readyState !== WebSocket.OPEN) return;

    try {
      // Subscribe to miniTicker stream (single)
      const miniTickerSubscribe = {
        method: 'subscribe',
        params: ['ETH_USDT@miniTicker'],
        id: generateCxpId(),
      };
      ws.send(JSON.stringify(miniTickerSubscribe));
      console.log('[MiniTickerScreen] Subscribed to ETH_USDT@miniTicker');
    } catch (error) {
      console.error('[MiniTickerScreen] Error subscribing:', error);
    }
  }, [ws]);

  // Subscribe to miniTicker updates
  useEffect(() => {
    try {
      TpSdk.miniTicker.subscribe((data) => {
        setMiniTicker(data);
      });

      return () => {
        try {
          TpSdk.miniTicker.unsubscribe();
        } catch {
          // Ignore cleanup errors
        }
      };
    } catch {
      // Ignore initialization errors
    }
  }, [setMiniTicker]);

  // Function to send fake miniTicker data
  const sendFakeData = useCallback(() => {
    try {
      TpSdk.miniTicker.sendFakeData({
        symbol: 'ETH_USDT',
        basePrice: 3000,
      });
    } catch (error) {
      console.error('Error sending fake miniTicker data:', error);
    }
  }, []);

  const formatPrice = (price: string) => {
    if (!price) return 'N/A';
    return parseFloat(price).toLocaleString('en-US', {
      minimumFractionDigits: 2,
      maximumFractionDigits: 8,
    });
  };

  const formatTimestamp = (timestamp: number) => {
    if (!timestamp) return 'N/A';
    return new Date(timestamp).toLocaleString();
  };

  return (
    <SafeAreaView style={styles.container}>
      <StatusBar barStyle="light-content" />

      <View style={styles.topHeader}>
        <View style={styles.symbolContainer}>
          <Text style={styles.symbolText}>ETH/USDT</Text>
          <Text style={styles.marketTypeText}>Mini Ticker</Text>
        </View>
        <TouchableOpacity style={styles.fakeButton} onPress={sendFakeData}>
          <Text style={styles.fakeButtonText}>Fake Data</Text>
        </TouchableOpacity>
      </View>

      <ScrollView
        style={styles.content}
        contentContainerStyle={styles.contentContainer}
      >
        {miniTicker ? (
          <View style={styles.tickerContainer}>
            <View style={styles.section}>
              <Text style={styles.sectionTitle}>Symbol</Text>
              <Text style={styles.sectionValue}>
                {miniTicker.symbol || 'N/A'}
              </Text>
            </View>

            <View style={styles.section}>
              <Text style={styles.sectionTitle}>Current Price</Text>
              <Text style={[styles.sectionValue, styles.priceValue]}>
                {formatPrice(miniTicker.currentPrice)}
              </Text>
            </View>

            <View style={styles.priceRow}>
              <View style={[styles.priceBox]}>
                <Text style={styles.priceLabel}>Open Price</Text>
                <Text style={[styles.priceValue]}>
                  {formatPrice(miniTicker.openPrice)}
                </Text>
              </View>

              <View style={[styles.priceBox]}>
                <Text style={styles.priceLabel}>High Price</Text>
                <Text style={[styles.priceValue, styles.highPrice]}>
                  {formatPrice(miniTicker.highPrice)}
                </Text>
              </View>

              <View style={[styles.priceBox]}>
                <Text style={styles.priceLabel}>Low Price</Text>
                <Text style={[styles.priceValue, styles.lowPrice]}>
                  {formatPrice(miniTicker.lowPrice)}
                </Text>
              </View>
            </View>

            <View style={styles.priceRow}>
              <View style={[styles.priceBox]}>
                <Text style={styles.priceLabel}>Price Change</Text>
                <Text
                  style={[
                    styles.priceValue,
                    parseFloat(miniTicker.priceChange || '0') >= 0
                      ? styles.positiveChange
                      : styles.negativeChange,
                  ]}
                >
                  {formatPrice(miniTicker.priceChange)}
                </Text>
              </View>

              <View style={[styles.priceBox]}>
                <Text style={styles.priceLabel}>Change %</Text>
                <Text
                  style={[
                    styles.priceValue,
                    parseFloat(miniTicker.priceChangePercent || '0') >= 0
                      ? styles.positiveChange
                      : styles.negativeChange,
                  ]}
                >
                  {formatPrice(miniTicker.priceChangePercent)}%
                </Text>
              </View>
            </View>

            <View style={styles.section}>
              <Text style={styles.sectionTitle}>Timestamp</Text>
              <Text style={styles.sectionValue}>
                {formatTimestamp(miniTicker.timestamp)}
              </Text>
            </View>

            <View style={styles.section}>
              <Text style={styles.sectionTitle}>Raw Data</Text>
              <View style={styles.rawDataContainer}>
                <Text style={styles.rawDataText}>
                  {JSON.stringify(miniTicker, null, 2)}
                </Text>
              </View>
            </View>
          </View>
        ) : (
          <View style={styles.emptyContainer}>
            <Text style={styles.emptyText}>No mini ticker data yet...</Text>
            <Text style={styles.emptySubtext}>
              Waiting for ETH_USDT@miniTicker stream
            </Text>
          </View>
        )}
      </ScrollView>
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
  content: {
    flex: 1,
  },
  contentContainer: {
    padding: 16,
  },
  tickerContainer: {
    backgroundColor: '#1a1a1b',
    borderRadius: 8,
    padding: 16,
    borderWidth: 1,
    borderColor: '#2a2e39',
  },
  section: {
    marginBottom: 20,
  },
  sectionTitle: {
    color: '#848e9c',
    fontSize: 12,
    fontWeight: '600',
    textTransform: 'uppercase',
    letterSpacing: 0.5,
    marginBottom: 8,
  },
  sectionValue: {
    color: '#ffffff',
    fontSize: 16,
    fontWeight: '600',
    fontFamily: 'monospace',
  },
  priceValue: {
    fontSize: 20,
    fontWeight: '700',
  },
  priceRow: {
    flexDirection: 'row',
    gap: 12,
    marginBottom: 20,
  },
  priceBox: {
    flex: 1,
    padding: 16,
    borderRadius: 8,
    borderWidth: 1,
    borderColor: '#2a2e39',
    backgroundColor: '#0f0f10',
  },
  priceLabel: {
    color: '#848e9c',
    fontSize: 11,
    fontWeight: '600',
    textTransform: 'uppercase',
    marginBottom: 8,
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
  rawDataContainer: {
    backgroundColor: '#0f0f10',
    padding: 12,
    borderRadius: 4,
    borderWidth: 1,
    borderColor: '#2a2e39',
  },
  rawDataText: {
    color: '#b7bdc6',
    fontSize: 11,
    fontFamily: 'monospace',
  },
  emptyContainer: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
    paddingVertical: 60,
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
