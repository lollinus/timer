/* -*- mode: c++; tab-width: 8; indent-tabs-mode: t; -*- vim: set ts=8 sw=8 sts=8 noet foldmethod=syntax: */
/**
 *
 */
#include "timer_manager.hpp"

// Boost libraries
#include <boost/thread.hpp>
#include <boost/function.hpp>
#include <boost/weak_ptr.hpp>

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
	, is_stopping_(false)
{
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
	std::cout << "add_timer(Timeout " << timeout << ", Action " << timeout_action << ")"
		  << " -> TimerId " << last_timer_
		  << " std::time(0) " << std::time(0)
		  << " timeout " << absolute_timeout << std::endl;
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
		Action a = timer_it->second->cancel_action;
		if(a) {
			a(id);
		}
		timeouts_.erase(timer_it);
		result = true;
	}
	wait_condition_.notify_one();
	return result;
}

void timer_manager::stop() {
	boost::mutex::scoped_lock accessGuard(manager_mutex_);
	is_stopping_ = true;
	wait_condition_.notify_one();
}

void timer_manager::operator()() {
	using namespace boost::posix_time;
	using namespace std;
	for(;;) {
		{ // check if timer manager is stopping
		boost::mutex::scoped_lock stoppingLock(manager_mutex_);
		if(is_stopping_) {
			cout << "Timer manager is stopping" << std::endl;
			/// @todo call all left timeouts to notify consumers that operation is closing
			return;
		}
		}
		{ // serve currently matched timeouts
		boost::mutex::scoped_lock accessGuard(timeouts_mutex_);
		std::time_t now = time(0);
		// read current timeouts and execute actions for them
		std::pair<TimeoutIterator, TimeoutIterator> match_time = timeouts_.equal_range(now);
		for(TimeoutIterator action_it=timeouts_.begin(); action_it!=match_time.second; ++action_it) {
			Action a = action_it->second->timeout_action;
			if(a) {
				a(action_it->second->id);
			}
		}
		timeouts_.erase(timeouts_.begin(), match_time.second);
		/// @todo add check if time is not met for next timeout
		if(!timeouts_.empty()) {
			TimeoutIterator next_timeout_it = timeouts_.begin();
			boost::xtime next_timeout;
			next_timeout.sec = next_timeout_it->first;
			wait_condition_.timed_wait(accessGuard, next_timeout);
			std::cout << "timed_wait wakeup " << std::time(0) << std::endl;
		} else {
			wait_condition_.wait(accessGuard);
			std::cout << "wait wakeup " << std::time(0) << std::endl;
		}
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
};
/** simple timer action to extend its timer */
struct SelfExtend {
	SelfExtend(boost::shared_ptr<timer_manager> manager)
		: manager_(manager)
	{ }

	void operator()(timer_manager::TimerId id) const {
		std::cout << "timer " << id << std::endl;
		boost::shared_ptr<timer_manager> manager = manager_.lock();
		if(manager) {
			timer_manager::TimerId new_id = manager->add_timer(1, SelfExtend(manager));
			std::cout << "extended timer(" << id << ") new_id(" << new_id << ")" << std::endl;
		} else {
			std::cout << "Error manager pointer invalid" << std::endl;
		}
	}
private:
	boost::weak_ptr<timer_manager> manager_;
};
/** timer action which will execute stop call on timer manager to stop timer work */
struct TimerManagerFinish {
	TimerManagerFinish(boost::shared_ptr<timer_manager> manager)
		: manager_(manager)
	{ }
	void operator()(timer_manager::TimerId id) const {
		std::cout << "timer " << id << std::endl;
		boost::shared_ptr<timer_manager> manager = manager_.lock();
		if(manager) {
			manager->stop();
			std::cout << "send stop message to timer manager" << std::endl;
		} else {
			std::cout << "wrong timer manager pointer" << std::endl;
		}
	}

private:
	boost::weak_ptr<timer_manager> manager_;
};

int main(int argc, char** argv) {
	using namespace std;
	boost::shared_ptr<timer_manager> manager(new timer_manager);

	manager->add_timer(1, TimePrint());
	manager->add_timer(2, TimePrint(), TimePrint());
	timer_manager::TimerId cancel_id = manager->add_timer(5, TimePrint(), TimePrint());

	boost::thread manager_thread(boost::ref(*manager));
	manager->add_timer(3, TimePrint());
	manager->add_timer(6, TimePrint());
	manager->add_timer(9, TimePrint());
	//manager->add_timer(1, SelfExtend(manager));
	//manager->add_timer(1, TimerManagerFinish(manager));
	sleep(4);

	cout << "cancelling timer " << cancel_id << std::endl;
	manager->cancel_timer(cancel_id);
	manager->add_timer(2, TimePrint());

	sleep(10);

	cout << "Stopping timer manager" << std::endl;
	manager->stop();
	cout << "Joining manager thread" << std::endl;
	manager_thread.join();
	cout << "Test programs finished" << std::endl;
	return 0;
}
#endif // _TEST_
