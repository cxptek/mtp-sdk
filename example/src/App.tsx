import 'react-native-gesture-handler';
import 'react-native-reanimated';
import { TpSdk } from '@cxptek/tp-sdk';
import { useEffect } from 'react';
import { StatusBar } from 'react-native';
import { AppNavigator } from './navigation/AppNavigator';
import { SafeAreaProvider, SafeAreaView } from 'react-native-safe-area-context';

// WebSocket configuration - supports both Binance and CXP
// Binance: wss://stream.binance.com/stream (Combined Streams API)
// CXP: wss://api.cxptek.com/ws (or your CXP WebSocket URL)
const BINANCE_BASE_URL = 'wss://stream.binance.com/stream';
const CXP_BASE_URL = 'wss://qc.cex-websocket.vcex.network/ws'; // Update with your actual CXP WebSocket URL

// Platform detection: Binance uses lowercase symbols without underscore (btcusdt)
// CXP uses uppercase symbols with underscore (USDT_KDG)
const DEFAULT_BINANCE_SYMBOL = 'btcusdt';
const DEFAULT_CXP_SYMBOL = 'ETH_USDT';

// Auto-detect platform from symbol format
function detectPlatform(symbol: string): 'binance' | 'cxp' {
  // CXP format: has underscore and uppercase (e.g., USDT_KDG)
  // Binance format: no underscore, lowercase (e.g., btcusdt)
  if (symbol.includes('_') && symbol === symbol.toUpperCase()) {
    return 'cxp';
  }
  return 'binance';
}

export default function App() {
  // Initialize SDK once on app start
  useEffect(() => {
    TpSdk.init((response) => {
      if (response === 'success') {
        console.log('[App] SDK initialized successfully');
      } else {
        console.error('[App] SDK initialization failed');
      }
    });
  }, []);

  // Auto-detect platform and use appropriate URL and symbol
  // Change DEFAULT_CXP_SYMBOL to DEFAULT_BINANCE_SYMBOL to use Binance
  const platform = detectPlatform(DEFAULT_CXP_SYMBOL);
  const baseUrl = platform === 'binance' ? BINANCE_BASE_URL : CXP_BASE_URL;
  const defaultSymbol =
    platform === 'binance' ? DEFAULT_BINANCE_SYMBOL : DEFAULT_CXP_SYMBOL;

  useEffect(() => {
    console.log(
      `[App] ${platform.toUpperCase()} WebSocket mode - screens will create their own connections`
    );
  }, [platform]);

  return (
    <SafeAreaProvider>
      <SafeAreaView style={styles.container}>
        <StatusBar barStyle="light-content" />
        <AppNavigator binanceBaseUrl={baseUrl} defaultSymbol={defaultSymbol} />
      </SafeAreaView>
    </SafeAreaProvider>
  );
}

const styles = {
  container: {
    flex: 1,
    backgroundColor: '#0f0f10',
  },
} as const;
