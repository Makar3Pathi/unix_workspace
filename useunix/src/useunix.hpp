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
    
    int start_accepting_connections();
    
private:
    
    u_socket server_fd, client_fd;
    sockaddr_un server_addr{}, client_addr{};
    socklen_t len;
    bool is_up = false;
    
    std::string extract_path();
        
    int bind_socket();
    
    int send_message(const std::string& msg);
    
    int send_message(const std::vector<file_props>& info);
    
    int listen_for_connection();
    
    int receive_messages();
};

// MARK:- client class:

class client
{
    
public:
    client(const std::string& bind_path);
    
    int ask_for_dirinfo();
    
private:
    
    u_socket client_fd;
    struct sockaddr_un server_addr;
    socklen_t len;
    
    int connect_to_server();
    
    int request_data();
    
    int parse_response();
    
    int close_client();
};

#endif /* useunix_hpp */
