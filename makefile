CXX ?= g++

DEBUG ?= 1
ifeq ($(DEBUG), 1)
    CXXFLAGS += -g
else
    CXXFLAGS += -O2

endif

server: ./src/main.cpp  ./src/http_conn/http_conn.cpp ./src/lst_timer/lst_timer.cpp ./src/log/log.cpp ./src/sql_connection_pool/sql_connection_pool.cpp ./src/webserver/webserver.cpp ./src/config/config.cpp
	$(CXX) -o server  $^ $(CXXFLAGS) -lpthread -lmysqlclient

clean:
	rm  -r server
