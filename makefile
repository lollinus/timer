CXX=g++

timer_manager_test: timer_manager.cpp timer_manager.hpp
	$(CXX) -o $@ -D_TEST_ $<

