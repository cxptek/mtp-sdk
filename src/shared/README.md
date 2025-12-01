# Shared Utilities

This directory contains shared utilities used across all SDK modules.

## Files

### `errors.ts`
Standardized error handling for the SDK.

**Exports:**
- `createSdkError(options)` - Create standardized SDK error
- `createSdkErrorWithCode(code, options)` - Create error with error code
- `SdkErrorCode` - Error code enum
- `SdkErrorOptions` - Error options interface

**Error Format:**
```
[TpSdk.module.function] [ERROR_CODE] message
  → REQUIRED: step1 → step2
  → Current: key1: value1, key2: value2
```

### `TpSdkInstance.ts`
Manages the singleton instance of the C++ hybrid object.

**Exports:**
- `TpSdkHybridObject` - Proxy to C++ instance

### `callbackQueue.ts`
Manages callback queue processing from C++ background thread.

**Exports:**
- `processCallbacks()` - Process queued callbacks
- `processFakeMessage()` - Process fake message for testing

## Usage

```typescript
import { createSdkError, SdkErrorCode } from '../shared/errors';
import { TpSdkHybridObject } from '../shared/TpSdkInstance';
import { processCallbacks } from '../shared/callbackQueue';
```





