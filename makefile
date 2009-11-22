CXX=g++
INCLUDE=-I$(HOME)/include -I/usr/local/include
LIBS_PATH=-L$(HOME)/lib -L/usr/local/lib
LIBS=-lboost_thread -lpthread

timer_manager_test: timer_manager.cpp timer_manager.hpp
	$(CXX) -o $@ $(INCLUDE) $(LIBS_PATH) $(LIBS) -D_TEST_ $<

