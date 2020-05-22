#pragma once

#include <mutex>
#include <condition_variable>
#include <chrono>

namespace LSW {
	namespace v5 {

		class SuperMutex {
			class weird_mtx {
				std::mutex m;
				bool locked = false;
			public:
				bool try_lock();
				void unlock();
				bool is_locked() const;
			} mu;
			std::mutex defu;
			std::condition_variable cond;
		public:
			void lock();
			void unlock();
		};

		class AutoLock {
			SuperMutex* you = nullptr;
			bool hasunlocked = false;
		public:
			AutoLock(SuperMutex&);
			~AutoLock();
			void unlock();
			void lock();
		};
	}
}