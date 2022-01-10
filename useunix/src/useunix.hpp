#ifndef useunix_hpp
#define useunix_hpp

#include <sys/socket.h>
#include <sys/un.h>
#include <iostream>

struct file_props
{
    std::string name;
    std::string owner;
    std::string group;
    size_t size;
};


// MARK:- u_socket class:

class u_socket
{
public:
    u_socket();
    u_socket(int descriptor);
    u_socket(const u_socket& other);
    ~u_socket();
    
    int get_fd();
    
    u_socket& operator= (const u_socket& other) = delete;
    u_socket& operator= (u_socket&& other);
    u_socket& operator= (int descriptor);

private:
    int fd;
    
};

// MARK:- server class:

class server
{
    
public:
    
    server(const std::string& bind_path);
    
    void start_accepting_connections();
    
private:
    
    u_socket server_fd, client_fd;
    sockaddr_un server_addr{}, client_addr{};
    socklen_t len;
    bool is_up = false;
    
    std::string extract_path();
        
    void bind_socket();
    
    void send_message(const std::string& msg);
    
    void send_message(const std::vector<file_props>& info);
    
    void listen_for_connection();
    
    void receive_messages();
};

// MARK:- client class:

class client
{
    
public:
    client(const std::string& bind_path);
    
    void ask_for_dirinfo();
    
private:
    
    u_socket client_fd;
    struct sockaddr_un server_addr;
    socklen_t len;
    
    void connect_to_server();
    
    void request_data();
    
    void parse_response();
    
    void close_client();
};

#endif /* useunix_hpp */
