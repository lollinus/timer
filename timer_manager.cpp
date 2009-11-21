/* -*- mode: c++; tab-width: 8; indent-tabs-mode: t; -*-
   vim: ts=8 sw=8 sts=8 noet foldmethod=syntax:
 */
/**
 *
 */
#include "timer_manager.hpp"

// Boost libraries
#include <boost/function.hpp>
#include <boost/weak_ptr.hpp>

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
	timer(timer_manager::TimerId p_id, timer_manager::Action const& a)
		: id(p_id), action(a) {};
	timer_manager::TimerId id;
	timer_manager::Action action;
}

timer_manager::timer_manager()
	: timeouts_()
	, last_timer_(0)
	, timeouts_mutex_()
	, wait_condition_()
	, manager_mutex_()
	, is_stopping_(false)
{
}

timer_manager::~timer_manager() {
}

timer_manager::TimerId timer_manager::add_timer(timer_manager::Timeout t, Action const& a) {
	boost::mutex::scoped_lock accessGuard(timeouts_mutex_);
	timer_ptr p_timer(new timer(++last_timer_, a));
	TimeoutMap::iterator timer_it = timeouts_.insert(std::make_pair(t, p_timer));
	wait_condition_.notify_one();
	return timer_it->second->id;
}

namespace {
struct equal_id : std::unary_function<timer_manager::TimeoutMap::value_type, bool> {
	equal_id(timer_manager::TimerId id) : id_(id) {}
	bool operator()(timer_manager::TimeoutMap::value_type const& v) const {
		if(v.second) {
			return v.second->id == id_;
		}
		return false;
	}
	timer_manager::TimerId id_;
};
}

bool timer_manager::cancel_timer(timer_manager::TimerId id) {
	bool result = false;
	boost::mutex::scoped_lock accessGuard(timeouts_mutex_);
	TimeoutMap::iterator timer_it = find_if(timeouts_.begin(), timeouts_.end(), equal_id(id));
	if(timer_it!=timeouts_.end()) {
		timeouts_.erase(timer_it);
		result = true;
	}
	wait_condition_.notify_one();
	return result;
}

void timer_manager::stop() {
	boost::mutex::scoped_lock accessGuard(manager_mutex_);
	is_stopping_ = true;
}

void timer_manager::operator()() const {
	using namespace boost::posix_time;
	for(;;) {
		{ // check if timer manager is stopping
		boost::scoped_lock stoppingLock(manager_mutex_);
		if(is_stopping_) {
			/// @todo call all left timeouts to notify consumers that operation is closing
			return;
		}
		}
		{ // serve currently matched timeouts
		boost::scoped_lock accessGuard(timeouts_mutex_);
		std::time_t now = time(0);
		// read current timeouts and execute actions for them
		std::pair<ConstTimeoutIterator, ConstTimeoutIterator> match_time = timeouts_::equal_range(now);
		for(ConstTimeoutIterator action_it = timeouts_.begin(); action_it != match_time.second; ++action_it) {
			action_it->second();
		}
		timeouts_.erase(timeouts_.begin(), match_time.second);
		/// @todo add check if time is not met for next timeout
		ConstTimeoutIterator next_timeout_it = timeouts_.begin();
		boost::xtime timeout(next_timeout_it->first);
		wait_condition_.timed_wait(accessGuard, timeout);
		}
	}
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
		boost::shared_ptr<timer_manager> mgr = manager_.lock();
		if(mgr) {
			timer_manager::TimerId new_id = mgr->add_timer(1000, SelfExtend(mgr));
			std::cout << "extended timer(" << id << ") new_id(" << new_id << ")" << std::endl;
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
		boost::shared_ptr<timer_manager> mgr = manager_.lock();
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
