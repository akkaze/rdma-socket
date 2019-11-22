all: server client

CXXFLAGS += -g -O0 -std=c++11

LIBS += -libverbs -lrt
LDFLAGS += -pthread

%.o: %.cc
	$(CXX) $(CXXFLAGS) -c $^ -o $@

%.o: %.c
	$(CXX) $(CXXFLAGS) -c $^ -o $@
OBJS = rdma_channel.o  rdma_poller.o work_request.o
server:rdma_server.o $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ $(LIBS) -o $@

client:rdma_client.o $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ $(LIBS) -o $@

.PHONY:
clean:
	rm -f *.o server client
