import { createBottomTabNavigator } from '@react-navigation/bottom-tabs';
import { NavigationContainer } from '@react-navigation/native';
import { StyleSheet } from 'react-native';
import { OrderBookScreen } from '../screens/OrderBookScreen';
import { TradesScreen } from '../screens/TradesScreen';
import { MiniTickerScreen } from '../screens/MiniTickerScreen';
import { MiniTickerPairScreen } from '../screens/MiniTickerPairScreen';
import { KlineScreen } from '../screens/KlineScreen';

const Tab = createBottomTabNavigator();

interface AppNavigatorProps {
  binanceBaseUrl: string;
  defaultSymbol: string;
}

export function AppNavigator({
  binanceBaseUrl,
  defaultSymbol,
}: AppNavigatorProps) {
  return (
    <NavigationContainer>
      <Tab.Navigator
        screenOptions={{
          headerShown: false,
          tabBarStyle: styles.tabBar,
          tabBarActiveTintColor: '#0ecb81',
          tabBarInactiveTintColor: '#848e9c',
          tabBarLabelStyle: styles.tabBarLabel,
        }}
      >
        <Tab.Screen
          name="OrderBook"
          options={{
            title: 'Order Book',
            tabBarLabel: 'Order Book',
          }}
        >
          {() => (
            <OrderBookScreen
              binanceBaseUrl={binanceBaseUrl}
              defaultSymbol={defaultSymbol}
            />
          )}
        </Tab.Screen>
        <Tab.Screen
          name="Trades"
          options={{
            title: 'Trades',
            tabBarLabel: 'Trades',
          }}
        >
          {() => (
            <TradesScreen
              binanceBaseUrl={binanceBaseUrl}
              defaultSymbol={defaultSymbol}
            />
          )}
        </Tab.Screen>
        <Tab.Screen
          name="MiniTicker"
          options={{
            title: 'Mini Ticker',
            tabBarLabel: 'Ticker',
          }}
        >
          {() => (
            <MiniTickerScreen
              binanceBaseUrl={binanceBaseUrl}
              defaultSymbol={defaultSymbol}
            />
          )}
        </Tab.Screen>
        <Tab.Screen
          name="MiniTickerPair"
          options={{
            title: 'Ticker Pairs',
            tabBarLabel: 'Pairs',
          }}
        >
          {() => (
            <MiniTickerPairScreen
              binanceBaseUrl={binanceBaseUrl}
              defaultSymbol={defaultSymbol}
            />
          )}
        </Tab.Screen>
        <Tab.Screen
          name="Kline"
          options={{
            title: 'Kline',
            tabBarLabel: 'Kline',
          }}
        >
          {() => (
            <KlineScreen
              binanceBaseUrl={binanceBaseUrl}
              defaultSymbol={defaultSymbol}
            />
          )}
        </Tab.Screen>
      </Tab.Navigator>
    </NavigationContainer>
  );
}

const styles = StyleSheet.create({
  tabBar: {
    backgroundColor: '#1a1a1b',
    borderTopWidth: 1,
    borderTopColor: '#2a2e39',
    height: 60,
    paddingBottom: 8,
    paddingTop: 8,
  },
  tabBarLabel: {
    fontSize: 12,
    fontWeight: '600',
    textTransform: 'uppercase',
    letterSpacing: 0.5,
  },
});
