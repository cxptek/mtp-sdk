/**
 * Subscription Manager
 *
 * Manages TypeScript subscriptions and bridges to C++ single-callback interface.
 * C++ only supports one callback per feature, so this manager forwards
 * C++ callbacks to all TypeScript subscriptions.
 */

export class SubscriptionManager<T> {
  private subscriptions = new Map<string, (data: T) => void>();
  private cppCallback: ((data: T) => void) | null = null;
  private isRegistered = false;

  constructor(
    private registerWithCpp: (callback: (data: T) => void) => void,
    private unregisterFromCpp: () => void
  ) {}

  subscribe(callback: (data: T) => void): string {
    const id = this.generateId();
    this.subscriptions.set(id, callback);

    if (!this.cppCallback) {
      this.cppCallback = (data: T) => {
        console.log(
          `[SubscriptionManager] C++ callback triggered, subscriptions=${this.subscriptions.size}`
        );
        this.subscriptions.forEach((cb) => {
          try {
            cb(data);
          } catch (error) {
            console.error('[SubscriptionManager] Callback error:', error);
            // Silent fail - callback errors should not crash the app
          }
        });
      };
      console.log('[SubscriptionManager] Registering C++ callback');
      this.registerWithCpp(this.cppCallback);
      this.isRegistered = true;
    }

    return id;
  }

  unsubscribe(id: string): void {
    this.subscriptions.delete(id);

    if (this.subscriptions.size === 0 && this.isRegistered) {
      this.unregisterFromCpp();
      this.cppCallback = null;
      this.isRegistered = false;
    }
  }

  clear(): void {
    if (this.isRegistered) {
      this.unregisterFromCpp();
      this.isRegistered = false;
    }
    this.subscriptions.clear();
    this.cppCallback = null;
  }

  get size(): number {
    return this.subscriptions.size;
  }

  private generateId(): string {
    return `sub_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
  }
}
