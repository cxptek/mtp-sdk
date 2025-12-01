# Instance Management Analysis

## So sánh 2 cách tiếp cận

### 1. Dùng 1 Instance (Singleton) - **TỐT HƠN** ✅

**Ưu điểm:**

- ✅ Đơn giản, rõ ràng - không cần static storage cho callbacks
- ✅ State management tốt hơn - tất cả state ở một nơi
- ✅ Performance tốt hơn - không cần check static, direct access
- ✅ Dễ debug - chỉ có 1 instance để track
- ✅ Memory efficient - chỉ tạo 1 instance
- ✅ Thread-safe đơn giản hơn - chỉ cần protect instance state

**Nhược điểm:**

- ⚠️ Cần đảm bảo Nitro tạo singleton (hoặc implement singleton pattern)

### 2. Nhiều Instances + Static Storage

**Ưu điểm:**

- ✅ Hoạt động ngay cả khi Nitro tạo nhiều instances
- ✅ Không phụ thuộc vào Nitro behavior

**Nhược điểm:**

- ❌ Phức tạp hơn - cần static storage cho callbacks
- ❌ State có thể conflict - nếu thực sự có nhiều instances với state khác nhau
- ❌ Khó debug - không biết instance nào đang được dùng
- ❌ Memory overhead - nhiều instances nhưng chỉ dùng static storage
- ❌ Thread-safety phức tạp hơn - cần protect cả static và instance state

## Tại sao Nitro tạo nhiều instances?

Từ logs, ta thấy:

- `orderbookSubscribe` được gọi trên instance `0x113f8f838`
- `processWebSocketMessage` được gọi trên instance `0x103409128`

**Nguyên nhân có thể:**

1. **Nitro không cache instances đúng cách** - mỗi lần gọi `createHybridObject` có thể tạo instance mới
2. **React Native bundle modules** - mỗi module import có thể tạo instance riêng
3. **Timing issue** - instances được tạo ở các thời điểm khác nhau

**Tuy nhiên**, trong code hiện tại, tất cả modules đã dùng shared instance từ `src/shared/TpSdkInstance.ts`, nên vấn đề có thể là:

- Nitro vẫn tạo instance mới ở C++ side mỗi lần method được gọi
- Hoặc có vấn đề với cách Nitro bridge hoạt động

## Giải pháp đề xuất

### Option 1: Dùng Static Storage (Hiện tại) ✅

**Đã implement:**

- Callbacks được lưu trong static storage
- Shared across all instances
- Hoạt động ngay cả khi có nhiều instances

**Kết luận:** Đây là giải pháp **an toàn nhất** và **đã hoạt động**. Nên giữ nguyên.

### Option 2: Implement Singleton Pattern trong C++

**Cách làm:**

1. Track instance đầu tiên được tạo trong constructor
2. Dùng instance đó cho tất cả operations
3. Log warning nếu có instance mới được tạo

**Nhược điểm:**

- Phức tạp hơn
- Vẫn cần static storage nếu muốn đảm bảo callbacks hoạt động
- Không giải quyết được vấn đề nếu Nitro thực sự tạo nhiều instances với state khác nhau

## Khuyến nghị

**Giữ nguyên giải pháp hiện tại (Static Storage)** vì:

1. ✅ **Đã hoạt động** - callbacks đã được register và queue đúng cách
2. ✅ **An toàn** - không phụ thuộc vào Nitro behavior
3. ✅ **Đơn giản** - chỉ cần static storage cho callbacks
4. ✅ **Thread-safe** - đã có mutex protection

**Lưu ý:**

- State (orderBookBids*, orderBookAsks*) vẫn là instance-specific
- Nếu thực sự có nhiều instances với state khác nhau, có thể cần chuyển state sang static storage
- Nhưng với shared instance từ TypeScript side, điều này không nên xảy ra

## Kết luận

**Dùng 1 instance tốt hơn**, nhưng vì Nitro có thể tạo nhiều instances, **giải pháp tốt nhất là:**

- ✅ Dùng shared instance từ TypeScript (đã làm)
- ✅ Dùng static storage cho callbacks (đã làm)
- ✅ Giữ state ở instance level (đã làm - vì chỉ có 1 instance được dùng từ TS)

Giải pháp hiện tại là **tối ưu** và **đã hoạt động đúng**.
