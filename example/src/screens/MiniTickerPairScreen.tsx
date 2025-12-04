import { useEffect, useRef, useState } from 'react';
import { StyleSheet, View, Text, FlatList } from 'react-native';
import { SafeAreaView, StatusBar } from 'react-native';
import { TpSdk } from '@cxptek/tp-sdk';
import { useTradingStore } from '../stores/useTradingStore';
import type { TickerMessageData } from '@cxptek/tp-sdk';
import { getTickerProperties } from '../types';

interface MiniTickerPairScreenProps {
  binanceBaseUrl: string;
  defaultSymbol: string;
}

export function MiniTickerPairScreen({
  binanceBaseUrl,
  defaultSymbol: _defaultSymbol,
}: MiniTickerPairScreenProps) {
  const miniTickerPair = useTradingStore((state) => state.miniTickerPair);
  const setMiniTickerPair = useTradingStore((state) => state.setMiniTickerPair);
  const wsRef = useRef<WebSocket | null>(null);
  const [isConnected, setIsConnected] = useState(false);

  // Auto-detect platform (Binance or CXP) - !miniTicker@arr is Binance-specific
  // For CXP, you might need a different stream name
  const platform = 'Binance'; // !miniTicker@arr is Binance format

  // WebSocket connection - supports both Binance and CXP
  useEffect(() => {
    console.log(
      `[MiniTickerPairScreen] Connecting to ${platform} WebSocket:`,
      binanceBaseUrl
    );

    const ws = new WebSocket(binanceBaseUrl);
    wsRef.current = ws;

    ws.onopen = () => {
      console.log(
        `[MiniTickerPairScreen] ${platform} WebSocket connected, sending SUBSCRIBE`
      );
      setIsConnected(true);

      // Send SUBSCRIBE message for all mini tickers
      // Note: !miniTicker@arr is Binance-specific format
      // For CXP, you may need to use a different stream name
      const subscribeMessage = {
        method: 'SUBSCRIBE',
        params: ['!miniTicker@arr'],
        id: 1,
      };
      ws.send(JSON.stringify(subscribeMessage));
      console.log('[MiniTickerPairScreen] Sent SUBSCRIBE:', subscribeMessage);
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
        console.error(
          '[MiniTickerPairScreen] Error processing message:',
          error
        );
      }
    };

    ws.onerror = (error) => {
      console.error('[MiniTickerPairScreen] WebSocket error:', error);
      setIsConnected(false);
    };

    ws.onclose = (event) => {
      console.log('[MiniTickerPairScreen] WebSocket closed, code:', event.code);
      setIsConnected(false);
    };

    return () => {
      console.log('[MiniTickerPairScreen] Cleaning up WebSocket');
      if (
        ws.readyState === WebSocket.OPEN ||
        ws.readyState === WebSocket.CONNECTING
      ) {
        ws.close();
      }
      wsRef.current = null;
      setIsConnected(false);
    };
  }, [binanceBaseUrl]);

  // Subscribe to miniTickerPair updates
  useEffect(() => {
    try {
      const subscriptionId = TpSdk.tickerAll.subscribe((data) => {
        setMiniTickerPair(data);
      });

      return () => {
        try {
          TpSdk.tickerAll.unsubscribe(subscriptionId);
        } catch {
          // Ignore cleanup errors
        }
      };
    } catch {
      // Ignore initialization errors
      return undefined;
    }
  }, [setMiniTickerPair]);

  const formatPrice = (price: string): string => {
    if (!price) return 'N/A';
    return parseFloat(price).toLocaleString('en-US', {
      minimumFractionDigits: 2,
      maximumFractionDigits: 8,
    });
  };

  const renderTicker = ({ item }: { item: TickerMessageData }) => {
    const props = getTickerProperties(item);
    return (
      <View style={styles.tickerItem}>
        <View style={styles.tickerHeader}>
          <Text style={styles.tickerSymbol}>{props.symbol || 'N/A'}</Text>
          <Text style={styles.tickerTimestamp}>
            {props.timestamp
              ? new Date(props.timestamp).toLocaleTimeString()
              : 'N/A'}
          </Text>
        </View>

        <View style={styles.priceRow}>
          <View style={styles.priceColumn}>
            <Text style={styles.priceLabel}>Price</Text>
            <Text style={[styles.priceValue, styles.currentPrice]}>
              {formatPrice(props.currentPrice)}
            </Text>
          </View>

          <View style={styles.priceColumn}>
            <Text style={styles.priceLabel}>High</Text>
            <Text style={[styles.priceValue, styles.highPrice]}>
              {formatPrice(props.highPrice)}
            </Text>
          </View>

          <View style={styles.priceColumn}>
            <Text style={styles.priceLabel}>Low</Text>
            <Text style={[styles.priceValue, styles.lowPrice]}>
              {formatPrice(props.lowPrice)}
            </Text>
          </View>

          <View style={styles.priceColumn}>
            <Text style={styles.priceLabel}>Change %</Text>
            <Text
              style={[
                styles.priceValue,
                parseFloat(props.priceChangePercent || '0') >= 0
                  ? styles.positiveChange
                  : styles.negativeChange,
              ]}
            >
              {formatPrice(props.priceChangePercent)}%
            </Text>
          </View>
        </View>
      </View>
    );
  };

  return (
    <SafeAreaView style={styles.container}>
      <StatusBar barStyle="light-content" />

      <View style={styles.topHeader}>
        <View style={styles.symbolContainer}>
          <Text style={styles.symbolText}>All Pairs</Text>
          <Text style={styles.marketTypeText}>Mini Ticker Pair</Text>
          {isConnected ? (
            <View style={styles.connectedIndicator} />
          ) : (
            <View style={styles.disconnectedIndicator} />
          )}
        </View>
      </View>

      <View style={styles.listHeader}>
        <Text style={styles.listHeaderText}>Ticker Pairs</Text>
      </View>

      <FlatList
        data={miniTickerPair}
        renderItem={renderTicker}
        keyExtractor={(item, index) => {
          const props = getTickerProperties(item);
          return `${props.symbol}-${props.timestamp}-${index}`;
        }}
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
});
