import { useEffect, useState, useCallback, useRef } from 'react';
import {
  SafeAreaView,
  StatusBar,
  StyleSheet,
  View,
  Text,
  TouchableOpacity,
} from 'react-native';
import { TpSdk } from '@cxptek/tp-sdk';
import type { OrderBookMessageData } from '@cxptek/tp-sdk';
import { OrderBookAnimated } from '../components/OrderBookAnimated';
import { useTradingStore } from '../stores/useTradingStore';
import type { OrderBookViewResult } from '../types';
import { runOnUISync, runOnJS } from 'react-native-worklets';

interface OrderBookScreenProps {
  binanceBaseUrl: string;
  defaultSymbol: string;
}

const AGGREGATION_OPTIONS = ['0.01', '0.1', '1', '10', '100'];

export function OrderBookScreen({
  binanceBaseUrl,
  defaultSymbol,
}: OrderBookScreenProps) {
  const displayData = useTradingStore((state) => state.orderBook);
  const setOrderBook = useTradingStore((state) => state.setOrderBook);
  const [aggregation, setAggregation] = useState('0.01');
  const subscriptionIdRef = useRef<string | null>(null);
  const wsRef = useRef<WebSocket | null>(null);
  const [isConnected, setIsConnected] = useState(false);

  // Auto-detect platform (Binance or CXP) from symbol format
  const isCXP =
    defaultSymbol.includes('_') &&
    defaultSymbol === defaultSymbol.toUpperCase();

  // WebSocket connection - supports both Binance and CXP
  // Binance: Uses Combined Streams API with SUBSCRIBE message
  // CXP: Direct stream connection (no SUBSCRIBE needed)
  useEffect(() => {
    const ws = new WebSocket(binanceBaseUrl);
    wsRef.current = ws;

    ws.onopen = () => {
      setIsConnected(true);

      // Send SUBSCRIBE message for both Binance and CXP
      // Binance: method="SUBSCRIBE", id=1 (number)
      // CXP: method="subscribe", id="g8fur6z" (string)
      const subscribeMessage = isCXP
        ? {
            method: 'subscribe',
            params: [`${defaultSymbol}@depth`],
            id: `orderbook_${Date.now()}`,
          }
        : {
            method: 'SUBSCRIBE',
            params: [`${defaultSymbol}@depth20@100ms`],
            id: 1,
          };

      console.log('subscribeMessage', subscribeMessage);

      try {
        ws.send(JSON.stringify(subscribeMessage));
      } catch (error) {
        console.error('[OrderBookScreen] Error sending SUBSCRIBE:', error);
      }
    };

    ws.onmessage = (event) => {
      if (!event.data) {
        return;
      }
      console.log('event', event.data);

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
        console.error('[OrderBookScreen] Error processing message:', error);
      }
    };

    ws.onerror = () => {
      setIsConnected(false);
    };

    ws.onclose = () => {
      setIsConnected(false);
    };

    return () => {
      if (
        ws.readyState === WebSocket.OPEN ||
        ws.readyState === WebSocket.CONNECTING
      ) {
        ws.close();
      }
      wsRef.current = null;
      setIsConnected(false);
    };
  }, [binanceBaseUrl, defaultSymbol, isCXP]);

  // Subscribe to orderbook updates from SDK
  useEffect(() => {
    // Wait for SDK to be initialized
    if (!TpSdk.isInitialized?.()) {
      return;
    }

    let subscriptionId: string | null = null;

    try {
      const orderbookCallback = (data: OrderBookMessageData) => {
        // Move heavy calculations (parseFloat, cumulative sums) to UI thread
        runOnUISync(() => {
          'worklet';
          // Calculate cumulative quantities for animation background
          let bidCumulative = 0;
          let askCumulative = 0;
          const maxBidCumulative: number[] = [];
          const maxAskCumulative: number[] = [];

          const bidsRaw = data.data.bids;
          const asksRaw = data.data.asks;

          // Process bids - calculate cumulative on UI thread
          const bids: Array<{
            priceStr: string;
            amountStr: string;
            cumulativeQuantity: string;
          }> = [];
          for (let i = 0; i < bidsRaw.length; i++) {
            const bid = bidsRaw[i];
            if (!bid) continue;
            const [price, quantity] = bid;
            const qty = parseFloat(quantity) || 0;
            bidCumulative += qty;
            maxBidCumulative.push(bidCumulative);
            bids.push({
              priceStr: price,
              amountStr: quantity,
              cumulativeQuantity: bidCumulative.toString(),
            });
          }

          // Process asks - calculate cumulative on UI thread
          const asks: Array<{
            priceStr: string;
            amountStr: string;
            cumulativeQuantity: string;
          }> = [];
          for (let i = 0; i < asksRaw.length; i++) {
            const ask = asksRaw[i];
            if (!ask) continue;
            const [price, quantity] = ask;
            const qty = parseFloat(quantity) || 0;
            askCumulative += qty;
            maxAskCumulative.push(askCumulative);
            asks.push({
              priceStr: price,
              amountStr: quantity,
              cumulativeQuantity: askCumulative.toString(),
            });
          }

          // Find max cumulative quantity for animation scaling
          let maxBid = 0;
          for (let i = 0; i < maxBidCumulative.length; i++) {
            const val = maxBidCumulative[i];
            if (val !== undefined && val > maxBid) {
              maxBid = val;
            }
          }

          let maxAsk = 0;
          for (let i = 0; i < maxAskCumulative.length; i++) {
            const val = maxAskCumulative[i];
            if (val !== undefined && val > maxAsk) {
              maxAsk = val;
            }
          }

          const maxCumulative = maxBid > maxAsk ? maxBid : maxAsk;

          // Build result object
          const converted: OrderBookViewResult = {
            bids,
            asks,
            symbol: data.data.symbol,
            maxCumulativeQuantity:
              maxCumulative > 0 ? maxCumulative.toString() : undefined,
          };

          // Call setOrderBook on JS thread
          runOnJS(setOrderBook)(converted);
        });
      };

      // Subscribe to orderbook updates
      subscriptionId = TpSdk.orderbook.subscribe(orderbookCallback);

      subscriptionIdRef.current = subscriptionId;
    } catch (error) {
      console.error('[OrderBookScreen] Error subscribing to orderbook:', error);
    }

    return () => {
      if (subscriptionId) {
        try {
          TpSdk.orderbook.unsubscribe(subscriptionId);
          subscriptionIdRef.current = null;
        } catch (error) {
          console.error('[OrderBookScreen] Error unsubscribing:', error);
        }
      }
    };
  }, [aggregation, setOrderBook]);

  // Handle aggregation change
  const handleAggregationChange = useCallback(
    (newAggregation: string) => {
      if (newAggregation === aggregation) {
        return;
      }
      // Update state - aggregation will be handled in UI if needed
      setAggregation(newAggregation);
    },
    [aggregation]
  );

  // Reset orderbook
  const handleReset = useCallback(() => {
    setOrderBook(null as any); // Allow null for reset
  }, [setOrderBook]);

  const symbolDisplay = defaultSymbol.toUpperCase().replace('USDT', '/USDT');

  return (
    <SafeAreaView style={styles.container}>
      <StatusBar barStyle="light-content" />

      {/* Header */}
      <View style={styles.header}>
        <View style={styles.symbolContainer}>
          <Text style={styles.symbolText}>{symbolDisplay}</Text>
          <Text style={styles.marketTypeText}>Spot</Text>
          {isConnected ? (
            <View style={styles.connectedIndicator} />
          ) : (
            <View style={styles.disconnectedIndicator} />
          )}
        </View>
        <View style={styles.headerButtons}>
          <TouchableOpacity
            style={[styles.headerButton, styles.resetButton]}
            onPress={handleReset}
          >
            <Text style={styles.headerButtonText}>Reset</Text>
          </TouchableOpacity>
        </View>
      </View>

      {/* Aggregation Selector */}
      <View style={styles.aggregationContainer}>
        <Text style={styles.aggregationLabel}>Aggregation:</Text>
        <View style={styles.aggregationButtons}>
          {AGGREGATION_OPTIONS.map((agg) => (
            <TouchableOpacity
              key={agg}
              style={[
                styles.aggregationButton,
                aggregation === agg && styles.aggregationButtonActive,
              ]}
              onPress={() => handleAggregationChange(agg)}
            >
              <Text
                style={[
                  styles.aggregationButtonText,
                  aggregation === agg && styles.aggregationButtonTextActive,
                ]}
              >
                {agg}
              </Text>
            </TouchableOpacity>
          ))}
        </View>
      </View>

      {/* Order Book Content */}
      <OrderBookAnimated data={displayData} />
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: '#0f0f10',
  },
  header: {
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
  headerButtons: {
    flexDirection: 'row',
    gap: 8,
  },
  headerButton: {
    paddingHorizontal: 12,
    paddingVertical: 6,
    borderRadius: 4,
  },
  resetButton: {
    backgroundColor: '#2a2e39',
  },
  headerButtonText: {
    color: '#ffffff',
    fontSize: 12,
    fontWeight: '600',
  },
  aggregationContainer: {
    paddingHorizontal: 16,
    paddingVertical: 12,
    backgroundColor: '#1a1a1b',
    borderBottomWidth: 1,
    borderBottomColor: '#2a2e39',
    flexDirection: 'row',
    alignItems: 'center',
    gap: 12,
  },
  aggregationLabel: {
    color: '#848e9c',
    fontSize: 12,
    fontWeight: '500',
  },
  aggregationButtons: {
    flexDirection: 'row',
    gap: 8,
    flex: 1,
  },
  aggregationButton: {
    paddingHorizontal: 12,
    paddingVertical: 6,
    backgroundColor: '#2a2e39',
    borderRadius: 4,
    minWidth: 50,
    alignItems: 'center',
  },
  aggregationButtonActive: {
    backgroundColor: '#0ecb81',
  },
  aggregationButtonText: {
    color: '#848e9c',
    fontSize: 12,
    fontWeight: '600',
    fontFamily: 'monospace',
  },
  aggregationButtonTextActive: {
    color: '#ffffff',
  },
});
