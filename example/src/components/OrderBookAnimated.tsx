import type { OrderBookViewResult } from '@cxptek/tp-sdk';
import React, { useMemo, useLayoutEffect } from 'react';
import { Dimensions, ScrollView, StyleSheet, Text, View } from 'react-native';
import {
  Canvas,
  Rect,
  LinearGradient,
  vec,
  Group,
} from '@shopify/react-native-skia';
import {
  useSharedValue,
  withTiming,
  useDerivedValue,
  Easing,
} from 'react-native-reanimated';
import type { SharedValue } from 'react-native-reanimated';
import { runOnUISync } from 'react-native-worklets';

const { width: SCREEN_WIDTH } = Dimensions.get('window');
const COLUMN_WIDTH = (SCREEN_WIDTH - 48) / 2;
const BAR_HEIGHT = 14;
const SPACING = 1;
const MAX_ROWS = 50; // Native returns up to 50 rows

// Colors
const BID_COLOR = '#0ecb81';
const ASK_COLOR = '#f6465d';
const BAR_OPACITY = 0.3; // Increased from 0.12 for better visibility
const BEST_ROW_BG = 'rgba(14, 203, 129, 0.08)';
const BEST_ASK_BG = 'rgba(246, 70, 93, 0.08)';
const TEXT_PRIMARY = '#ffffff';
const TEXT_SECONDARY = '#b7bdc6';
const TEXT_TERTIARY = '#848e9c';
const BG_DARK = '#0f0f10';
const BG_CARD = '#1a1a1b';
const BORDER_COLOR = '#2a2e39';

interface OrderBookProps {
  data: OrderBookViewResult | null;
}

// Optimized single bar component for Skia canvas
// Uses DerivedValue to compute animated x and width from SharedValue
const AnimatedBar: React.FC<{
  width: SharedValue<number>;
  y: number;
  isBid: boolean;
  index: number;
}> = ({ width, y, isBid }) => {
  // Compute x position: for bids, start from right; for asks, start from left
  const animatedX = useDerivedValue(() => {
    'worklet';
    const w = Math.max(0, width.value);
    // For bids: start from right edge, so x = COLUMN_WIDTH - width
    // For asks: start from left edge, so x = 0
    return isBid ? COLUMN_WIDTH - w : 0;
  });

  // Width animation - ensure it's always >= 0
  const animatedWidth = useDerivedValue(() => {
    'worklet';
    return Math.max(0, width.value);
  });

  // Gradient positions (fixed)
  const startX = isBid ? COLUMN_WIDTH : 0;
  const endX = isBid ? 0 : COLUMN_WIDTH;

  return (
    <Rect
      x={animatedX}
      y={y}
      width={animatedWidth}
      height={BAR_HEIGHT}
      opacity={BAR_OPACITY}
    >
      <LinearGradient
        start={vec(startX, y)}
        end={vec(endX, y + BAR_HEIGHT)}
        colors={
          isBid
            ? [
                `rgba(14, 203, 129, ${BAR_OPACITY})`,
                `rgba(14, 203, 129, ${BAR_OPACITY * 0.3})`,
              ]
            : [
                `rgba(246, 70, 93, ${BAR_OPACITY})`,
                `rgba(246, 70, 93, ${BAR_OPACITY * 0.3})`,
              ]
        }
      />
    </Rect>
  );
};

AnimatedBar.displayName = 'AnimatedBar';

// Optimized Canvas component - renders all bars efficiently in single Canvas
// Much better performance than individual components
interface BackgroundCanvasProps {
  widths: SharedValue<number>[];
  isBid: boolean;
}

// Memoize to prevent re-renders when widths array reference doesn't change
// Skia will still detect SharedValue changes internally
const BackgroundCanvas: React.FC<BackgroundCanvasProps> = React.memo(
  ({ widths, isBid }) => {
    return (
      <Canvas style={StyleSheet.absoluteFill} pointerEvents="none">
        <Group>
          {widths.map((width, index) => {
            const y = index * (BAR_HEIGHT + SPACING);
            return (
              <AnimatedBar
                key={`${isBid ? 'bid' : 'ask'}-bar-${index}`}
                width={width}
                y={y}
                isBid={isBid}
                index={index}
              />
            );
          })}
        </Group>
      </Canvas>
    );
  },
  // Only re-render if widths array reference changes
  (prevProps, nextProps) => {
    return (
      prevProps.widths === nextProps.widths &&
      prevProps.isBid === nextProps.isBid
    );
  }
);

BackgroundCanvas.displayName = 'BackgroundCanvas';

interface OrderBookRowProps {
  priceStr: string | null;
  amountStr: string | null;
  isBid: boolean;
  isBestPrice?: boolean;
}

const OrderBookRow: React.FC<OrderBookRowProps> = React.memo(
  ({ priceStr, amountStr, isBid, isBestPrice = false }) => {
    const displayPrice = priceStr || '--';
    const displayAmount = amountStr || '--';

    const rowStyle = React.useMemo(() => {
      if (!isBestPrice) return styles.row;
      return [styles.row, isBid ? styles.bestBidRow : styles.bestAskRow];
    }, [isBestPrice, isBid]);

    const priceTextStyle = React.useMemo(() => {
      const baseColor = isBid ? BID_COLOR : ASK_COLOR;
      const baseStyle = { ...styles.priceText, color: baseColor };
      return isBestPrice ? [baseStyle, styles.bestPriceText] : baseStyle;
    }, [isBid, isBestPrice]);

    return (
      <View style={rowStyle}>
        <View style={styles.barContainer}>
          <View style={styles.overlayContent}>
            <Text style={priceTextStyle}>{displayPrice}</Text>
            <Text style={[styles.amountText, { color: TEXT_SECONDARY }]}>
              {displayAmount}
            </Text>
          </View>
        </View>
      </View>
    );
  },
  // Custom comparison to prevent unnecessary re-renders
  (prevProps, nextProps) => {
    if (prevProps.priceStr !== nextProps.priceStr) return false;
    if (prevProps.amountStr !== nextProps.amountStr) return false;
    if (prevProps.isBid !== nextProps.isBid) return false;
    if (prevProps.isBestPrice !== nextProps.isBestPrice) return false;
    return true;
  }
);

OrderBookRow.displayName = 'OrderBookRow';

export const OrderBookAnimated: React.FC<OrderBookProps> = React.memo(
  ({ data }) => {
    // Native returns up to MAX_ROWS (50) rows
    // All heavy calculations moved to UI thread
    const bids = useMemo(() => data?.bids ?? [], [data?.bids]);
    const asks = useMemo(() => data?.asks ?? [], [data?.asks]);
    const maxCumulative = useMemo(
      () =>
        data?.maxCumulativeQuantity ? +data.maxCumulativeQuantity || 0 : 0,
      [data?.maxCumulativeQuantity]
    );

    // Get actual number of rows from native (up to MAX_ROWS)
    const actualRows = useMemo(
      () => Math.min(MAX_ROWS, Math.max(bids.length, asks.length)),
      [bids.length, asks.length]
    );

    // Pre-create all shared values - must be created at component level, not in loops
    // Create them individually to satisfy React hooks rules
    const bidWidth0 = useSharedValue(0);
    const bidWidth1 = useSharedValue(0);
    const bidWidth2 = useSharedValue(0);
    const bidWidth3 = useSharedValue(0);
    const bidWidth4 = useSharedValue(0);
    const bidWidth5 = useSharedValue(0);
    const bidWidth6 = useSharedValue(0);
    const bidWidth7 = useSharedValue(0);
    const bidWidth8 = useSharedValue(0);
    const bidWidth9 = useSharedValue(0);
    const bidWidth10 = useSharedValue(0);
    const bidWidth11 = useSharedValue(0);
    const bidWidth12 = useSharedValue(0);
    const bidWidth13 = useSharedValue(0);
    const bidWidth14 = useSharedValue(0);
    const bidWidth15 = useSharedValue(0);
    const bidWidth16 = useSharedValue(0);
    const bidWidth17 = useSharedValue(0);
    const bidWidth18 = useSharedValue(0);
    const bidWidth19 = useSharedValue(0);
    const bidWidth20 = useSharedValue(0);
    const bidWidth21 = useSharedValue(0);
    const bidWidth22 = useSharedValue(0);
    const bidWidth23 = useSharedValue(0);
    const bidWidth24 = useSharedValue(0);
    const bidWidth25 = useSharedValue(0);
    const bidWidth26 = useSharedValue(0);
    const bidWidth27 = useSharedValue(0);
    const bidWidth28 = useSharedValue(0);
    const bidWidth29 = useSharedValue(0);
    const bidWidth30 = useSharedValue(0);
    const bidWidth31 = useSharedValue(0);
    const bidWidth32 = useSharedValue(0);
    const bidWidth33 = useSharedValue(0);
    const bidWidth34 = useSharedValue(0);
    const bidWidth35 = useSharedValue(0);
    const bidWidth36 = useSharedValue(0);
    const bidWidth37 = useSharedValue(0);
    const bidWidth38 = useSharedValue(0);
    const bidWidth39 = useSharedValue(0);
    const bidWidth40 = useSharedValue(0);
    const bidWidth41 = useSharedValue(0);
    const bidWidth42 = useSharedValue(0);
    const bidWidth43 = useSharedValue(0);
    const bidWidth44 = useSharedValue(0);
    const bidWidth45 = useSharedValue(0);
    const bidWidth46 = useSharedValue(0);
    const bidWidth47 = useSharedValue(0);
    const bidWidth48 = useSharedValue(0);
    const bidWidth49 = useSharedValue(0);

    const askWidth0 = useSharedValue(0);
    const askWidth1 = useSharedValue(0);
    const askWidth2 = useSharedValue(0);
    const askWidth3 = useSharedValue(0);
    const askWidth4 = useSharedValue(0);
    const askWidth5 = useSharedValue(0);
    const askWidth6 = useSharedValue(0);
    const askWidth7 = useSharedValue(0);
    const askWidth8 = useSharedValue(0);
    const askWidth9 = useSharedValue(0);
    const askWidth10 = useSharedValue(0);
    const askWidth11 = useSharedValue(0);
    const askWidth12 = useSharedValue(0);
    const askWidth13 = useSharedValue(0);
    const askWidth14 = useSharedValue(0);
    const askWidth15 = useSharedValue(0);
    const askWidth16 = useSharedValue(0);
    const askWidth17 = useSharedValue(0);
    const askWidth18 = useSharedValue(0);
    const askWidth19 = useSharedValue(0);
    const askWidth20 = useSharedValue(0);
    const askWidth21 = useSharedValue(0);
    const askWidth22 = useSharedValue(0);
    const askWidth23 = useSharedValue(0);
    const askWidth24 = useSharedValue(0);
    const askWidth25 = useSharedValue(0);
    const askWidth26 = useSharedValue(0);
    const askWidth27 = useSharedValue(0);
    const askWidth28 = useSharedValue(0);
    const askWidth29 = useSharedValue(0);
    const askWidth30 = useSharedValue(0);
    const askWidth31 = useSharedValue(0);
    const askWidth32 = useSharedValue(0);
    const askWidth33 = useSharedValue(0);
    const askWidth34 = useSharedValue(0);
    const askWidth35 = useSharedValue(0);
    const askWidth36 = useSharedValue(0);
    const askWidth37 = useSharedValue(0);
    const askWidth38 = useSharedValue(0);
    const askWidth39 = useSharedValue(0);
    const askWidth40 = useSharedValue(0);
    const askWidth41 = useSharedValue(0);
    const askWidth42 = useSharedValue(0);
    const askWidth43 = useSharedValue(0);
    const askWidth44 = useSharedValue(0);
    const askWidth45 = useSharedValue(0);
    const askWidth46 = useSharedValue(0);
    const askWidth47 = useSharedValue(0);
    const askWidth48 = useSharedValue(0);
    const askWidth49 = useSharedValue(0);

    const bidWidths = useMemo(
      () => [
        bidWidth0,
        bidWidth1,
        bidWidth2,
        bidWidth3,
        bidWidth4,
        bidWidth5,
        bidWidth6,
        bidWidth7,
        bidWidth8,
        bidWidth9,
        bidWidth10,
        bidWidth11,
        bidWidth12,
        bidWidth13,
        bidWidth14,
        bidWidth15,
        bidWidth16,
        bidWidth17,
        bidWidth18,
        bidWidth19,
        bidWidth20,
        bidWidth21,
        bidWidth22,
        bidWidth23,
        bidWidth24,
        bidWidth25,
        bidWidth26,
        bidWidth27,
        bidWidth28,
        bidWidth29,
        bidWidth30,
        bidWidth31,
        bidWidth32,
        bidWidth33,
        bidWidth34,
        bidWidth35,
        bidWidth36,
        bidWidth37,
        bidWidth38,
        bidWidth39,
        bidWidth40,
        bidWidth41,
        bidWidth42,
        bidWidth43,
        bidWidth44,
        bidWidth45,
        bidWidth46,
        bidWidth47,
        bidWidth48,
        bidWidth49,
      ],
      [
        bidWidth0,
        bidWidth1,
        bidWidth2,
        bidWidth3,
        bidWidth4,
        bidWidth5,
        bidWidth6,
        bidWidth7,
        bidWidth8,
        bidWidth9,
        bidWidth10,
        bidWidth11,
        bidWidth12,
        bidWidth13,
        bidWidth14,
        bidWidth15,
        bidWidth16,
        bidWidth17,
        bidWidth18,
        bidWidth19,
        bidWidth20,
        bidWidth21,
        bidWidth22,
        bidWidth23,
        bidWidth24,
        bidWidth25,
        bidWidth26,
        bidWidth27,
        bidWidth28,
        bidWidth29,
        bidWidth30,
        bidWidth31,
        bidWidth32,
        bidWidth33,
        bidWidth34,
        bidWidth35,
        bidWidth36,
        bidWidth37,
        bidWidth38,
        bidWidth39,
        bidWidth40,
        bidWidth41,
        bidWidth42,
        bidWidth43,
        bidWidth44,
        bidWidth45,
        bidWidth46,
        bidWidth47,
        bidWidth48,
        bidWidth49,
      ]
    );

    const askWidths = useMemo(
      () => [
        askWidth0,
        askWidth1,
        askWidth2,
        askWidth3,
        askWidth4,
        askWidth5,
        askWidth6,
        askWidth7,
        askWidth8,
        askWidth9,
        askWidth10,
        askWidth11,
        askWidth12,
        askWidth13,
        askWidth14,
        askWidth15,
        askWidth16,
        askWidth17,
        askWidth18,
        askWidth19,
        askWidth20,
        askWidth21,
        askWidth22,
        askWidth23,
        askWidth24,
        askWidth25,
        askWidth26,
        askWidth27,
        askWidth28,
        askWidth29,
        askWidth30,
        askWidth31,
        askWidth32,
        askWidth33,
        askWidth34,
        askWidth35,
        askWidth36,
        askWidth37,
        askWidth38,
        askWidth39,
        askWidth40,
        askWidth41,
        askWidth42,
        askWidth43,
        askWidth44,
        askWidth45,
        askWidth46,
        askWidth47,
        askWidth48,
        askWidth49,
      ],
      [
        askWidth0,
        askWidth1,
        askWidth2,
        askWidth3,
        askWidth4,
        askWidth5,
        askWidth6,
        askWidth7,
        askWidth8,
        askWidth9,
        askWidth10,
        askWidth11,
        askWidth12,
        askWidth13,
        askWidth14,
        askWidth15,
        askWidth16,
        askWidth17,
        askWidth18,
        askWidth19,
        askWidth20,
        askWidth21,
        askWidth22,
        askWidth23,
        askWidth24,
        askWidth25,
        askWidth26,
        askWidth27,
        askWidth28,
        askWidth29,
        askWidth30,
        askWidth31,
        askWidth32,
        askWidth33,
        askWidth34,
        askWidth35,
        askWidth36,
        askWidth37,
        askWidth38,
        askWidth39,
        askWidth40,
        askWidth41,
        askWidth42,
        askWidth43,
        askWidth44,
        askWidth45,
        askWidth46,
        askWidth47,
        askWidth48,
        askWidth49,
      ]
    );

    // Update shared values when data changes - ALL calculations on UI thread
    useLayoutEffect(() => {
      if (!maxCumulative || maxCumulative <= 0 || !data) {
        return;
      }

      // Pass raw data to UI thread - extract and calculate everything there
      const bidsRaw = data.bids ?? [];
      const asksRaw = data.asks ?? [];

      // Move ALL calculations to UI thread using runOnUISync
      runOnUISync(() => {
        'worklet';
        const animationConfig = {
          duration: 300,
          easing: Easing.out(Easing.cubic),
        };

        // Extract cumulativeQuantity and calculate targets all on UI thread
        // Support all 50 rows with animation
        const maxAnimatedRows = Math.min(
          MAX_ROWS,
          bidsRaw.length,
          asksRaw.length
        );
        for (let i = 0; i < maxAnimatedRows; i++) {
          // Extract bid cumulativeQuantity and calculate target
          let bidCumQty = 0;
          if (i < bidsRaw.length) {
            const bid = bidsRaw[i];
            if (bid?.cumulativeQuantity) {
              const cumQtyStr = bid.cumulativeQuantity;
              bidCumQty = cumQtyStr ? +cumQtyStr || 0 : 0;
            }
          }
          const bidTarget =
            maxCumulative > 0 ? (bidCumQty / maxCumulative) * COLUMN_WIDTH : 0;

          // Extract ask cumulativeQuantity and calculate target
          let askCumQty = 0;
          if (i < asksRaw.length) {
            const ask = asksRaw[i];
            if (ask?.cumulativeQuantity) {
              const cumQtyStr = ask.cumulativeQuantity;
              askCumQty = cumQtyStr ? +cumQtyStr || 0 : 0;
            }
          }
          const askTarget =
            maxCumulative > 0 ? (askCumQty / maxCumulative) * COLUMN_WIDTH : 0;

          // Only animate if value changed significantly
          const currentBidWidth = bidWidths[i]!.value;
          const currentAskWidth = askWidths[i]!.value;

          if (Math.abs(currentBidWidth - bidTarget) > 0.1) {
            bidWidths[i]!.value = withTiming(bidTarget, animationConfig);
          }
          if (Math.abs(currentAskWidth - askTarget) > 0.1) {
            askWidths[i]!.value = withTiming(askTarget, animationConfig);
          }
        }
      });
      // bidWidths and askWidths are stable arrays created at component level,
      // they don't need to be in dependencies
      // eslint-disable-next-line react-hooks/exhaustive-deps
    }, [data, maxCumulative]);

    1; // Prepare rows data - memoize to prevent recreation on every render
    const bidData = useMemo(() => {
      const rows: Array<{
        priceStr: string | null;
        amountStr: string | null;
        key: string;
        isBestPrice: boolean;
      }> = [];
      for (let i = 0; i < actualRows; i++) {
        const bid = bids[i];
        rows.push({
          priceStr: bid?.priceStr ?? null,
          amountStr: bid?.amountStr ?? null,
          key: `bid-${i}`,
          isBestPrice: i === 0 && bid?.priceStr !== null,
        });
      }
      return rows;
    }, [bids, actualRows]);

    const askData = useMemo(() => {
      const rows: Array<{
        priceStr: string | null;
        amountStr: string | null;
        key: string;
        isBestPrice: boolean;
      }> = [];
      for (let i = 0; i < actualRows; i++) {
        const ask = asks[i];
        rows.push({
          priceStr: ask?.priceStr ?? null,
          amountStr: ask?.amountStr ?? null,
          key: `ask-${i}`,
          isBestPrice: i === 0 && ask?.priceStr !== null,
        });
      }
      return rows;
    }, [asks, actualRows]);

    // Container height - calculate based on actual rows
    const containerHeight = actualRows * (BAR_HEIGHT + SPACING);

    if (!data) {
      return (
        <View style={styles.container}>
          <View style={styles.header}>
            <View style={styles.headerColumn}>
              <Text style={styles.headerLabel}>Price (USDT)</Text>
            </View>
            <View style={styles.headerColumn}>
              <Text style={styles.headerLabel}>Amount</Text>
            </View>
          </View>
          <View style={styles.orderbookContainer}>
            <Text style={styles.loadingText}>
              Waiting for order book data...
            </Text>
          </View>
        </View>
      );
    }

    // Check if we have valid data to display
    const hasValidData =
      (data.bids && data.bids.length > 0) ||
      (data.asks && data.asks.length > 0);

    if (!hasValidData) {
      return (
        <View style={styles.container}>
          <View style={styles.header}>
            <View style={styles.headerColumn}>
              <Text style={styles.headerLabel}>Price (USDT)</Text>
            </View>
            <View style={styles.headerColumn}>
              <Text style={styles.headerLabel}>Amount</Text>
            </View>
          </View>
          <View style={styles.orderbookContainer}>
            <Text style={styles.loadingText}>No order book data available</Text>
          </View>
        </View>
      );
    }

    return (
      <View style={styles.container}>
        <View style={styles.header}>
          <View style={styles.headerColumn}>
            <Text style={styles.headerLabel}>Price (USDT)</Text>
          </View>
          <View style={styles.headerColumn}>
            <Text style={styles.headerLabel}>Amount</Text>
          </View>
        </View>

        <ScrollView
          style={styles.orderbookContainer}
          contentContainerStyle={{ minHeight: containerHeight }}
          showsVerticalScrollIndicator={false}
          removeClippedSubviews={true}
        >
          <View style={styles.sideBySideContainer}>
            {/* Bids Container with Skia Background */}
            <View style={[styles.bidsContainer, { height: containerHeight }]}>
              <BackgroundCanvas widths={bidWidths} isBid={true} />
              {bidData.map((row) => (
                <OrderBookRow
                  key={row.key}
                  priceStr={row.priceStr}
                  amountStr={row.amountStr}
                  isBid={true}
                  isBestPrice={row.isBestPrice}
                />
              ))}
            </View>

            {/* Asks Container with Skia Background */}
            <View style={[styles.asksContainer, { height: containerHeight }]}>
              <BackgroundCanvas widths={askWidths} isBid={false} />
              {askData.map((row) => (
                <OrderBookRow
                  key={row.key}
                  priceStr={row.priceStr}
                  amountStr={row.amountStr}
                  isBid={false}
                  isBestPrice={row.isBestPrice}
                />
              ))}
            </View>
          </View>
        </ScrollView>
      </View>
    );
  }
);

const styles = StyleSheet.create({
  container: {
    flex: 1,
    backgroundColor: BG_DARK,
    padding: 12,
  },
  loadingText: {
    color: TEXT_SECONDARY,
    textAlign: 'center',
    marginTop: 20,
    fontSize: 14,
  },
  header: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    paddingVertical: 8,
    paddingHorizontal: 8,
    borderBottomWidth: 1,
    borderBottomColor: BORDER_COLOR,
    marginBottom: 4,
    backgroundColor: BG_CARD,
    borderRadius: 4,
  },
  headerColumn: {
    flex: 1,
  },
  headerLabel: {
    color: TEXT_TERTIARY,
    fontSize: 10,
    fontWeight: '500',
    textTransform: 'uppercase',
    letterSpacing: 0.5,
  },
  orderbookContainer: {
    flex: 1,
  },
  sideBySideContainer: {
    flexDirection: 'row',
    flex: 1,
  },
  asksContainer: {
    flex: 1,
    marginLeft: 4,
    position: 'relative',
    backgroundColor: BG_CARD,
    borderRadius: 4,
    paddingVertical: 2,
  },
  bidsContainer: {
    flex: 1,
    marginRight: 4,
    position: 'relative',
    backgroundColor: BG_CARD,
    borderRadius: 4,
    paddingVertical: 2,
  },
  centerPrice: {
    paddingVertical: 16,
    paddingHorizontal: 12,
    alignItems: 'center',
    borderTopWidth: 1,
    borderBottomWidth: 1,
    borderColor: BORDER_COLOR,
    backgroundColor: BG_CARD,
    marginTop: 4,
    borderRadius: 4,
  },
  centerPriceLabel: {
    color: TEXT_TERTIARY,
    fontSize: 10,
    fontWeight: '500',
    textTransform: 'uppercase',
    letterSpacing: 0.5,
    marginBottom: 4,
  },
  centerPriceText: {
    color: TEXT_PRIMARY,
    fontSize: 28,
    fontWeight: '700',
    fontFamily: 'monospace',
    fontVariant: ['tabular-nums'],
    marginBottom: 6,
  },
  spreadContainer: {
    flexDirection: 'row',
    alignItems: 'center',
    gap: 4,
  },
  spreadLabel: {
    color: TEXT_TERTIARY,
    fontSize: 11,
    fontFamily: 'monospace',
  },
  spreadValue: {
    color: TEXT_SECONDARY,
    fontSize: 11,
    fontWeight: '600',
    fontFamily: 'monospace',
    fontVariant: ['tabular-nums'],
  },
  spreadPercent: {
    color: TEXT_TERTIARY,
    fontSize: 11,
    fontFamily: 'monospace',
    fontVariant: ['tabular-nums'],
  },
  row: {
    flexDirection: 'row',
    height: BAR_HEIGHT + SPACING,
    marginVertical: 0.5,
    paddingHorizontal: 6,
    borderRadius: 2,
  },
  bestBidRow: {
    backgroundColor: BEST_ROW_BG,
    borderLeftWidth: 2,
    borderLeftColor: BID_COLOR,
  },
  bestAskRow: {
    backgroundColor: BEST_ASK_BG,
    borderRightWidth: 2,
    borderRightColor: ASK_COLOR,
  },
  priceText: {
    fontSize: 11,
    fontWeight: '500',
    fontFamily: 'monospace',
    fontVariant: ['tabular-nums'],
    minWidth: 70,
  },
  bestPriceText: {
    fontWeight: '700',
    fontSize: 11.5,
  },
  amountText: {
    fontSize: 10,
    fontFamily: 'monospace',
    fontVariant: ['tabular-nums'],
    textAlign: 'right',
    flex: 1,
  },
  barContainer: {
    flex: 1,
    height: BAR_HEIGHT,
    position: 'relative',
    backgroundColor: 'transparent',
    borderRadius: 2,
    overflow: 'hidden',
  },
  overlayContent: {
    position: 'absolute',
    left: 0,
    right: 0,
    top: 0,
    bottom: 0,
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'space-between',
    zIndex: 1,
  },
});
