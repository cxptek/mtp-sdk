import type { OrderBookViewResult } from '../types';
import React, { useMemo } from 'react';
import { Dimensions, StyleSheet, Text, View } from 'react-native';

const { width: SCREEN_WIDTH } = Dimensions.get('window');
const COLUMN_WIDTH = (SCREEN_WIDTH - 48) / 2; // Each column width (with padding and margin)
const BAR_HEIGHT = 20;
const SPACING = 2;
const MAX_LEVELS = 30;
const VISIBLE_ITEMS = 25; // Render 25 visible items per side (reduced for better performance)

// Professional trading app palette (Binance/Coinbase style)
const BID_COLOR = '#0ecb81'; // Green for bids
const ASK_COLOR = '#f6465d'; // Red for asks
const BAR_OPACITY = 0.12; // More visible depth bars
const BEST_ROW_BG = 'rgba(14, 203, 129, 0.08)'; // Subtle highlight for best bid
const BEST_ASK_BG = 'rgba(246, 70, 93, 0.08)'; // Subtle highlight for best ask
const TEXT_PRIMARY = '#ffffff';
const TEXT_SECONDARY = '#b7bdc6';
const TEXT_TERTIARY = '#848e9c';
const BG_DARK = '#0f0f10';
const BG_CARD = '#1a1a1b';
const BORDER_COLOR = '#2a2e39';

interface OrderBookProps {
  data: OrderBookViewResult | null;
  maxLevels?: number;
}

interface OrderBookRowProps {
  priceStr: string | null;
  amountStr: string | null;
  barWidth: number;
  isBid: boolean;
  isBestPrice?: boolean; // Highlight best bid/ask
}

// Single row component for both bids and asks
// ✅ TRADING APP STYLE: Professional layout with highlight for best prices
const OrderBookRow: React.FC<OrderBookRowProps> = React.memo(
  ({ priceStr, amountStr, barWidth, isBid, isBestPrice = false }) => {
    // ✅ OPTIMIZATION: Use constants instead of string concatenation
    const displayPrice = priceStr || '--';
    const displayAmount = amountStr || '--';

    const rowStyle = React.useMemo(() => {
      if (!isBestPrice) return styles.row;
      return [styles.row, isBid ? styles.bestBidRow : styles.bestAskRow];
    }, [isBestPrice, isBid]);

    // ✅ OPTIMIZATION: Pre-compute text style with memoization
    const priceTextStyle = React.useMemo(() => {
      const baseColor = isBid ? BID_COLOR : ASK_COLOR;
      const baseStyle = { ...styles.priceText, color: baseColor };
      return isBestPrice ? [baseStyle, styles.bestPriceText] : baseStyle;
    }, [isBid, isBestPrice]);

    const amountTextStyle = React.useMemo(
      () => ({ ...styles.amountText, color: TEXT_SECONDARY }),
      []
    );

    // ✅ OPTIMIZATION: Memoize depth bar style
    const barStyle = React.useMemo(() => {
      const clampedWidth = Math.max(0, Math.min(COLUMN_WIDTH, barWidth));
      return {
        ...styles.barFill,
        width: clampedWidth,
        backgroundColor: isBid ? BID_COLOR : ASK_COLOR,
        opacity: BAR_OPACITY,
        ...(isBid ? { right: 0 } : { left: 0 }),
      };
    }, [barWidth, isBid]);

    return (
      <View style={rowStyle}>
        <View style={styles.barContainer}>
          {/* ✅ OPTIMIZATION: Depth bar background */}
          {barWidth > 0 && <View style={barStyle} />}
          <View style={styles.overlayContent}>
            {/* ✅ TRADING APP STYLE: Price on left, Amount on right (standard layout) */}
            <Text style={priceTextStyle}>{displayPrice}</Text>
            <Text style={amountTextStyle}>{displayAmount}</Text>
          </View>
        </View>
      </View>
    );
  },
  // ✅ OPTIMIZATION: Improved comparison with early returns
  (prevProps, nextProps) => {
    if (prevProps.priceStr !== nextProps.priceStr) return false;
    if (prevProps.amountStr !== nextProps.amountStr) return false;
    if (Math.abs(prevProps.barWidth - nextProps.barWidth) >= 0.1) return false;
    if (prevProps.isBid !== nextProps.isBid) return false;
    if (prevProps.isBestPrice !== nextProps.isBestPrice) return false;
    return true;
  }
);

OrderBookRow.displayName = 'OrderBookRow';

// ✅ FRAME FIX: Memoize OrderBook component to prevent unnecessary re-renders
export const OrderBook: React.FC<OrderBookProps> = React.memo(
  ({ data, maxLevels = MAX_LEVELS }) => {
    // Parse maxCumulativeQuantity for percentage calculation
    const maxCumulative = useMemo(() => {
      if (!data) return 0;
      const maxStr = data.maxCumulativeQuantity || '0';
      return parseFloat(maxStr) || 0;
    }, [data]);

    // Memoize display arrays - limit to visible items for performance
    const baseBids = useMemo(() => {
      if (!data) return [];
      const bids = data.bids ?? [];
      if (!bids.length) return [];
      return bids.slice(0, Math.min(maxLevels, VISIBLE_ITEMS));
    }, [data, maxLevels]);

    const baseAsks = useMemo(() => {
      if (!data) return [];
      const asks = data.asks ?? [];
      if (!asks.length) return [];
      return asks.slice(0, Math.min(maxLevels, VISIBLE_ITEMS));
    }, [data, maxLevels]);

    // Pre-compute background widths (optimized: cache parseFloat results)
    const bidWidths = useMemo(() => {
      if (!maxCumulative) return new Array(VISIBLE_ITEMS).fill(0);
      const widths = new Array(VISIBLE_ITEMS);
      for (let i = 0; i < baseBids.length && i < VISIBLE_ITEMS; i++) {
        const b = baseBids[i];
        if (!b || !b.cumulativeQuantity || b.cumulativeQuantity === '--') {
          widths[i] = 0;
        } else {
          // Direct number conversion is faster than parseFloat for simple strings
          const cumQty = +b.cumulativeQuantity || 0;
          widths[i] = (cumQty / maxCumulative) * COLUMN_WIDTH;
        }
      }
      // Fill remaining with 0
      for (let i = baseBids.length; i < VISIBLE_ITEMS; i++) {
        widths[i] = 0;
      }
      return widths;
    }, [baseBids, maxCumulative]);

    const askWidths = useMemo(() => {
      if (!maxCumulative) return new Array(VISIBLE_ITEMS).fill(0);
      const widths = new Array(VISIBLE_ITEMS);
      for (let i = 0; i < baseAsks.length && i < VISIBLE_ITEMS; i++) {
        const a = baseAsks[i];
        if (!a || !a.cumulativeQuantity || a.cumulativeQuantity === '--') {
          widths[i] = 0;
        } else {
          // Direct number conversion is faster than parseFloat for simple strings
          const cumQty = +a.cumulativeQuantity || 0;
          widths[i] = (cumQty / maxCumulative) * COLUMN_WIDTH;
        }
      }
      // Fill remaining with 0
      for (let i = baseAsks.length; i < VISIBLE_ITEMS; i++) {
        widths[i] = 0;
      }
      return widths;
    }, [baseAsks, maxCumulative]);

    // ✅ OPTIMIZATION: Merge display arrays with widths and keys in single pass
    interface DisplayRow {
      priceStr: string | null;
      amountStr: string | null;
      barWidth: number;
      key: string;
      isBestPrice: boolean;
    }

    const bidRows = useMemo((): DisplayRow[] => {
      const rows: DisplayRow[] = new Array(VISIBLE_ITEMS);
      const len = Math.min(baseBids.length, VISIBLE_ITEMS);

      for (let i = 0; i < len; i++) {
        const bid = baseBids[i];
        rows[i] = {
          priceStr: bid?.priceStr ?? null,
          amountStr: bid?.amountStr ?? null,
          barWidth: bidWidths[i] || 0,
          key: `bid-${i}-${bid?.priceStr ?? 'null'}`,
          isBestPrice: i === 0 && bid?.priceStr !== null,
        };
      }

      // Fill remaining with empty rows
      for (let i = len; i < VISIBLE_ITEMS; i++) {
        rows[i] = {
          priceStr: null,
          amountStr: null,
          barWidth: 0,
          key: `bid-${i}-null`,
          isBestPrice: false,
        };
      }

      return rows;
    }, [baseBids, bidWidths]);

    const askRows = useMemo((): DisplayRow[] => {
      const rows: DisplayRow[] = new Array(VISIBLE_ITEMS);
      const len = Math.min(baseAsks.length, VISIBLE_ITEMS);

      for (let i = 0; i < len; i++) {
        const ask = baseAsks[i];
        rows[i] = {
          priceStr: ask?.priceStr ?? null,
          amountStr: ask?.amountStr ?? null,
          barWidth: askWidths[i] || 0,
          key: `ask-${i}-${ask?.priceStr ?? 'null'}`,
          isBestPrice: i === 0 && ask?.priceStr !== null,
        };
      }

      // Fill remaining with empty rows
      for (let i = len; i < VISIBLE_ITEMS; i++) {
        rows[i] = {
          priceStr: null,
          amountStr: null,
          barWidth: 0,
          key: `ask-${i}-null`,
          isBestPrice: false,
        };
      }

      return rows;
    }, [baseAsks, askWidths]);

    // ✅ OPTIMIZATION: Memoize formatted strings only (no unused variables)
    const { midPriceFormatted, spreadFormatted, spreadPercentFormatted } =
      useMemo(() => {
        const bidStr = bidRows[0]?.priceStr || '--';
        const askStr = askRows[0]?.priceStr || '--';
        // Use unary + operator instead of parseFloat for better performance
        const bid = bidStr !== '--' ? +bidStr.replace(/,/g, '') || 0 : 0;
        const ask = askStr !== '--' ? +askStr.replace(/,/g, '') || 0 : 0;
        const mid = bid > 0 && ask > 0 ? (bid + ask) / 2 : 0;
        const spreadValue = ask > 0 && bid > 0 ? ask - bid : 0;
        const spreadPct = mid > 0 ? (spreadValue / mid) * 100 : 0;
        return {
          midPriceFormatted: mid > 0 ? mid.toFixed(2) : '--',
          spreadFormatted: spreadValue > 0 ? spreadValue.toFixed(2) : '--',
          spreadPercentFormatted: spreadPct > 0 ? spreadPct.toFixed(4) : '',
        };
      }, [bidRows, askRows]);

    // Early return check
    if (!data) {
      return (
        <View style={styles.container}>
          <Text style={styles.loadingText}>Loading orderbook...</Text>
        </View>
      );
    }

    return (
      <View style={styles.container}>
        {/* ✅ TRADING APP STYLE: Professional header with Price | Amount columns */}
        <View style={styles.header}>
          <View style={styles.headerColumn}>
            <Text style={styles.headerLabel}>Price (USDT)</Text>
          </View>
          <View style={styles.headerColumn}>
            <Text style={styles.headerLabel}>Amount</Text>
          </View>
        </View>

        <View style={styles.orderbookContainer}>
          <View style={styles.sideBySideContainer}>
            {/* ✅ OPTIMIZATION: Bids (left) - Pre-computed rows with keys */}
            <View style={styles.bidsContainer}>
              {bidRows.map((row) => (
                <OrderBookRow
                  key={row.key}
                  priceStr={row.priceStr}
                  amountStr={row.amountStr}
                  barWidth={row.barWidth}
                  isBid={true}
                  isBestPrice={row.isBestPrice}
                />
              ))}
            </View>

            {/* ✅ OPTIMIZATION: Asks (right) - Pre-computed rows with keys */}
            <View style={styles.asksContainer}>
              {askRows.map((row) => (
                <OrderBookRow
                  key={row.key}
                  priceStr={row.priceStr}
                  amountStr={row.amountStr}
                  barWidth={row.barWidth}
                  isBid={false}
                  isBestPrice={row.isBestPrice}
                />
              ))}
            </View>
          </View>

          {/* ✅ OPTIMIZATION: Professional center price display with cached formatted strings */}
          <View style={styles.centerPrice}>
            <Text style={styles.centerPriceLabel}>Last Price</Text>
            <Text style={styles.centerPriceText}>{midPriceFormatted}</Text>
            <View style={styles.spreadContainer}>
              <Text style={styles.spreadLabel}>Spread:</Text>
              <Text style={styles.spreadValue}>{spreadFormatted}</Text>
              {spreadPercentFormatted && (
                <Text style={styles.spreadPercent}>
                  ({spreadPercentFormatted}%)
                </Text>
              )}
            </View>
          </View>
        </View>
      </View>
    );
  },
  // ✅ OPTIMIZATION: Improved comparison with early returns and more checks
  (prevProps, nextProps) => {
    // Early return if maxLevels changed
    if (prevProps.maxLevels !== nextProps.maxLevels) return false;

    const prevData = prevProps.data;
    const nextData = nextProps.data;

    // Handle null cases
    if (!prevData && !nextData) return true;
    if (!prevData || !nextData) return false;

    // Compare maxCumulativeQuantity first (cheaper check)
    if (prevData.maxCumulativeQuantity !== nextData.maxCumulativeQuantity) {
      return false;
    }

    // Compare first bid/ask prices (most likely to change)
    const prevBestBid = prevData.bids?.[0]?.priceStr;
    const prevBestAsk = prevData.asks?.[0]?.priceStr;
    const nextBestBid = nextData.bids?.[0]?.priceStr;
    const nextBestAsk = nextData.asks?.[0]?.priceStr;

    if (prevBestBid !== nextBestBid || prevBestAsk !== nextBestAsk) {
      return false;
    }

    // All checks passed
    return true;
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
    paddingVertical: 10,
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
    fontSize: 11,
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
    marginVertical: 1,
    paddingHorizontal: 8,
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
    fontSize: 13,
    fontWeight: '500',
    fontFamily: 'monospace',
    fontVariant: ['tabular-nums'],
    minWidth: 80,
  },
  bestPriceText: {
    fontWeight: '700',
    fontSize: 13.5,
  },
  amountText: {
    fontSize: 12,
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
  barFill: {
    height: BAR_HEIGHT,
    position: 'absolute',
    top: 0,
    borderRadius: 2,
  },
});
