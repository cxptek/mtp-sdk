#include "LifecycleManager.hpp"
#include "../TpSdkCppHybrid.hpp"
#include <mutex>
#include <iostream>

namespace margelo::nitro::cxpmobile_tpsdk
{
    namespace LifecycleManager
    {
        bool isInitialized(TpSdkCppHybrid *instance)
        {
            if (instance == nullptr)
                return false;
            return instance->isInitialized_.load();
        }

        void markInitialized(TpSdkCppHybrid *instance)
        {
            if (instance != nullptr)
            {
                instance->isInitialized_.store(true);
            }
        }

        void transferStateFrom(TpSdkCppHybrid *newInstance, TpSdkCppHybrid *oldInstance)
        {
            if (oldInstance == nullptr || oldInstance == newInstance)
            {
                return;
            }

            try
            {
                // Transfer single-symbol state
                // Note: Cannot copy structs directly because they contain std::mutex (non-copyable)
                // Must copy data members manually, excluding mutex
                {
                    // OrderBook state is now handled in RN store, no copying needed
                }

                {
                    std::lock_guard<std::mutex> oldLock(oldInstance->tradesState_.mutex);
                    std::lock_guard<std::mutex> newLock(newInstance->tradesState_.mutex);
                    // Copy all data members except mutex
                    newInstance->tradesState_.queue = oldInstance->tradesState_.queue;
                    newInstance->tradesState_.maxRows = oldInstance->tradesState_.maxRows;
                    // mutex is not copied - new instance has its own mutex
                }

                {
                    std::lock_guard<std::mutex> oldLock(oldInstance->klineState_.mutex);
                    std::lock_guard<std::mutex> newLock(newInstance->klineState_.mutex);
                    // Copy all data members except mutex
                    newInstance->klineState_.data = oldInstance->klineState_.data;
                    // mutex is not copied - new instance has its own mutex
                }

                {
                    std::lock_guard<std::mutex> oldLock(oldInstance->tickerState_.mutex);
                    std::lock_guard<std::mutex> newLock(newInstance->tickerState_.mutex);
                    // Copy all data members except mutex
                    newInstance->tickerState_.data = oldInstance->tickerState_.data;
                    // mutex is not copied - new instance has its own mutex
                }

                {
                    std::lock_guard<std::mutex> oldLock(oldInstance->orderBookCallbackMutex_);
                    std::lock_guard<std::mutex> newLock(newInstance->orderBookCallbackMutex_);
                    newInstance->orderBookCallback_ = oldInstance->orderBookCallback_;
                }

                // Trading pair management removed - app manages trading pairs

                {
                    std::lock_guard<std::mutex> oldLock(oldInstance->tradesCallbackMutex_);
                    std::lock_guard<std::mutex> newLock(newInstance->tradesCallbackMutex_);
                    newInstance->tradesCallback_ = oldInstance->tradesCallback_;
                }

                {
                    std::lock_guard<std::mutex> oldLock(oldInstance->miniTickerCallbackMutex_);
                    std::lock_guard<std::mutex> newLock(newInstance->miniTickerCallbackMutex_);
                    newInstance->miniTickerCallback_ = oldInstance->miniTickerCallback_;
                }

                {
                    std::lock_guard<std::mutex> oldLock(oldInstance->miniTickerPairCallbackMutex_);
                    std::lock_guard<std::mutex> newLock(newInstance->miniTickerPairCallbackMutex_);
                    newInstance->miniTickerPairCallback_ = oldInstance->miniTickerPairCallback_;
                }

                {
                    std::lock_guard<std::mutex> oldLock(oldInstance->klineCallbackMutex_);
                    std::lock_guard<std::mutex> newLock(newInstance->klineCallbackMutex_);
                    newInstance->klineCallback_ = oldInstance->klineCallback_;
                }

                {
                    std::lock_guard<std::mutex> oldLock(oldInstance->userDataCallbackMutex_);
                    std::lock_guard<std::mutex> newLock(newInstance->userDataCallbackMutex_);
                    newInstance->userDataCallback_ = oldInstance->userDataCallback_;
                }
            }
            catch (const std::system_error &e)
            {
                std::cerr << "[C++ ERROR] System error during state transfer: " << e.what() << std::endl;
            }
            catch (const std::exception &e)
            {
                std::cerr << "[C++ ERROR] Exception during state transfer: " << e.what() << std::endl;
            }
            catch (...)
            {
                std::cerr << "[C++ ERROR] Unknown exception during state transfer" << std::endl;
            }
        }

        void clearOldInstanceData(TpSdkCppHybrid *oldInstance)
        {
            if (oldInstance == nullptr)
            {
                return;
            }

            try
            {
                // Clear all data in old instance
                {
                    // OrderBook state is now handled in RN store, no cleanup needed
                }

                {
                    std::lock_guard<std::mutex> lock(oldInstance->tradesState_.mutex);
                    oldInstance->tradesState_.clear();
                }

                {
                    std::lock_guard<std::mutex> lock(oldInstance->klineState_.mutex);
                    oldInstance->klineState_.clear();
                }

                {
                    std::lock_guard<std::mutex> lock(oldInstance->tickerState_.mutex);
                    oldInstance->tickerState_.data = TickerMessageData();
                }

                // Trading pair management removed - app manages trading pairs

                // Clear all callbacks in old instance (React components will re-register when they mount)
                {
                    std::lock_guard<std::mutex> lock(oldInstance->orderBookCallbackMutex_);
                    oldInstance->orderBookCallback_ = nullptr;
                }

                {
                    std::lock_guard<std::mutex> lock(oldInstance->tradesCallbackMutex_);
                    oldInstance->tradesCallback_ = nullptr;
                }

                {
                    std::lock_guard<std::mutex> lock(oldInstance->miniTickerCallbackMutex_);
                    oldInstance->miniTickerCallback_ = nullptr;
                }

                {
                    std::lock_guard<std::mutex> lock(oldInstance->miniTickerPairCallbackMutex_);
                    oldInstance->miniTickerPairCallback_ = nullptr;
                }

                {
                    std::lock_guard<std::mutex> lock(oldInstance->klineCallbackMutex_);
                    oldInstance->klineCallback_ = nullptr;
                }

                {
                    std::lock_guard<std::mutex> lock(oldInstance->userDataCallbackMutex_);
                    oldInstance->userDataCallback_ = nullptr;
                }
            }
            catch (const std::system_error &e)
            {
                std::cerr << "[C++ ERROR] System error during old instance cleanup: " << e.what() << std::endl;
            }
            catch (const std::exception &e)
            {
                std::cerr << "[C++ ERROR] Exception during old instance cleanup: " << e.what() << std::endl;
            }
            catch (...)
            {
                std::cerr << "[C++ ERROR] Unknown exception during old instance cleanup" << std::endl;
            }
        }
    }
}
