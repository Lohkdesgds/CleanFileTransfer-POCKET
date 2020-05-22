#include "supermutex.h"

namespace LSW {
	namespace v5 {

		bool SuperMutex::weird_mtx::try_lock()
		{
			if (locked) return false;
			m.lock();
			locked = true;
			m.unlock();
			return true;
		}
		void SuperMutex::weird_mtx::unlock()
		{
			m.lock();
			locked = false;
			m.unlock();
		}
		bool SuperMutex::weird_mtx::is_locked() const
		{
			return locked;
		}

		void SuperMutex::lock()
		{
			if (!mu.try_lock()) {
				std::unique_lock<std::mutex> ul(defu);
				do {
					cond.wait_until(ul, std::chrono::system_clock::now() + std::chrono::milliseconds(100));
				} while (!mu.try_lock());
			}
		}
		void SuperMutex::unlock()
		{
			if (mu.is_locked()) {
				mu.unlock();
				cond.notify_one();
				std::this_thread::sleep_for(std::chrono::milliseconds(2));
			}
		}

		AutoLock::AutoLock(SuperMutex& m)
		{
			m.lock();
			you = &m;
		}
		AutoLock::~AutoLock()
		{
			if (!hasunlocked) you->unlock();
		}
		void AutoLock::unlock()
		{
			if (!hasunlocked) {
				hasunlocked = true;
				you->unlock();
			}
		}
		void AutoLock::lock()
		{
			if (hasunlocked) {
				hasunlocked = false;
				you->lock();
			}
		}
	}
}