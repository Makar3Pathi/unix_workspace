#ifndef useunix_hpp
#define useunix_hpp

#include <sys/socket.h>
#include <sys/un.h>
#include <iostream>

class u_socket
{
    
};

class server
{
    
public:
    
    server(const std::string& bind_path);
        
    void get_connections(); //change the name
    
private:
    
    unsigned int server_fd, client_fd;
    sockaddr_un server_addr{}, client_addr{};
    socklen_t len;
    bool is_up = false;

    std::string extract_path();

    std::string collect_dir_info();
    
    void send_message(const std::string& msg);
    
    void listen_for_connection();
    
    void receive_messages();
};

class client
{
    
public: //Public fields, methods should come first, even if there're private fields
    client(const std::string& bind_path); //String objects should be passed by cost reference
    
    void get_dir_info(); //Change name: get - function's expected to return something
    
private:
    
    unsigned int client_fd;
    struct sockaddr_un server_addr;
    socklen_t len;

    int connect_to_server();
    
    void request_data();
    
    void parse_response();
    
    void close_client();
};

#endif /* useunix_hpp */
