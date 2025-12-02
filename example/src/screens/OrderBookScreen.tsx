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
import type { OrderBookViewResult } from '@cxptek/tp-sdk';
import { OrderBookAnimated } from '../components/OrderBookAnimated';
import { useTradingStore } from '../stores/useTradingStore';
import { generateCxpId } from '../utils/cxpId';

interface OrderBookScreenProps {
  ws: WebSocket | null;
}

const AGGREGATION_OPTIONS = ['0.01', '0.1', '1', '10', '100'];
const DEFAULT_SYMBOL = 'ETH_USDT';
// ETH_USDT decimals: base (ETH) = 8, quote (USDT) = 2
const ETH_USDT_DECIMALS = { base: 8, quote: 2 };

export function OrderBookScreen({ ws }: OrderBookScreenProps) {
  const displayData = useTradingStore((state) => state.orderBook);
  const setOrderBook = useTradingStore((state) => state.setOrderBook);
  const [aggregation, setAggregation] = useState('0.01');
  const subscriptionIdRef = useRef<string | null>(null);
  const [useFakeStream, setUseFakeStream] = useState(true); // Enable fake stream by default
  const fakeStreamIntervalRef = useRef<ReturnType<typeof setInterval> | null>(
    null
  );
  const fakeStreamBasePriceRef = useRef<number>(3000);
  const fakeStreamPriceDirectionRef = useRef<number>(1); // 1 for up, -1 for down

  // Fake stream function to generate and send fake depth data
  const sendFakeDepthUpdate = useCallback(() => {
    const basePrice = fakeStreamBasePriceRef.current;
    const numLevels = 20;
    const priceStep = 0.5;

    // Generate fake bids and asks with slight price movement
    const bids: [string, string][] = [];
    const asks: [string, string][] = [];

    for (let i = 1; i <= numLevels; i++) {
      const bidPrice = (basePrice - i * priceStep).toFixed(2);
      const askPrice = (basePrice + i * priceStep).toFixed(2);
      // Random quantity with some variation
      const quantity = (Math.random() * 10 + 1).toFixed(8);

      bids.push([bidPrice, quantity]);
      asks.push([askPrice, quantity]);
    }

    // Create fake depth update message in CXP format
    const timestamp = Date.now();
    const fakeMessage = JSON.stringify({
      stream: `${DEFAULT_SYMBOL}@depth`,
      data: {
        e: 'depthUpdate',
        E: timestamp,
        s: DEFAULT_SYMBOL,
        U: '1',
        u: '2',
        b: bids,
        a: asks,
      },
    });

    // Process message through SDK - automatically triggers callbacks
    TpSdk.parseMessage(fakeMessage);

    // Update base price with slight movement (simulate market movement)
    const priceChange = (Math.random() - 0.5) * 2; // -1 to +1
    fakeStreamBasePriceRef.current += priceChange;

    // Reverse direction if price moves too far
    if (fakeStreamBasePriceRef.current > 3100) {
      fakeStreamPriceDirectionRef.current = -1;
    } else if (fakeStreamBasePriceRef.current < 2900) {
      fakeStreamPriceDirectionRef.current = 1;
    }
  }, []);

  // Fake stream interval
  useEffect(() => {
    if (!useFakeStream) {
      // Clear interval if fake stream is disabled
      if (fakeStreamIntervalRef.current) {
        clearInterval(fakeStreamIntervalRef.current);
        fakeStreamIntervalRef.current = null;
      }
      return;
    }

    // Start fake stream - send updates every 100ms (10 updates per second)
    fakeStreamIntervalRef.current = setInterval(() => {
      sendFakeDepthUpdate();
    }, 100);

    console.log('[OrderBookScreen] Fake depth stream started');

    return () => {
      if (fakeStreamIntervalRef.current) {
        clearInterval(fakeStreamIntervalRef.current);
        fakeStreamIntervalRef.current = null;
        console.log('[OrderBookScreen] Fake depth stream stopped');
      }
    };
  }, [useFakeStream, sendFakeDepthUpdate]);

  // Subscribe to WebSocket depth stream and handle messages (only if not using fake stream)
  useEffect(() => {
    if (useFakeStream || !ws) {
      return;
    }

    // Wait for WebSocket to be open before subscribing
    const subscribeToDepth = () => {
      if (ws.readyState !== WebSocket.OPEN) {
        return;
      }

      try {
        const subscribeMessage = {
          method: 'subscribe',
          params: [`${DEFAULT_SYMBOL}@depth`],
          id: generateCxpId(),
        };
        ws.send(JSON.stringify(subscribeMessage));
        console.log(`[OrderBookScreen] Subscribed to ${DEFAULT_SYMBOL}@depth`);
      } catch (error) {
        console.error(
          '[OrderBookScreen] Error subscribing to WebSocket:',
          error
        );
      }
    };

    // Subscribe when WebSocket opens
    const handleOpen = () => {
      subscribeToDepth();
    };

    // Handle WebSocket messages - process through SDK
    const handleMessage = (event: { data?: string }) => {
      if (!event.data) {
        return;
      }

      // Process message through SDK - automatically triggers callbacks
      TpSdk.parseMessage(event.data);
    };

    // Set up event handlers
    if (ws.readyState === WebSocket.OPEN) {
      subscribeToDepth();
    } else {
      ws.addEventListener('open', handleOpen);
    }

    ws.addEventListener('message', handleMessage);

    return () => {
      ws.removeEventListener('open', handleOpen);
      ws.removeEventListener('message', handleMessage);
    };
  }, [ws, useFakeStream]);

  // Initialize SDK if not already initialized
  useEffect(() => {
    const initSdk = async () => {
      try {
        // Check if SDK is already initialized
        if (TpSdk.isInitialized?.()) {
          return;
        }

        // Initialize SDK
        TpSdk.init((response) => {
          if (response === 'success') {
            console.log('[OrderBookScreen] SDK initialized');
          } else {
            console.error('[OrderBookScreen] SDK initialization failed');
          }
        });
      } catch (error) {
        console.error('[OrderBookScreen] Error initializing SDK:', error);
      }
    };

    initSdk();
  }, []);

  // Subscribe to orderbook updates from SDK
  useEffect(() => {
    // Wait for SDK to be initialized
    if (!TpSdk.isInitialized?.()) {
      console.log('[OrderBookScreen] Waiting for SDK initialization...');
      return;
    }

    let subscriptionId: string | null = null;

    try {
      const orderbookCallback = (data: OrderBookViewResult) => {
        console.log(
          '[OrderBookScreen] Callback received, bids=',
          data.bids.length,
          'asks=',
          data.asks.length
        );
        setOrderBook(data);
      };

      // Subscribe with aggregation and decimals options
      subscriptionId = TpSdk.orderbook.subscribe(orderbookCallback, {
        aggregation: aggregation,
        decimals: ETH_USDT_DECIMALS,
      });

      subscriptionIdRef.current = subscriptionId;

      console.log('[OrderBookScreen] Subscribed to orderbook updates');
    } catch (error) {
      console.error('[OrderBookScreen] Error subscribing to orderbook:', error);
    }

    return () => {
      if (subscriptionId) {
        try {
          TpSdk.orderbook.unsubscribe(subscriptionId);
          subscriptionIdRef.current = null;
          console.log('[OrderBookScreen] Unsubscribed from orderbook');
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

      try {
        // Update aggregation directly - C++ will trigger callback automatically
        TpSdk.orderbook.setAggregation(newAggregation);
        // Update state to reflect UI change
        setAggregation(newAggregation);
      } catch (error) {
        console.error('[OrderBookScreen] Error changing aggregation:', error);
      }
    },
    [aggregation]
  );

  // Send fake snapshot data for testing
  const handleSendFakeSnapshot = useCallback(() => {
    try {
      const basePrice = 3000;
      const numLevels = 20;
      const priceStep = 0.5;

      // Generate fake bids and asks
      const bids: [string, string][] = [];
      const asks: [string, string][] = [];

      for (let i = 1; i <= numLevels; i++) {
        const bidPrice = (basePrice - i * priceStep).toFixed(2);
        const askPrice = (basePrice + i * priceStep).toFixed(2);
        const quantity = (Math.random() * 10 + 1).toFixed(8);

        bids.push([bidPrice, quantity]);
        asks.push([askPrice, quantity]);
      }

      // Unsubscribe first
      if (subscriptionIdRef.current) {
        TpSdk.orderbook.unsubscribe(subscriptionIdRef.current);
      }

      // Re-subscribe with snapshot data
      const orderbookCallback = (data: OrderBookViewResult) => {
        console.log(
          '[OrderBookScreen] Callback received, bids=',
          data.bids.length,
          'asks=',
          data.asks.length
        );
        setOrderBook(data);
      };

      subscriptionIdRef.current = TpSdk.orderbook.subscribe(orderbookCallback, {
        aggregation: aggregation,
        decimals: ETH_USDT_DECIMALS,
        snapshot: { bids, asks },
      });

      console.log('[OrderBookScreen] Sent fake snapshot data');
    } catch (error) {
      console.error('[OrderBookScreen] Error sending fake snapshot:', error);
    }
  }, [aggregation, setOrderBook]);

  // Reset orderbook
  const handleReset = useCallback(() => {
    try {
      TpSdk.orderbook.reset();
      setOrderBook(null);
      // Reset fake stream base price
      fakeStreamBasePriceRef.current = 3000;
      console.log('[OrderBookScreen] Reset orderbook');
    } catch (error) {
      console.error('[OrderBookScreen] Error resetting orderbook:', error);
    }
  }, [setOrderBook]);

  // Toggle fake stream
  const handleToggleFakeStream = useCallback(() => {
    setUseFakeStream((prev) => !prev);
  }, []);

  return (
    <SafeAreaView style={styles.container}>
      <StatusBar barStyle="light-content" />

      {/* Header */}
      <View style={styles.header}>
        <View style={styles.symbolContainer}>
          <Text style={styles.symbolText}>
            {DEFAULT_SYMBOL.replace('_', '/')}
          </Text>
          <Text style={styles.marketTypeText}>Spot</Text>
        </View>
        <View style={styles.headerButtons}>
          <TouchableOpacity
            style={[styles.headerButton, styles.resetButton]}
            onPress={handleReset}
          >
            <Text style={styles.headerButtonText}>Reset</Text>
          </TouchableOpacity>
          <TouchableOpacity
            style={[
              styles.headerButton,
              useFakeStream
                ? styles.fakeStreamActiveButton
                : styles.fakeStreamButton,
            ]}
            onPress={handleToggleFakeStream}
          >
            <Text style={styles.headerButtonText}>
              {useFakeStream ? 'Fake Stream ON' : 'Fake Stream OFF'}
            </Text>
          </TouchableOpacity>
          <TouchableOpacity
            style={[styles.headerButton, styles.fakeButton]}
            onPress={handleSendFakeSnapshot}
          >
            <Text style={styles.headerButtonText}>Fake Snapshot</Text>
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
  fakeButton: {
    backgroundColor: '#f6465d',
  },
  fakeStreamButton: {
    backgroundColor: '#2a2e39',
  },
  fakeStreamActiveButton: {
    backgroundColor: '#0ecb81',
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
