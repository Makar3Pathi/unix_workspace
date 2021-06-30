#include "useunix.hpp"
int main(int argc, const char * argv[]) {
    const char socket_path[256]  = "/Users/tripathi/Desktop/Sockets/tmp/u_socket";
    server sr = server(socket_path);
    sr.get_connections();

}
		
