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
			timer_manager::TimerId new_id = manager->add_timer(2, SelfExtend(manager));
			ssMessage << ", extended timer(" << id << ") new_id(" << new_id << ")" << std::endl;
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

int main(int argc, char** argv) {
	using namespace std;
	boost::shared_ptr<timer_manager> manager(new timer_manager);

	manager->add_timer(1, boost::bind(TimePrint(), _1, "timeout"));
	manager->add_timer(2, boost::bind(TimePrint(), _1, "timeout"), boost::bind(TimePrint(), _1, "cancel"));
	timer_manager::TimerId cancel_id = manager->add_timer(5, boost::bind(TimePrint(), _1, "timeout"), boost::bind(TimePrint(), _1, "cancel"));

	boost::thread manager_thread(boost::ref(*manager));
	manager->add_timer(3, boost::bind(TimePrint(), _1, "timeout"));
	manager->add_timer(6, boost::bind(TimePrint(), _1, "timeout"));
	manager->add_timer(9, boost::bind(TimePrint(), _1, "timeout"));
	manager->add_timer(1, SelfExtend(manager));
	manager->add_timer(10, TimerManagerFinish(manager));
	sleep(4);

	cout << "cancelling timer " << cancel_id << std::endl;
	manager->cancel_timer(cancel_id);
	manager->add_timer(2, boost::bind(TimePrint(), _1, "timeout"));

	sleep(10);

	//cout << "Stopping timer manager" << std::endl;
	//manager->stop();
	cout << "Joining manager thread" << std::endl;
	manager_thread.join();
	cout << "Test programs finished" << std::endl;
	return 0;
}

