#include <jni.h>
#include "cxpmobile_tpsdkOnLoad.hpp"

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*) {
  return margelo::nitro::cxpmobile_tpsdk::initialize(vm);
}
