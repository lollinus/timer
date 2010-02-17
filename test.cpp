/* -*- mode: c++; tab-width: 8: indent-tabs-mode: t; -*- vim: set ts=8 sts=8 sw=8 tw=80 noet foldmethod=syntax: */
// STL libraries
#include <iostream>
#include <functional>
#include <sstream>

// Boost libraries
#include <boost/scoped_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/bind.hpp>

#include "timer_manager.hpp"

/** simple timer action to print value on console */
struct TimePrint : std::binary_function<timer_manager::TimerId, std::string, void> {
	void operator()(timer_manager::TimerId id, std::string const& message) const {
		std::cout << message << ", id=" << id << std::endl;
	}
};
/** simple timer action to extend its timer */
struct SelfExtend {
	SelfExtend(boost::shared_ptr<timer_manager> manager)
		: manager_(manager)
	{ }

	void operator()(timer_manager::TimerId id) const {
		std::ostringstream ssMessage;
		ssMessage << "timer " << id;
		boost::shared_ptr<timer_manager> manager = manager_.lock();
		if(manager) {
			timer_handler new_timer = manager->add_timer(2, SelfExtend(manager));
			ssMessage << ", extended timer(" << id << ") new_id(" << new_timer.id_ << ")" << std::endl;
		} else {
			ssMessage << ", Error manager pointer invalid" << std::endl;
		}
		std::cout << ssMessage.str();
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
		std::ostringstream ssMessage;
		ssMessage << "timer " << id;
		boost::shared_ptr<timer_manager> manager = manager_.lock();
		if(manager) {
			manager->stop();
			ssMessage << ", send stop message to timer manager" << std::endl;
		} else {
			ssMessage << ", wrong timer manager pointer" << std::endl;
		}
		std::cout << ssMessage.str();
	}

private:
	boost::weak_ptr<timer_manager> manager_;
};

/**
 * @struct timer_manager_deleter
 * @brief Simple deleter which can be used with boost::shared_ptr storing
 * timer_manager instance. Before deleting object it will call stop on timer
 * manager object to gracefully finish if it is executed by thread.
 *
 */
struct timer_manager_deleter : std::unary_function<timer_manager*, void> {
	timer_manager_deleter() : thread_(0) {};
	void operator()(timer_manager* t) const {
		if(t) {
			t->stop();
			if(thread_) {
				using std::cout;
				cout << "Joining manager thread" << std::endl;
				thread_->join();
			}
			delete t;
		}
	}

	void setThread(boost::thread* th) {
		thread_=th;
	}

	boost::thread* thread_;
};

int main(int argc, char** argv) {
	using namespace std;
	// declare new timer_manager
	boost::shared_ptr<timer_manager> manager(new timer_manager, timer_manager_deleter());

	manager->add_timer(1, boost::bind(TimePrint(), _1, "timeout"));
	manager->add_timer(2, boost::bind(TimePrint(), _1, "timeout"), boost::bind(TimePrint(), _1, "cancel"));
	//timer_manager::TimerId cancel_id = manager->add_timer(5, boost::bind(TimePrint(), _1, "timeout"), boost::bind(TimePrint(), _1, "cancel"));
	timer_handler timer = manager->add_timer(5, boost::bind(TimePrint(), _1, "timeout"), boost::bind(TimePrint(), _1, "cancel"));

	boost::thread manager_thread(boost::ref(*manager));
	if(timer_manager_deleter* d=boost::get_deleter<timer_manager_deleter>(manager)) {
		d->setThread(&manager_thread);
	}
	for(int i=0; i<100; ++i) {
		manager->add_timer(8, boost::bind(TimePrint(), _1, " multiple timeout timeouts at the same time"));
	}
	manager->add_timer(3, boost::bind(TimePrint(), _1, "timeout"));
	manager->add_timer(6, boost::bind(TimePrint(), _1, "timeout"));
	manager->add_timer(9, boost::bind(TimePrint(), _1, "timeout"));
	manager->add_timer(1, SelfExtend(manager));
	//manager->add_timer(10, TimerManagerFinish(manager));
	sleep(4);

	cout << "cancelling timer " << timer.id_ << std::endl;
	timer.cancel();
	manager->add_timer(2, boost::bind(TimePrint(), _1, "timeout"));

	for(int i=0; i<10; ++i) {
		cout << "main thread iteration: " << i << std::endl;
		sleep(1);
	}
	manager.reset();
	//cout << "Joining manager thread" << std::endl;
	//manager_thread.join();
	cout << "Test programs finished" << std::endl;
	return 0;
}

