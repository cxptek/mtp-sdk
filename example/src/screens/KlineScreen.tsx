import React, {
  useCallback,
  useMemo,
  useEffect,
  useRef,
  useState,
} from 'react';
import { StyleSheet, View, Text, FlatList } from 'react-native';
import { SafeAreaView, StatusBar } from 'react-native';
import { TpSdk } from '@cxptek/tp-sdk';
import { useTradingStore } from '../stores/useTradingStore';
import type { KlineMessageData } from '@cxptek/tp-sdk';
import { getKlineProperties } from '../types';

interface KlineScreenProps {
  binanceBaseUrl: string;
  defaultSymbol: string;
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
  const props = useMemo(() => getKlineProperties(item), [item]);

  // Pre-compute values once per item
  const priceColor = useMemo(
    () => getPriceChangeColor(props.open, props.close),
    [props.open, props.close]
  );

  const formattedOpen = useMemo(() => formatPrice(props.open), [props.open]);
  const formattedHigh = useMemo(() => formatPrice(props.high), [props.high]);
  const formattedLow = useMemo(() => formatPrice(props.low), [props.low]);
  const formattedClose = useMemo(() => formatPrice(props.close), [props.close]);
  const formattedVolume = useMemo(
    () => formatVolume(props.volume),
    [props.volume]
  );
  const formattedQuoteVolume = useMemo(
    () => (props.quoteVolume ? formatVolume(props.quoteVolume) : null),
    [props.quoteVolume]
  );
  const formattedTimestamp = useMemo(
    () => formatTimestamp(props.timestamp),
    [props.timestamp]
  );
  const formattedOpenTime = useMemo(
    () => (props.openTime ? formatTimestamp(props.openTime) : null),
    [props.openTime]
  );
  const formattedCloseTime = useMemo(
    () => (props.closeTime ? formatTimestamp(props.closeTime) : null),
    [props.closeTime]
  );

  return (
    <View style={styles.klineItem}>
      <View style={styles.klineHeader}>
        <View>
          <Text style={styles.klineSymbol}>{props.symbol || 'N/A'}</Text>
          <Text style={styles.klineInterval}>
            {props.interval || 'N/A'} •{' '}
            {props.isClosed === 'true' ? 'Closed' : 'Open'}
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

        {props.trades && (
          <View style={styles.volumeColumn}>
            <Text style={styles.volumeLabel}>Trades</Text>
            <Text style={styles.volumeValue}>{props.trades}</Text>
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

export function KlineScreen({
  binanceBaseUrl,
  defaultSymbol,
}: KlineScreenProps) {
  const klines = useTradingStore((state) => state.klines);
  const addKline = useTradingStore((state) => state.addKline);
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
      `[KlineScreen] Connecting to ${platform} WebSocket:`,
      binanceBaseUrl
    );

    const ws = new WebSocket(binanceBaseUrl);
    wsRef.current = ws;

    ws.onopen = () => {
      console.log(`[KlineScreen] ${platform} WebSocket connected`);
      setIsConnected(true);

      // Send SUBSCRIBE message for both Binance and CXP
      // Binance: method="SUBSCRIBE", id=1 (number)
      // CXP: method="subscribe", id="g8fur6z" (string)
      const subscribeMessage = isCXP
        ? {
            method: 'subscribe',
            params: [`${defaultSymbol}@kline_1m`],
            id: `kline_${Date.now()}`,
          }
        : {
            method: 'SUBSCRIBE',
            params: [`${defaultSymbol}@kline_1m`],
            id: 1,
          };

      ws.send(JSON.stringify(subscribeMessage));
      console.log('[KlineScreen] Sent SUBSCRIBE:', subscribeMessage);
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
        console.error('[KlineScreen] Error processing message:', error);
      }
    };

    ws.onerror = (error) => {
      console.error('[KlineScreen] WebSocket error:', error);
      setIsConnected(false);
    };

    ws.onclose = (event) => {
      console.log('[KlineScreen] WebSocket closed, code:', event.code);
      setIsConnected(false);
    };

    return () => {
      console.log('[KlineScreen] Cleaning up WebSocket');
      if (
        ws.readyState === WebSocket.OPEN ||
        ws.readyState === WebSocket.CONNECTING
      ) {
        ws.close();
      }
      wsRef.current = null;
      setIsConnected(false);
    };
  }, [binanceBaseUrl, defaultSymbol, isCXP, platform]);

  // Subscribe to kline updates
  useEffect(() => {
    let subscriptionId: string | null = null;
    try {
      subscriptionId = TpSdk.kline.subscribe((data) => {
        addKline(data);
      });
    } catch {
      // Ignore initialization errors
    }

    return () => {
      if (subscriptionId) {
        try {
          TpSdk.kline.unsubscribe(subscriptionId);
        } catch {
          // Ignore cleanup errors
        }
      }
    };
  }, [addKline]);

  // Memoize render function to prevent recreation on each render
  const renderKline = useCallback(
    ({ item }: { item: KlineMessageData }) => <KlineItem item={item} />,
    []
  );

  // Memoize keyExtractor to prevent recreation
  const keyExtractor = useCallback((item: KlineMessageData, index: number) => {
    const props = getKlineProperties(item);
    return `${props.symbol}-${props.timestamp}-${props.interval}-${index}`;
  }, []);

  const symbolDisplay = defaultSymbol.toUpperCase().replace('USDT', '/USDT');

  return (
    <SafeAreaView style={styles.container}>
      <StatusBar barStyle="light-content" />

      <View style={styles.topHeader}>
        <View style={styles.symbolContainer}>
          <Text style={styles.symbolText}>{symbolDisplay}</Text>
          <Text style={styles.marketTypeText}>Kline (1m)</Text>
          {isConnected ? (
            <View style={styles.connectedIndicator} />
          ) : (
            <View style={styles.disconnectedIndicator} />
          )}
        </View>
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
              Waiting for {defaultSymbol}@kline_1m stream
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
});
