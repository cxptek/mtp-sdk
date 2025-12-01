import 'react-native-gesture-handler';
import 'react-native-reanimated';
import { TpSdk } from '@cxptek/tp-sdk';
import { useEffect, useRef, useState } from 'react';
import { StatusBar } from 'react-native';
import { AppNavigator } from './navigation/AppNavigator';
import { SafeAreaProvider, SafeAreaView } from 'react-native-safe-area-context';

const WEBSOCKET_URL = 'wss://qc.cex-websocket.vcex.network/ws';

export default function App() {
  const [ws, setWs] = useState<WebSocket | null>(null);
  const wsRef = useRef<WebSocket | null>(null);

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

  // WebSocket connection management
  // Each screen will handle its own message subscriptions
  useEffect(() => {
    console.log('[App] Creating WebSocket connection to:', WEBSOCKET_URL);
    const websocket = new WebSocket(WEBSOCKET_URL);
    wsRef.current = websocket;
    setWs(websocket);

    websocket.onopen = () => {
      console.log('[App] WebSocket connected');
    };

    websocket.onerror = (error: Event) => {
      // Ignore system warnings that don't affect functionality
      const errorMessage = error.toString();
      if (
        errorMessage.includes('nw_protocol_socket_set_no_wake_from_sleep') ||
        errorMessage.includes('SO_NOWAKEFROMSLEEP')
      ) {
        return;
      }
      console.error('[App] WebSocket error:', error);
    };

    websocket.onclose = (event: { code?: number; reason?: string }) => {
      console.log('[App] WebSocket disconnected, code:', event.code);
    };

    return () => {
      console.log('[App] Cleaning up WebSocket');
      if (
        websocket.readyState === WebSocket.OPEN ||
        websocket.readyState === WebSocket.CONNECTING
      ) {
        websocket.close();
      }
      // Clear handlers
      websocket.onopen = null;
      websocket.onerror = null;
      websocket.onclose = null;
      wsRef.current = null;
      setWs(null);
    };
  }, []);

  // Dummy processing stats for compatibility with screens that expect it
  const processingStats = {
    totalProcessed: 0,
    queueProcessed: 0,
    lastMessageType: 'none',
    lastProcessTime: 0,
  };

  return (
    <SafeAreaProvider>
      <SafeAreaView style={styles.container}>
        <StatusBar barStyle="light-content" />
        <AppNavigator ws={ws} processingStats={processingStats} />
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
