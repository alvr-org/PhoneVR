

CC = gcc
OBJS = driver.o
CXXFLAGS = -I../../../common/libs/eigen -I../../libs/json -I../../libs/x264/include -I../../../common/libs/ifaddrs -I../../../common/src -I../../../common/libs/asio --std=c++17

driver: $(OBJS)
	$(CC) -o driver $(OBJS) $(CXXFLAGS)
