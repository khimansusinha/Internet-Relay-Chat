C++ := c++

All: client server

server: server.o worker.o nick_cmd.o user_cmd.o time_cmd.o quit_cmd.o privmsg_cmd.o join_cmd.o topic_cmd.o names_cmd.o part_cmd.o all_cmd_common.o
	$(C++) -o server server.o worker.o nick_cmd.o user_cmd.o time_cmd.o quit_cmd.o privmsg_cmd.o join_cmd.o topic_cmd.o names_cmd.o part_cmd.o all_cmd_common.o
	rm -rf *.o

server.o: server.cpp
	$(C++) -c server.cpp

worker.o: worker.cpp
	$(C++) -c worker.cpp

part_cmd.o: part_cmd.cpp
	$(C++) -c part_cmd.cpp

names_cmd.o: names_cmd.cpp
	$(C++) -c names_cmd.cpp

topic_cmd.o: topic_cmd.cpp
	$(C++) -c topic_cmd.cpp

join_cmd.o: join_cmd.cpp
	$(C++) -c join_cmd.cpp

privmsg_cmd.o: privmsg_cmd.cpp
	$(C++) -c privmsg_cmd.cpp

quit_cmd.o: quit_cmd.cpp
	$(C++) -c quit_cmd.cpp

time_cmd.o: time_cmd.cpp
	$(C++) -c time_cmd.cpp

user_cmd.o: user_cmd.cpp
	$(C++) -c user_cmd.cpp

nick_cmd.o: nick_cmd.cpp
	$(C++) -c nick_cmd.cpp

all_cmd_common.o: all_cmd_common.cpp
	$(C++) -c all_cmd_common.cpp

client: client.o
	$(C++) -o client client.o

client.o: client.cpp
	$(C++) -c client.cpp

clean:
	rm -rf *.o server client
