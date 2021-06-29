#include "useunix.hpp"
int main(int argc, const char * argv[]) {
    
    const char socket_path[256]  = "/Users/tripathi/Desktop/Sockets/tmp/u_socket";
    client cl = client(socket_path);
    cl.get_dir_info();
    return 0;
}
