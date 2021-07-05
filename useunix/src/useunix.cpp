#include "useunix.hpp"
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/un.h>
#include <iostream>
#include <stdio.h>
#include <dirent.h>
#include <pwd.h>
#include <grp.h>
#include <sys/stat.h>
#include <sstream>
#include <mach-o/dyld.h>
#include <vector>
#include <memory.h>


// MARK:- Auxiliary stuff:

static int initialize_socket(u_socket &sock)
{
    try
    {
        sock = u_socket(socket(AF_UNIX, SOCK_STREAM, 0));
    }
    catch (int e)
    {
        std::cout << "Error creating a socket: " << strerror(e) << std::endl;
        return -1;
    }
    return 1;
}

auto del = [](DIR* dir)
{
    closedir(dir);
};

void some_error_caught(const std::string& msg, int error_no)
{
    std::cout << msg << strerror(error_no) << std::endl;
}

// MARK:- Definitions for u_socket class:

u_socket::u_socket()
{
    fd = 0;
}

u_socket::u_socket(int descriptor)
{
    fd = descriptor;
    if (fd < 0) {
        int exc = errno;
        throw exc;
    }
}

u_socket::~u_socket()
{
    close(fd);
}

int u_socket::get_fd()
{
    return fd;
}

u_socket& u_socket::operator= (u_socket&& other)
{
    fd = other.fd;
    other.fd = -1;
    return (*this);
}

u_socket& u_socket::operator= (int descriptor)
{
    this->fd = descriptor;
    return (*this);
}

// MARK:- Definitions for socket class:

static std::string extract_path()
{
    char buffer[0];
    unsigned int length = sizeof(buffer);
    _NSGetExecutablePath(buffer, &length);
    std::vector<char> buf(length);
    _NSGetExecutablePath(buf.data(), &length);
    auto path = std::string(buf.data());
    return path.substr(0, path.find_last_of("/"));
}

static int collect_dir_info(std::vector<file_props>& info) {
    file_props file;
    std::string path_to_dir = extract_path();
    struct dirent *entry = NULL;
    std::unique_ptr<DIR, decltype(del)> server_dir {opendir(path_to_dir.c_str()), del};
    struct stat file_info;
    std::ostringstream writer;
    
    std::cout << "Current working directory is " << path_to_dir << std::endl;
    writer << "Server's current location is: " << path_to_dir << std::endl << std::endl;
    
    if (server_dir == NULL)
    {
        int e = errno;
        some_error_caught("Can't find such directory ", e);
        return -1;
        
    }
    
    while((entry = readdir(server_dir.get())) != NULL)
    {
        stat((std::string(path_to_dir) + "/" + std::string(entry->d_name)).c_str(), &file_info);
        
        file.name = entry->d_name;
        file.size = (int)file_info.st_size;
        file.owner = getpwuid(file_info.st_uid)->pw_name;
        file.group = getgrgid(file_info.st_gid)->gr_name;
        info.push_back(file);
    }
    return 1;
}

server::server(const std::string& bind_path)
{
    initialize_socket(server_fd);
    
    std::cout << "Server's fd is " << server_fd.get_fd() << std::endl;
    len = sizeof(client_addr);
    
    std::strcpy(server_addr.sun_path, bind_path.c_str());
    server_addr.sun_family = AF_UNIX;
    unlink(bind_path.c_str());
    
}

int server::bind_socket()
{
    if (bind(server_fd.get_fd(), (sockaddr *) &server_addr, len) < 0)
    {
        std::cout << "Error binding the socket! " << strerror(errno) << std::endl;
        return -1; //Get bind out of constructor or return exc
    }
    std::cout << "Socket was bound to a location " << server_addr.sun_path << std::endl;
    return 1;
}

int server::send_message(const std::string& msg)
{
    size_t len = msg.size();
    if (send(client_fd.get_fd(), msg.c_str(), (int)len, 0) < 0)
    {
        int e = errno;
        some_error_caught("Error while sending info to a client ", e);
        return -1;
    }
    std::cout << "Info was sent" << std::endl;
    return 1;
}

int server::send_message(const std::vector<file_props>& info)
{
    std::ostringstream os;
    for (file_props file: info)
    {
        os << file.name << " " << file.size << " " << file.owner << " " << file.group << " ";
    }
    std::string sending_message = os.str();
    if(send(client_fd.get_fd(), sending_message.c_str(), sending_message.size(), 0) < 0)
    {
        int e = errno;
        some_error_caught("Error while sending info to a client ", e);
        return -1;
    }
    os.flush();
    std::cout << "Info was sent" << std::endl;
    return 1;
}

int server::start_accepting_connections()
{
    bind_socket();
    listen_for_connection();
    is_up = true;
    while(is_up)
    {
        std::cout << "Waiting for a connection..." << std::endl;
        client_fd = u_socket(accept(server_fd.get_fd(), (struct sockaddr *) &client_addr, &len));
        if (client_fd.get_fd() < 0)
        {
            int e = errno;
            some_error_caught("Error accepting connections ", e);
            return -1;
        }
        std::cout << "Accepted connection from " << getpeername(client_fd.get_fd(), (struct sockaddr *) &client_addr, &len) << " with fd " << client_fd.get_fd() << std::endl;
        
        receive_messages();
    }
    return 1;
}

int server::listen_for_connection()
{
    if(listen(server_fd.get_fd(), 0) < 0)
    {
        int e = errno;
        some_error_caught("Error listening to connections ", e);
        return -1;
    }
    std::cout << "Server is listening..." << std::endl;
    return 1;
}

int server::receive_messages()
{
    //Read in chunks
    std::string input = "";
    char buffer[64];
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        if(read(client_fd.get_fd(),buffer, sizeof(buffer)) < sizeof(buffer))
        {
            input += std::string(buffer).substr(0, std::strlen(buffer));
            break;
        }
        input += std::string(buffer).substr(0, std::strlen(buffer));
    }
    
    std::vector<file_props> info;
    if (input.compare("GET dir_info") == 0)
    {
        collect_dir_info(info);
        send_message(info);
    }
    else
    {
        send_message("Unknown request");
    }
    return 1;
}

// MARK:- Definitions for client class:

client::client(const std::string& bind_path)
{
    initialize_socket(client_fd);
    std::cout << "Client's fd is " << client_fd.get_fd() << std::endl;
    memset(&server_addr, 0, sizeof(sockaddr_un));
    len = sizeof(server_addr);
    
    std::strcpy(server_addr.sun_path, bind_path.c_str());
    server_addr.sun_family = AF_UNIX;
}

int client::connect_to_server()
{
    if(connect(client_fd.get_fd(), (struct sockaddr *) &server_addr, len) < 0)
    {
        int e = errno;
        some_error_caught("Error connecting to server ", e);
        return -1;
    }
    std::cout << "Client connected to server" << std::endl;
    return 1;
}

int client::request_data()
{
    std::string dir_info_request = "GET dir_info";
    std::cout << "Sending a request..." << std::endl;
    if (write(client_fd.get_fd(), dir_info_request.c_str(), sizeof(dir_info_request)) < 0 )
    {
        int e = errno;
        some_error_caught("Error sending request to the server ", e);
        return -1;
    }
    return 1;
}

int client::parse_response()
{
    std::string input = "";
    char buffer[64];
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        if(read(client_fd.get_fd(),buffer, sizeof(buffer)) < sizeof(buffer))
        {
            input += std::string(buffer).substr(0, std::strlen(buffer) - 1);
            break;
        }
        input += std::string(buffer).substr(0, std::strlen(buffer) - 1);
    }
    
    size_t delim_position = 0;
    std::string delim = " ";
    std::vector<file_props> info;
    file_props file;
    while(delim_position != input.npos)
    {
        delim_position = input.find(delim);
        file.name = input.substr(0, delim_position);
        input.erase(0, delim_position + delim.length());
        delim_position = input.find(delim);
        file.size = std::stoi(input.substr(0, delim_position));
        input.erase(0, delim_position + delim.length());
        delim_position = input.find(delim);
        file.owner = input.substr(0, delim_position);
        input.erase(0, delim_position + delim.length());
        delim_position = input.find(delim);
        file.group = input.substr(0, delim_position);
        input.erase(0, delim_position + delim.length());
        info.push_back(file);
        
    }
    std::cout << "____ Received info____" << std::endl;
    for (int i = 0; i < info.size(); i++)
    {
        std::cout << info[i].name << " " << info[i].size << " " << info[i].owner << " " << info[i].group << std::endl;
    }
    return 1;
}

int client::ask_for_dirinfo()
{
    if(connect_to_server() < 0)
    {
        int e = errno;
        some_error_caught("Error connecting to the server ", e);
        return -1;
    }
    request_data();
    std::cout << "Waiting for the server..." << std::endl;
    parse_response();
    return 1;
}
