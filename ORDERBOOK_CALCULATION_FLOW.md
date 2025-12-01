# OrderBook - Luồng Tính Toán Data

## 📋 Tổng Quan

Document này mô tả **chi tiết các bước tính toán** để transform raw order book data thành format hiển thị trong component OrderBook.

---

## 🔄 Luồng Tính Toán Tổng Quát

```
Raw Order Book Data (orderBooks[symbol])
    ↓
[1] Aggregate Levels (aggregateTopNFromLevels)
    ↓
[2] Sort & Slice (top N rows)
    ↓
[3] Calculate Cumulative Quantities
    ↓
[4] Format Prices & Amounts
    ↓
[5] Calculate Max Cumulative Quantity
    ↓
Formatted Order Book View (bidsRows, asksRows)
```

---

## 📊 Chi Tiết Các Bước Tính Toán

### 1. **Aggregate Levels** (`aggregateTopNFromLevels`)

**Input:**
```typescript
levels: DepthLevel[] = [[price: BigNumber, qty: BigNumber], ...]
aggStr: string = "0.01" | "1" | ...
isBid: boolean
n: number (maxRows + 2)
```

**Bước 1.1: Tạo Rounder Function**
```typescript
// makeRounder(aggBN, isBid)
if (aggBN >= 1) {
  // Round về bội số của aggregation
  isBid: price / agg * floor * agg  (round down)
  !isBid: price / agg * ceil * agg  (round up)
} else {
  // Round về decimal places
  factor = 10^decimals
  isBid: price * factor * floor / factor  (round down)
  !isBid: price * factor * ceil / factor  (round up)
}
```

**Bước 1.2: Aggregate vào Buckets**
```typescript
bucket = Map<string, BigNumber>()

for each [price, qty] in levels:
  roundedPrice = rounder(price)
  roundedKey = roundedPrice.toString()
  
  // Sum quantities cùng price level
  bucket[roundedKey] = bucket[roundedKey] + qty
  
  // Early stop khi đủ unique levels
  if (uniqueLevels >= n + buffer) break
```

**Output:**
```typescript
Record<string, BigNumber> = {
  "1234.56": BigNumber(100.5),
  "1234.57": BigNumber(200.3),
  ...
}
```

**Ví dụ:**
```
Input levels:
  [[1234.561, 50], [1234.562, 30], [1234.569, 20]]
  
Aggregation: "0.01"
Rounder: round về 0.01

Output:
  {
    "1234.56": 50 + 30 = 80,  // 1234.561, 1234.562 → 1234.56
    "1234.57": 20             // 1234.569 → 1234.57
  }
```

---

### 2. **Sort & Slice** (trong `processSide`)

**Input:**
```typescript
record: Record<string, BigNumber> (từ bước 1)
isBid: boolean
maxRows: number
```

**Bước 2.1: Sort Keys**
```typescript
keys = Object.keys(record).sort((a, b) => {
  aBN = BigNumber(a)
  bBN = BigNumber(b)
  
  if (isBid) {
    return bBN.comparedTo(aBN)  // Descending: 1234.57 > 1234.56
  } else {
    return aBN.comparedTo(bBN)  // Ascending: 1234.56 < 1234.57
  }
})
```

**Bước 2.2: Slice Top N**
```typescript
keys = keys.slice(0, maxRows)
```

**Output:**
```typescript
keys: string[] = ["1234.57", "1234.56", "1234.55", ...]  // Top N sorted
```

---

### 3. **Calculate Cumulative Quantities**

**Input:**
```typescript
keys: string[] (sorted từ bước 2)
record: Record<string, BigNumber>
isBid: boolean
```

**Bước 3.1: Tính Cumulative**
```typescript
cumulativeQuantities: Record<string, BigNumber> = {}
runningTotal = BigNumber(0)

for each key in keys (theo thứ tự sorted):
  amount = record[key]
  runningTotal = runningTotal + amount
  cumulativeQuantities[key] = runningTotal
```

**Logic:**
- **Bids**: Accumulate từ highest price (best bid) xuống
- **Asks**: Accumulate từ lowest price (best ask) lên

**Ví dụ:**
```
Bids (sorted descending):
  Key: "1234.57", amount: 20  → cumulative: 20
  Key: "1234.56", amount: 80  → cumulative: 20 + 80 = 100
  Key: "1234.55", amount: 50  → cumulative: 100 + 50 = 150

Asks (sorted ascending):
  Key: "1234.58", amount: 30  → cumulative: 30
  Key: "1234.59", amount: 40  → cumulative: 30 + 40 = 70
  Key: "1234.60", amount: 25  → cumulative: 70 + 25 = 95
```

**Output:**
```typescript
cumulativeQuantities: Record<string, BigNumber> = {
  "1234.57": BigNumber(20),
  "1234.56": BigNumber(100),
  "1234.55": BigNumber(150),
  ...
}
```

---

### 4. **Format Prices & Amounts**

**Input:**
```typescript
keys: string[] (sorted)
cumulativeQuantities: Record<string, BigNumber>
record: Record<string, BigNumber>
priceDisplayDecimals: number
baseDecimals: number
```

**Bước 4.1: Format Price**
```typescript
priceStr = formatNumberWithDecimals(key, priceDisplayDecimals)
// Ví dụ: "1234.56" → "1,234.56" (với commas)
```

**Bước 4.2: Format Amount (Cumulative)**
```typescript
cumulativeQty = cumulativeQuantities[key]

if (cumulativeQty exists) {
  amountStr = formatNumberWithDecimals(
    cumulativeQty.toString(), 
    baseDecimals
  )
} else {
  // Fallback: dùng individual amount
  amountStr = formatNumberWithDecimals(
    record[key].toString(),
    baseDecimals
  )
}
```

**Bước 4.3: Tạo OrderBookViewItem**
```typescript
items = keys.map(key => ({
  priceStr: formatNumberWithDecimals(key, priceDisplayDecimals),
  amountStr: formatNumberWithDecimals(
    cumulativeQuantities[key].toString(),
    baseDecimals
  ),
  cumulativeQuantity: cumulativeQuantities[key].toString()
}))
```

**Bước 4.4: Pad với Null Items**
```typescript
while (items.length < maxRows) {
  items.push({
    priceStr: null,
    amountStr: null,
    cumulativeQuantity: null
  })
}
```

**Output:**
```typescript
OrderBookViewItem[] = [
  {
    priceStr: "1,234.57",
    amountStr: "20.5",
    cumulativeQuantity: "20.5"
  },
  {
    priceStr: "1,234.56",
    amountStr: "100.3",
    cumulativeQuantity: "100.3"
  },
  ...
]
```

---

### 5. **Calculate Max Cumulative Quantity**

**Input:**
```typescript
bidsResult.cumulativeQuantities: Record<string, BigNumber>
asksResult.cumulativeQuantities: Record<string, BigNumber>
```

**Bước 5.1: Collect All Cumulative Values**
```typescript
allCumulativeQuantities = [
  ...Object.values(bidsResult.cumulativeQuantities),
  ...Object.values(asksResult.cumulativeQuantities)
]
```

**Bước 5.2: Find Maximum**
```typescript
maxCumulativeQuantity = 
  allCumulativeQuantities.length > 0
    ? BigNumber.maximum(...allCumulativeQuantities).toString()
    : "0"
```

**Output:**
```typescript
maxCumulativeQuantity: string = "150.5"  // Max từ cả bids và asks
```

**Purpose:** Dùng để scale depth visualization bars trong UI.

---

## 🔑 Open Order Keys Calculation

### **Luồng Tính Toán Open Order Keys**

**Input:**
```typescript
openOrders: OpenOrderType[] = [
  { price: "1234.561", action: "BUY", status: "ACTIVE", ... },
  { price: "1234.569", action: "SELL", status: "ACTIVE", ... },
  ...
]
aggregation: string = "0.01"
quoteDecimals: number = 2
```

**Bước 1: Filter Active Orders**
```typescript
activeOrders = openOrders.filter(order => 
  order.status in ["ACTIVE", "PARTIALLY_FILLED", "NEW", "LIVE", "PENDING"]
  && order.symbolCode === symbol
)
```

**Bước 2: Round Price (giống aggregateTopNFromLevels)**
```typescript
// Sử dụng cùng logic makeRounder
rounder = makeRounder(aggBN, isBid)

for each order:
  priceBN = BigNumber(order.price)
  roundedPrice = rounder(priceBN)
  
  // Tạo key giống aggregateTopNFromLevels
  keyFromAgg = decimals > 0 
    ? roundedPrice.toFixed(decimals) 
    : roundedPrice.toString()
```

**Bước 3: Format Key (giống useOrderBookView)**
```typescript
formattedKey = formatNumberWithDecimals(
  keyFromAgg,
  displayDecimals
)
// Ví dụ: "1234.56" → "1,234.56"
```

**Bước 4: Normalize Key (remove commas/spaces)**
```typescript
keyNormalized = formattedKey.replace(/[, ]/g, "")
// Ví dụ: "1,234.56" → "1234.56"
```

**Bước 5: Separate by Side**
```typescript
if (order.action === "BUY") {
  openBidKeys.add(keyNormalized)
}
if (order.action === "SELL") {
  openAskKeys.add(keyNormalized)
}
```

**Output:**
```typescript
{
  openBidKeys: Set<string> = Set(["1234.56", "1234.55", ...]),
  openAskKeys: Set<string> = Set(["1234.57", "1234.58", ...])
}
```

**Key Point:** Keys phải match **chính xác** với keys từ `useOrderBookView` sau khi normalize (remove commas/spaces).

---

## 🎯 Matching Logic trong Component

### **So sánh Keys để Highlight Rows**

**Trong OrderBook Component:**
```typescript
// Từ useOrderBookView
item.priceStr = "1,234.56"  // Formatted với commas

// Normalize để so sánh
keyNormalized = item.priceStr.replace(/[, ]/g, "")
// → "1234.56"

// Check trong openOrderBuckets
isAvailableOrder = openKeys.has(keyNormalized)
// → true nếu có order ở price này
```

**Flow:**
```
useOrderBookView:
  priceStr = "1,234.56" (formatted)
  
useOpenOrderBuckets:
  keyNormalized = "1234.56" (normalized)
  
Component:
  keyNormalized = "1,234.56".replace(/[, ]/g, "") = "1234.56"
  openKeys.has("1234.56") → true ✅
```

---

## 📐 Aggregation & Rounding Logic

### **Case 1: Aggregation >= 1** (e.g., "1", "10", "100")

```typescript
// Ví dụ: aggregation = "10"
aggBN = BigNumber("10")

// Bids (round down)
price = 1234.56
rounded = (1234.56 / 10) * floor * 10
        = (123.456) * floor * 10
        = 123 * 10
        = 1230

// Asks (round up)
price = 1234.56
rounded = (1234.56 / 10) * ceil * 10
        = (123.456) * ceil * 10
        = 124 * 10
        = 1240
```

### **Case 2: Aggregation < 1** (e.g., "0.01", "0.1", "0.001")

```typescript
// Ví dụ: aggregation = "0.01"
aggBN = BigNumber("0.01")
decimals = 2
factor = 10^2 = 100

// Bids (round down)
price = 1234.561
rounded = (1234.561 * 100) * floor / 100
        = (123456.1) * floor / 100
        = 123456 / 100
        = 1234.56

// Asks (round up)
price = 1234.561
rounded = (1234.561 * 100) * ceil / 100
        = (123456.1) * ceil / 100
        = 123457 / 100
        = 1234.57
```

**Lý do:**
- **Bids**: Round down để hiển thị giá thấp hơn (tốt cho buyer)
- **Asks**: Round up để hiển thị giá cao hơn (tốt cho seller)

---

## 🔢 Price Display Decimals Calculation

```typescript
priceDisplayDecimals = getAggregationPriceDecimals(
  effectiveAggregation,
  quoteDecimals
)
```

**Logic:**
- Nếu aggregation < 1: decimals = số chữ số sau dấu chấm của aggregation
- Nếu aggregation >= 1: decimals = 0
- Giới hạn bởi `quoteDecimals` từ trading pair config

**Ví dụ:**
```
aggregation = "0.01" → decimals = 2
aggregation = "0.001" → decimals = 3
aggregation = "1" → decimals = 0
aggregation = "10" → decimals = 0
```

---

## 📊 Complete Calculation Example

### **Input Data:**
```typescript
orderBook = {
  bids: [
    [BigNumber("1234.561"), BigNumber("50")],
    [BigNumber("1234.562"), BigNumber("30")],
    [BigNumber("1234.555"), BigNumber("20")]
  ],
  asks: [
    [BigNumber("1234.568"), BigNumber("40")],
    [BigNumber("1234.569"), BigNumber("25")]
  ]
}

aggregation = "0.01"
maxRows = 5
baseDecimals = 2
quoteDecimals = 2
```

### **Step-by-Step Calculation:**

#### **Bids Processing:**

**Step 1: Aggregate**
```
1234.561 → round(0.01, down) → 1234.56 → bucket["1234.56"] += 50
1234.562 → round(0.01, down) → 1234.56 → bucket["1234.56"] += 30
1234.555 → round(0.01, down) → 1234.55 → bucket["1234.55"] += 20

Result:
  bucket = {
    "1234.56": 80,
    "1234.55": 20
  }
```

**Step 2: Sort & Slice**
```
keys = ["1234.56", "1234.55"]  // Descending
```

**Step 3: Cumulative**
```
"1234.56": cumulative = 80
"1234.55": cumulative = 80 + 20 = 100
```

**Step 4: Format**
```
[
  {
    priceStr: "1,234.56",
    amountStr: "80.00",
    cumulativeQuantity: "80"
  },
  {
    priceStr: "1,234.55",
    amountStr: "100.00",
    cumulativeQuantity: "100"
  }
]
```

#### **Asks Processing:**

**Step 1: Aggregate**
```
1234.568 → round(0.01, up) → 1234.57 → bucket["1234.57"] += 40
1234.569 → round(0.01, up) → 1234.57 → bucket["1234.57"] += 25

Result:
  bucket = {
    "1234.57": 65
  }
```

**Step 2: Sort & Slice**
```
keys = ["1234.57"]  // Ascending
```

**Step 3: Cumulative**
```
"1234.57": cumulative = 65
```

**Step 4: Format**
```
[
  {
    priceStr: "1,234.57",
    amountStr: "65.00",
    cumulativeQuantity: "65"
  }
]
```

#### **Final Output:**
```typescript
{
  bids: [
    { priceStr: "1,234.56", amountStr: "80.00", cumulativeQuantity: "80" },
    { priceStr: "1,234.55", amountStr: "100.00", cumulativeQuantity: "100" },
    { priceStr: null, amountStr: null, cumulativeQuantity: null },  // Padding
    { priceStr: null, amountStr: null, cumulativeQuantity: null },
    { priceStr: null, amountStr: null, cumulativeQuantity: null }
  ],
  asks: [
    { priceStr: "1,234.57", amountStr: "65.00", cumulativeQuantity: "65" },
    { priceStr: null, amountStr: null, cumulativeQuantity: null },  // Padding
    { priceStr: null, amountStr: null, cumulativeQuantity: null },
    { priceStr: null, amountStr: null, cumulativeQuantity: null },
    { priceStr: null, amountStr: null, cumulativeQuantity: null }
  ],
  maxCumulativeQuantity: "100"  // Max(80, 100, 65) = 100
}
```

---

## 🔍 Key Implementation Details

### **1. Early Stop trong aggregateTopNFromLevels**
```typescript
if (unique >= n + buffer) {
  break;  // Dừng sớm khi đủ levels (n + buffer)
}
```
- **Lý do**: Performance optimization, không cần process toàn bộ levels
- **Buffer**: Đảm bảo có đủ levels sau khi sort

### **2. BigNumber Usage**
- Tất cả calculations dùng `BigNumber` để tránh precision issues
- Keys dùng `string` để tránh floating point comparison issues

### **3. Memoization**
```typescript
const { bids, asks, maxCumulativeQuantity } = useMemo(() => {
  // ... calculations
}, [orderBook, maxRows, baseDecimals, effectiveAggregation, priceDisplayDecimals])
```
- Chỉ re-calculate khi dependencies thay đổi
- Tránh unnecessary recalculations

### **4. Key Normalization**
- **useOrderBookView**: Format với commas → `"1,234.56"`
- **useOpenOrderBuckets**: Normalize → `"1234.56"`
- **Component**: Normalize để match → `"1234.56"`

---

## 📝 Summary

**Luồng tính toán chính:**
1. **Aggregate** levels theo aggregation (round up/down)
2. **Sort** keys (descending cho bids, ascending cho asks)
3. **Calculate** cumulative quantities
4. **Format** prices và amounts với decimals
5. **Calculate** max cumulative quantity

**Key Points:**
- Aggregation logic phải consistent giữa `useOrderBookView` và `useOpenOrderBuckets`
- Keys phải được normalize (remove commas/spaces) để match
- Cumulative quantities dùng cho depth visualization
- Tất cả calculations dùng BigNumber để tránh precision issues

