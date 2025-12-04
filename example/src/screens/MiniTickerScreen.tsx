import { useEffect, useRef, useState } from 'react';
import { StyleSheet, View, Text, ScrollView } from 'react-native';
import { SafeAreaView, StatusBar } from 'react-native';
import { TpSdk } from '@cxptek/tp-sdk';
import { useTradingStore } from '../stores/useTradingStore';
import { getTickerProperties } from '../types';

interface MiniTickerScreenProps {
  binanceBaseUrl: string;
  defaultSymbol: string;
}

export function MiniTickerScreen({
  binanceBaseUrl,
  defaultSymbol,
}: MiniTickerScreenProps) {
  const miniTicker = useTradingStore((state) => state.miniTicker);
  const setMiniTicker = useTradingStore((state) => state.setMiniTicker);
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
      `[MiniTickerScreen] Connecting to ${platform} WebSocket:`,
      binanceBaseUrl
    );

    const ws = new WebSocket(binanceBaseUrl);
    wsRef.current = ws;

    ws.onopen = () => {
      console.log(`[MiniTickerScreen] ${platform} WebSocket connected`);
      setIsConnected(true);

      // Send SUBSCRIBE message for both Binance and CXP
      // Binance: method="SUBSCRIBE", id=1 (number)
      // CXP: method="subscribe", id="g8fur6z" (string)
      const subscribeMessage = isCXP
        ? {
            method: 'subscribe',
            params: [`${defaultSymbol}@miniTicker`],
            id: `miniticker_${Date.now()}`,
          }
        : {
            method: 'SUBSCRIBE',
            params: [`${defaultSymbol}@miniTicker`],
            id: 1,
          };
      ws.send(JSON.stringify(subscribeMessage));
      console.log('[MiniTickerScreen] Sent SUBSCRIBE:', subscribeMessage);
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
        console.error('[MiniTickerScreen] Error processing message:', error);
      }
    };

    ws.onerror = (error) => {
      console.error('[MiniTickerScreen] WebSocket error:', error);
      setIsConnected(false);
    };

    ws.onclose = (event) => {
      console.log('[MiniTickerScreen] WebSocket closed, code:', event.code);
      setIsConnected(false);
    };

    return () => {
      console.log('[MiniTickerScreen] Cleaning up WebSocket');
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

  // Subscribe to miniTicker updates
  useEffect(() => {
    try {
      const subscriptionId = TpSdk.ticker.subscribe((data) => {
        setMiniTicker(data);
      });

      return () => {
        try {
          TpSdk.ticker.unsubscribe(subscriptionId);
        } catch {
          // Ignore cleanup errors
        }
      };
    } catch {
      // Ignore initialization errors
      return undefined;
    }
  }, [setMiniTicker]);

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

  const symbolDisplay = defaultSymbol.toUpperCase().replace('USDT', '/USDT');

  return (
    <SafeAreaView style={styles.container}>
      <StatusBar barStyle="light-content" />

      <View style={styles.topHeader}>
        <View style={styles.symbolContainer}>
          <Text style={styles.symbolText}>{symbolDisplay}</Text>
          <Text style={styles.marketTypeText}>Mini Ticker</Text>
          {isConnected ? (
            <View style={styles.connectedIndicator} />
          ) : (
            <View style={styles.disconnectedIndicator} />
          )}
        </View>
      </View>

      <ScrollView
        style={styles.content}
        contentContainerStyle={styles.contentContainer}
      >
        {miniTicker ? (
          (() => {
            const props = getTickerProperties(miniTicker);
            return (
              <View style={styles.tickerContainer}>
                <View style={styles.section}>
                  <Text style={styles.sectionTitle}>Symbol</Text>
                  <Text style={styles.sectionValue}>
                    {props.symbol || 'N/A'}
                  </Text>
                </View>

                <View style={styles.section}>
                  <Text style={styles.sectionTitle}>Current Price</Text>
                  <Text style={[styles.sectionValue, styles.priceValue]}>
                    {formatPrice(props.currentPrice)}
                  </Text>
                </View>

                <View style={styles.priceRow}>
                  <View style={[styles.priceBox]}>
                    <Text style={styles.priceLabel}>Open Price</Text>
                    <Text style={[styles.priceValue]}>
                      {formatPrice(props.openPrice)}
                    </Text>
                  </View>

                  <View style={[styles.priceBox]}>
                    <Text style={styles.priceLabel}>High Price</Text>
                    <Text style={[styles.priceValue, styles.highPrice]}>
                      {formatPrice(props.highPrice)}
                    </Text>
                  </View>

                  <View style={[styles.priceBox]}>
                    <Text style={styles.priceLabel}>Low Price</Text>
                    <Text style={[styles.priceValue, styles.lowPrice]}>
                      {formatPrice(props.lowPrice)}
                    </Text>
                  </View>
                </View>

                <View style={styles.priceRow}>
                  <View style={[styles.priceBox]}>
                    <Text style={styles.priceLabel}>Price Change</Text>
                    <Text
                      style={[
                        styles.priceValue,
                        parseFloat(props.priceChange || '0') >= 0
                          ? styles.positiveChange
                          : styles.negativeChange,
                      ]}
                    >
                      {formatPrice(props.priceChange)}
                    </Text>
                  </View>

                  <View style={[styles.priceBox]}>
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

                <View style={styles.section}>
                  <Text style={styles.sectionTitle}>Timestamp</Text>
                  <Text style={styles.sectionValue}>
                    {formatTimestamp(props.timestamp)}
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
            );
          })()
        ) : (
          <View style={styles.emptyContainer}>
            <Text style={styles.emptyText}>No mini ticker data yet...</Text>
            <Text style={styles.emptySubtext}>
              Waiting for {defaultSymbol}@miniTicker stream
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
});
