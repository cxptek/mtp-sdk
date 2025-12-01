import React, { useCallback, useMemo, useEffect } from 'react';
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
import type { KlineMessageData } from '@cxptek/tp-sdk';
import { generateCxpId } from '../utils/cxpId';

interface KlineScreenProps {
  ws: WebSocket | null;
  processingStats: {
    totalProcessed: number;
    queueProcessed: number;
    lastMessageType: string;
    lastProcessTime: number;
  };
}

// Move format functions outside component to avoid recreation on each render
const formatPrice = (price: string): string => {
  if (!price) return 'N/A';
  const num = parseFloat(price);
  if (isNaN(num)) return 'N/A';
  return num.toLocaleString('en-US', {
    minimumFractionDigits: 2,
    maximumFractionDigits: 8,
  });
};

const formatVolume = (volume: string): string => {
  if (!volume) return 'N/A';
  const num = parseFloat(volume);
  if (isNaN(num)) return 'N/A';
  if (num >= 1000000) {
    return (num / 1000000).toFixed(2) + 'M';
  }
  if (num >= 1000) {
    return (num / 1000).toFixed(2) + 'K';
  }
  return num.toFixed(2);
};

const formatTimestamp = (timestamp: number): string => {
  if (!timestamp) return 'N/A';
  return new Date(timestamp).toLocaleString();
};

const getPriceChangeColor = (open: string, close: string): string => {
  if (!open || !close) return '#ffffff';
  const openPrice = parseFloat(open);
  const closePrice = parseFloat(close);
  if (isNaN(openPrice) || isNaN(closePrice)) return '#ffffff';
  if (closePrice > openPrice) return '#0ecb81';
  if (closePrice < openPrice) return '#f6465d';
  return '#ffffff';
};

// Memoized Kline item component to prevent unnecessary re-renders
const KlineItem = React.memo(({ item }: { item: KlineMessageData }) => {
  // Pre-compute values once per item
  const priceColor = useMemo(
    () => getPriceChangeColor(item.open, item.close),
    [item.open, item.close]
  );

  const formattedOpen = useMemo(() => formatPrice(item.open), [item.open]);
  const formattedHigh = useMemo(() => formatPrice(item.high), [item.high]);
  const formattedLow = useMemo(() => formatPrice(item.low), [item.low]);
  const formattedClose = useMemo(() => formatPrice(item.close), [item.close]);
  const formattedVolume = useMemo(
    () => formatVolume(item.volume),
    [item.volume]
  );
  const formattedQuoteVolume = useMemo(
    () => (item.quoteVolume ? formatVolume(item.quoteVolume) : null),
    [item.quoteVolume]
  );
  const formattedTimestamp = useMemo(
    () => formatTimestamp(item.timestamp),
    [item.timestamp]
  );
  const formattedOpenTime = useMemo(
    () => (item.openTime ? formatTimestamp(parseInt(item.openTime, 10)) : null),
    [item.openTime]
  );
  const formattedCloseTime = useMemo(
    () =>
      item.closeTime ? formatTimestamp(parseInt(item.closeTime, 10)) : null,
    [item.closeTime]
  );

  return (
    <View style={styles.klineItem}>
      <View style={styles.klineHeader}>
        <View>
          <Text style={styles.klineSymbol}>{item.symbol || 'N/A'}</Text>
          <Text style={styles.klineInterval}>
            {item.interval || 'N/A'} •{' '}
            {item.isClosed === 'true' ? 'Closed' : 'Open'}
          </Text>
        </View>
        <Text style={styles.klineTimestamp}>{formattedTimestamp}</Text>
      </View>

      <View style={styles.ohlcRow}>
        <View style={styles.ohlcColumn}>
          <Text style={styles.ohlcLabel}>Open</Text>
          <Text style={[styles.ohlcValue, { color: priceColor }]}>
            {formattedOpen}
          </Text>
        </View>

        <View style={styles.ohlcColumn}>
          <Text style={styles.ohlcLabel}>High</Text>
          <Text style={[styles.ohlcValue, styles.highPrice]}>
            {formattedHigh}
          </Text>
        </View>

        <View style={styles.ohlcColumn}>
          <Text style={styles.ohlcLabel}>Low</Text>
          <Text style={[styles.ohlcValue, styles.lowPrice]}>
            {formattedLow}
          </Text>
        </View>

        <View style={styles.ohlcColumn}>
          <Text style={styles.ohlcLabel}>Close</Text>
          <Text style={[styles.ohlcValue, { color: priceColor }]}>
            {formattedClose}
          </Text>
        </View>
      </View>

      <View style={styles.volumeRow}>
        <View style={styles.volumeColumn}>
          <Text style={styles.volumeLabel}>Volume</Text>
          <Text style={styles.volumeValue}>{formattedVolume}</Text>
        </View>

        {formattedQuoteVolume && (
          <View style={styles.volumeColumn}>
            <Text style={styles.volumeLabel}>Quote Volume</Text>
            <Text style={styles.volumeValue}>{formattedQuoteVolume}</Text>
          </View>
        )}

        {item.trades && (
          <View style={styles.volumeColumn}>
            <Text style={styles.volumeLabel}>Trades</Text>
            <Text style={styles.volumeValue}>{item.trades}</Text>
          </View>
        )}
      </View>

      {formattedOpenTime && formattedCloseTime && (
        <View style={styles.timeRow}>
          <Text style={styles.timeLabel}>Open: {formattedOpenTime}</Text>
          <Text style={styles.timeLabel}>Close: {formattedCloseTime}</Text>
        </View>
      )}
    </View>
  );
});

KlineItem.displayName = 'KlineItem';

export function KlineScreen({ ws }: KlineScreenProps) {
  const klines = useTradingStore((state) => state.klines);
  const addKline = useTradingStore((state) => state.addKline);

  // Subscribe to kline WebSocket stream
  useEffect(() => {
    if (!ws || ws.readyState !== WebSocket.OPEN) return;

    try {
      // Subscribe to kline stream
      const klineSubscribe = {
        method: 'subscribe',
        params: ['ETH_USDT@kline_1m'],
        id: generateCxpId(),
      };
      ws.send(JSON.stringify(klineSubscribe));
      console.log('[KlineScreen] Subscribed to ETH_USDT@kline_1m');
    } catch (error) {
      console.error('[KlineScreen] Error subscribing:', error);
    }
  }, [ws]);

  // Subscribe to kline updates
  useEffect(() => {
    try {
      TpSdk.kline.subscribe((data) => {
        addKline(data);
      });

      return () => {
        try {
          TpSdk.kline.unsubscribe();
        } catch {
          // Ignore cleanup errors
        }
      };
    } catch {
      // Ignore initialization errors
    }
  }, [addKline]);

  // Function to send fake kline data
  const sendFakeData = useCallback(() => {
    try {
      TpSdk.kline.sendFakeData({
        symbol: 'ETH_USDT',
        interval: '1m',
        basePrice: 3000,
      });
    } catch (error) {
      console.error('Error sending fake kline data:', error);
    }
  }, []);

  // Memoize render function to prevent recreation on each render
  const renderKline = useCallback(
    ({ item }: { item: KlineMessageData }) => <KlineItem item={item} />,
    []
  );

  // Memoize keyExtractor to prevent recreation
  const keyExtractor = useCallback(
    (item: KlineMessageData, index: number) =>
      `${item.symbol}-${item.timestamp}-${item.interval}-${index}`,
    []
  );

  return (
    <SafeAreaView style={styles.container}>
      <StatusBar barStyle="light-content" />

      <View style={styles.topHeader}>
        <View style={styles.symbolContainer}>
          <Text style={styles.symbolText}>ETH/USDT</Text>
          <Text style={styles.marketTypeText}>Kline (1m)</Text>
        </View>
        <TouchableOpacity style={styles.fakeButton} onPress={sendFakeData}>
          <Text style={styles.fakeButtonText}>Fake Data</Text>
        </TouchableOpacity>
      </View>

      <View style={styles.listHeader}>
        <Text style={styles.listHeaderText}>Candlesticks</Text>
      </View>

      <FlatList
        data={klines}
        renderItem={renderKline}
        keyExtractor={keyExtractor}
        contentContainerStyle={styles.listContent}
        removeClippedSubviews={true}
        maxToRenderPerBatch={10}
        updateCellsBatchingPeriod={50}
        windowSize={10}
        initialNumToRender={15}
        ListEmptyComponent={
          <View style={styles.emptyContainer}>
            <Text style={styles.emptyText}>No kline data yet...</Text>
            <Text style={styles.emptySubtext}>
              Waiting for ETH_USDT@kline_1m stream
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
  klineItem: {
    backgroundColor: '#1a1a1b',
    padding: 12,
    marginBottom: 8,
    borderRadius: 4,
    borderWidth: 1,
    borderColor: '#2a2e39',
  },
  klineHeader: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'flex-start',
    marginBottom: 12,
  },
  klineSymbol: {
    color: '#ffffff',
    fontSize: 14,
    fontWeight: '700',
    fontFamily: 'monospace',
    marginBottom: 4,
  },
  klineInterval: {
    color: '#848e9c',
    fontSize: 11,
    fontFamily: 'monospace',
  },
  klineTimestamp: {
    color: '#848e9c',
    fontSize: 10,
    fontFamily: 'monospace',
    textAlign: 'right',
  },
  ohlcRow: {
    flexDirection: 'row',
    gap: 8,
    marginBottom: 12,
  },
  ohlcColumn: {
    flex: 1,
  },
  ohlcLabel: {
    color: '#848e9c',
    fontSize: 10,
    fontWeight: '500',
    marginBottom: 4,
    textTransform: 'uppercase',
  },
  ohlcValue: {
    fontSize: 12,
    fontWeight: '600',
    fontFamily: 'monospace',
  },
  highPrice: {
    color: '#0ecb81',
  },
  lowPrice: {
    color: '#f6465d',
  },
  volumeRow: {
    flexDirection: 'row',
    gap: 8,
    marginTop: 8,
    paddingTop: 12,
    borderTopWidth: 1,
    borderTopColor: '#2a2e39',
  },
  volumeColumn: {
    flex: 1,
  },
  volumeLabel: {
    color: '#848e9c',
    fontSize: 10,
    fontWeight: '500',
    marginBottom: 4,
    textTransform: 'uppercase',
  },
  volumeValue: {
    color: '#b7bdc6',
    fontSize: 12,
    fontWeight: '600',
    fontFamily: 'monospace',
  },
  timeRow: {
    marginTop: 8,
    paddingTop: 8,
    borderTopWidth: 1,
    borderTopColor: '#2a2e39',
    flexDirection: 'row',
    justifyContent: 'space-between',
  },
  timeLabel: {
    color: '#5a5f6a',
    fontSize: 9,
    fontFamily: 'monospace',
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
