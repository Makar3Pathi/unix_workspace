#include "useunix.hpp"
int main(int argc, const char * argv[]) {
    const char socket_path[]  = "/Users/makartripathi/Desktop/unix/tmp/u_socket";
    server sr = server(socket_path);
    sr.start_accepting_connections();
    return 0;
}
		
