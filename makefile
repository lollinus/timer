CXX=g++
INCLUDE=-I$(HOME)/include
LIBS_PATH=-L$(HOME)/lib
LIBS=-lboost_thread -lpthread

timer_manager_test: timer_manager.cpp timer_manager.hpp
	$(CXX) -o $@ -D_TEST_ $<

