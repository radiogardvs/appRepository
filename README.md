Compilare
gcc server.c -o server -lws2_32 -lcjson
gcc client.c -o client -lw

./server
./client
