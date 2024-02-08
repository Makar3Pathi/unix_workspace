#include "useunix.hpp"
int main(int argc, const char * argv[]) {
    
    const char socket_path[]  = "/Users/makartripathi/Desktop/unix/tmp/u_socket";
    client cl = client(socket_path);
    cl.ask_for_dirinfo();
    return 0;
}
