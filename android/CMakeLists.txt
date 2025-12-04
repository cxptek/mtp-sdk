project(cxpmobile_tpsdk)
cmake_minimum_required(VERSION 3.9.0)

set(PACKAGE_NAME cxpmobile_tpsdk)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define C++ library and add all sources
add_library(${PACKAGE_NAME} SHARED 
  src/main/cpp/cpp-adapter.cpp
  ../cpp/TpSdkCppHybrid.cpp
  ../cpp/WebSocketMessageProcessor.cpp
  ../cpp/Utils.cpp
  # Simdjson implementation
  ../cpp/simdjson.cpp
  # Core optimized components
  ../cpp/core/SimdjsonParser.cpp
  ../cpp/core/MemoryDebug.cpp
  # Helpers
  ../cpp/helpers/JsonHelpers.cpp
  # Managers
  ../cpp/managers/OrderBookManager.cpp
  ../cpp/managers/TradesManager.cpp
  ../cpp/managers/KlineManager.cpp
  ../cpp/managers/TickerManager.cpp
  ../cpp/managers/UserDataManager.cpp
  ../cpp/managers/LifecycleManager.cpp
)

# Add Nitrogen specs - this will add generated sources, includes, and link libraries
include(${CMAKE_SOURCE_DIR}/../nitrogen/generated/android/cxpmobile_tpsdk+autolinking.cmake)

# Set up local includes for project-specific source files
# (Nitrogen autolinking.cmake already includes nitrogen/generated paths)
include_directories(
  src/main/cpp
  ../cpp
  ../cpp/core
  ../cpp/helpers
  ../cpp/managers
)

find_library(LOG_LIB log)

# Link additional libraries (Nitrogen autolinking.cmake already links fbjni, ReactAndroid, etc.)
target_link_libraries(
        ${PACKAGE_NAME}
        ${LOG_LIB}
        android # <-- Android core
)
