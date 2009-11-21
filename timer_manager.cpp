/* -*- mode: c++; tab-width: 8; indent-tabs-mode: t; -*-
   vim: ts=8 sw=8 sts=8 noet foldmethod=syntax:
 */
/**
 *
 */
#include "timer_manager.hpp"
#include <boost/function.hpp>

timer_manager::TimerId const timer_manager::empty = std::numeric_limits<timer_manager::TimerId>::max();
//timer_manager* timer_manager::instance = 0;
//boost::mutex	timer_manager::instance_mutex;

//timer_manager& timer_manager::get() {
//	if(!instance) {
//		boost::mutex::scoped_lock instanceGuard(instance_mutex);
//		if(!instance) {
//			instance = new timer_manager;
//		}
//	}
//	return instance;
//}

/** simple container to keep one timer  */
struct timer {
	timer_manager::TimerId id;
	timer_manager::Action action;
}

timer_manager::timer_manager()
	: timeouts_()
	, last_timer_(0)
	, timeouts_mutex_()
{
}

timer_manager::~timer_manager() {
}

timer_manager::TimerId timer_manager::add_timer(timer_manager::Timeout t, Action const& a) {
	boost::mutex::scoped_lock accessGuard(timeout_mutex_);
	timer_ptr p_timer(new timer(++timer_id, a));
	TimeoutMap::iterator timer_it = timeouts_.insert(std::make_pair(t, p_timer));
	return timer_it->second->id;
}

namespace {
struct equal_id : std::unary_function<TimeoutMap::value_type, bool> {
	equal_id(timer_manager::TimerId id) : id_(id) {}
	bool operator()(TimeoutMap::value_type const& v) const {
		if(v.second) {
			return v.second->id == id_;
		}
		return false;
	}
	timer_manager::TimerId id_;
}
}

bool timer_manager::cancel_timer(timer_manager::TimerId id) {
	bool result = false;
	boost::mutex::scoped_lock accessGuard(timeout_mutex_);
	TimeoutMap::iterator timer_it = find_if(timeouts_.begin(), timeouts_.end(), equal_id(id));
	if(timer_it!=timeouts_.end()) {
		timeouts_.erase(timer_it);
		result = true;
	}
	return result;
}

#ifdef _TEST_

#include <iostream>
#include <boost/scoped_ptr.hpp>

/** simple timer action to print value on console */
struct TimePrint {
	void operator()(timer_manager::TimerId id) const {
		std::cout << "timer " << id << std::endl;
	}
}
/** simple timer action to extend its timer */
struct SelfExtend {
	SelfExtend(boost::shared_ptr<timer_manager> manager)
		: manager_(manager)
	{ }

	void operator()(timer_manager::TimerId id) const {
		std::cout << "timer " << id << std::endl;
		boost::shared_ptr<timer_manager> mgr = manager.lock();
		if(mgr) {
			timer_manager::TimerId new_id = mgr->add_timer(1000, SelfExtend(mgr));
			std::count << "extended timer(" << id <<") new_id(" << new_id << ")" << std::endl;
		}
	}
private:
	boost::weak_ptr<timer_manager> manager_;
}
/** timer action which will execute stop call on timer manager to stop timer work */
struct TimerManagerFinish {
	TimerManagerFinish(boost::shared_ptr<timer_manager> manager)
		: manager_(manager)
	{ }
	void operator()(timer_manager::TimerId id) const {
		std::cout << "timer " << id << std::endl;
		boost::shared_ptr<timer_manager> mgr = manager.lock();
		if(mgr) {
			mgr->stop();
			std::cout << "stopped timer manager" << std::endl;
		}
	}

private:
	boost::weak_ptr<timer_manager> manager_;
}

int main(int argc, char** argv) {
	boost::shared_ptr<timer_manager> manager;

	manager->add_timer(1000, TimePrint());
	manager->add_timer(2000, TimePrint());

	boost::thread manager_thread(manager);
	manager->add_timer(3000, TimePrint());
	manager->add_timer(1000, SelfExtend(manager));
	manager->add_timer(10000, TimerManagerFinish(manager));

	sleep(10);

	manager_thread.join();
	return 0;
}
#endif // _TEST_
