/* -*- mode: c++; tab-width: 8: indent-tabs-mode: t; -*- vim: set ts=8 sts=8 sw=8 tw=80 noet foldmethod=syntax: */
/**
 *
 */
#include "timer_manager.hpp"

// Boost libraries
#include <boost/thread.hpp>
#include <boost/function.hpp>

timer_manager::TimerId const timer_manager::empty = std::numeric_limits<timer_manager::TimerId>::max();

/** @struct timer
 *  @brief simple container to keep one timer
 */
struct timer {
	timer(timer_manager::TimerId p_id, timer_manager::Action const& a, timer_manager::Action const& ca)
		: id(p_id), timeout_action(a), cancel_action(ca) {};
	timer_manager::TimerId id;
	timer_manager::Action timeout_action;
	timer_manager::Action cancel_action;
};

timer_manager::timer_manager()
	: timeouts_()
	, last_timer_(0)
	, timeouts_mutex_()
	, wait_condition_()
	, manager_mutex_()
	, is_stopping_(false) {
}

timer_manager::~timer_manager() {
}

timer_manager::TimerId timer_manager::add_timer(timer_manager::Timeout timeout, Action const& timeout_action) {
	return add_timer(timeout, timeout_action, 0);
}

timer_manager::TimerId timer_manager::add_timer(timer_manager::Timeout timeout, Action const& timeout_action, Action const& cancel_action) {
	boost::mutex::scoped_lock accessGuard(timeouts_mutex_);
	timer_ptr p_timer(new timer(++last_timer_, timeout_action, cancel_action));
	Timeout absolute_timeout = std::time(0) + timeout;
	TimeoutMap::iterator timer_it = timeouts_.insert(std::make_pair(absolute_timeout, p_timer));
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
	Action a;
	{ // mutex scope
		boost::mutex::scoped_lock accessGuard(timeouts_mutex_);
		TimeoutMap::iterator timer_it = find_if(timeouts_.begin(), timeouts_.end(), equal_id(id));
		if(timer_it!=timeouts_.end()) {
			Action a = timer_it->second->cancel_action;
			timeouts_.erase(timer_it);
		} else {
			// log timer not found
		}
		result = true;
		wait_condition_.notify_one();
	}
	// run action after releasing lock to avoid deadlock if action works on timer_manager
	if(a) {
		a(id);
	}
	return result;
}

void timer_manager::stop() {
	boost::mutex::scoped_lock accessGuard(manager_mutex_);
	is_stopping_ = true;
	wait_condition_.notify_one();
}

void timer_manager::operator()() {
	//using namespace boost::posix_time;
	using namespace std;
	for(;;) {
		{ // check if timer manager is stopping
			boost::mutex::scoped_lock stoppingLock(manager_mutex_);
			if(is_stopping_) {
				//cout << "Timer manager is stopping" << std::endl;
				/// @todo call all left timeouts to notify consumers that operation is closing
				return;
			}
		}
		TimeoutMap current_actions;
		{ // opening scope for mutex
			boost::mutex::scoped_lock accessGuard(timeouts_mutex_);
			std::time_t now = time(0);
			// read current timeouts and execute actions for them
			std::pair<TimeoutIterator, TimeoutIterator> match_time = timeouts_.equal_range(now);
			current_actions.insert(timeouts_.begin(), match_time.second);
			timeouts_.erase(timeouts_.begin(), match_time.second);
		} // mutex is closed here so if running action is doing something on timer_manager it will nod deadlock
		run_actions(current_actions);
		{ // opening scope for mutex
			boost::mutex::scoped_lock accessGuard(timeouts_mutex_);
			/// @todo add check if time is not met for next timeout
			if(!timeouts_.empty()) {
				TimeoutIterator next_timeout_it = timeouts_.begin();
				boost::xtime next_timeout;
				next_timeout.sec = next_timeout_it->first;
				wait_condition_.timed_wait(accessGuard, next_timeout);
				//std::cout << "timed_wait wakeup " << std::time(0) << std::endl;
			} else {
				wait_condition_.wait(accessGuard);
				//std::cout << "wait wakeup " << std::time(0) << std::endl;
			}
		}
	}
}

void timer_manager::run_actions(TimeoutMap const& actions) const {
	for(ConstTimeoutIterator action_it=actions.begin(); action_it!=actions.end(); ++action_it) {
		Action a = action_it->second->timeout_action;
		if(a) {
			a(action_it->second->id);
		}
	}
}
