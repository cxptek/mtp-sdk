package com.margelo.nitro.cxpmobile.tpsdk
  
import com.facebook.proguard.annotations.DoNotStrip

@DoNotStrip
class TpSdk : HybridTpSdkSpec() {
  override fun multiply(a: Double, b: Double): Double {
    return a * b
  }
}
