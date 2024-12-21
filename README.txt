1.使用帶有Posix的Mingw64 https://github.com/niXman/mingw-builds-binaries/releases
2.編譯方式
g++ -o server.exe server.cpp -lws2_32
g++ -o client.exe client.cpp -lws2_32
3.執行方式
./server setting_port_number
./client server_ip server_port_number