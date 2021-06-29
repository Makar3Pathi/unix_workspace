#ifndef useunix_hpp
#define useunix_hpp

#include <sys/socket.h>
#include <sys/un.h>
#include <iostream>

void initialize_socket(unsigned int &fd);

class server {
    
private:
    unsigned int server_fd, client_fd;
    struct sockaddr_un server_addr, client_addr;
    socklen_t len;
    bool is_up = false;
    char buffer[];
    
public:
    server(std::string bind_path);
        
    void get_connections();
    
private:
    
    std::string extract_path();

    std::string collect_dir_info();
    
    void send_message(std::string msg);
    
    void listen_for_connection();
    
    void receive_messages();
};

class client {
    
private:
    unsigned int client_fd;
    struct sockaddr_un server_addr;
    socklen_t len;
    
public:
    client(std::string bind_path);
    
    void get_dir_info();
    
private:
    int connect_to_server();
    
    void request_data();
    
    void parse_response();
    
    void close_client();
};

#endif /* useunix_hpp */
