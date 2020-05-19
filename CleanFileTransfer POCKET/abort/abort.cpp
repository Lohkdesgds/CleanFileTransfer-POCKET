#include "abort.h"

namespace LSW {
	namespace v5 {
		namespace Abort {
			Abort::Abort(std::string fw, std::string dt, abort_level lv, std::function<void(void)> fc)
			{
				s_from_where = fw;
				s_details = dt;
				abort_lvl = lv;
				quick_exit = fc;
			}
			const std::string& Abort::getWhereFrom()
			{
				return s_from_where;
			}
			const std::string& Abort::getDetails()
			{
				return s_details;
			}
			const abort_level& Abort::getLevel()
			{
				return abort_lvl;
			}
			void Abort::runFunctionIfAny()
			{
				if (quick_exit) quick_exit(); // if a function defined something to be made before dying, we might want to free their stuff or something
			}


			AutoAbort::AutoAbort(const unsigned dt, std::function<void(void)> f)
			{
				t = GetTickCount64() + dt;
				call_death = f;
				autodrop = [&]() {
					while (GetTickCount64() - t > 0) {
						if (drop) return;
					} 
					if (call_death) call_death();
					exit(EXIT_FAILURE);
				};
				thr = new std::thread(autodrop);
			}
			void AutoAbort::abort()
			{
				drop = true;
				if (thr) {
					thr->join();
					delete thr;
					thr = nullptr;
				}
			}
		}
	}
}