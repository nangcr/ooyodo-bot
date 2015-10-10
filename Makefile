Ooyodo: Ooyodo.cpp
	g++ -std=c++11 -g -oOoyodo -DBACKWARD_HAS_DW=1 $(CXXFLAGS) Ooyodo.cpp backward.cpp `pkg-config --static --libs libcurl` -ldw $(LIBS)
