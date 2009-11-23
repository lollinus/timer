CXX=g++
INCLUDE=-I$(HOME)/include -I/usr/local/include -L/opt/sysincludes/linux64/ix86/sysincludes_1.3/usr/include
LIBS_PATH=-L/opt/sysincludes/linux64/ix86/sysincludes_1.3/usr/lib64
LIBS=-lboost_thread 
OBJECTS=timer_manager.o
FLAGS=#-DTEST__
CXXFLAGS=
LDDFLAGS=

timer_manager_test: test.o $(OBJECTS)
	$(CXX) -o $@ $(LIBS_PATH) $(LIBS) $(FLAGS) $(LDDFLAGS) $< $(OBJECTS)

.PHONY: clean
clean:
	@rm -rf *.core *.o timer_manager_test


%.o: %.cpp %.hpp
	$(CXX) -c -o $@ $(INCLUDE) $(CXXFLAGS) $(FLAGS) $< 
