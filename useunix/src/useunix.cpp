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
#include "unix_exception.cpp"
#include <pwd.h>
#include <grp.h>
// MARK:- Auxiliary stuff:

static void initialize_socket(u_socket& sock)
{
    try
    {
        sock = u_socket(socket(AF_UNIX, SOCK_STREAM, 0));
    }
    catch (int e)
    {
        throw unix_exception("Error creating a socket" + std::string(strerror(e)));
    }
}

auto derictory_obj_deleter = [](DIR* dir)
{
    closedir(dir);
};

// MARK:- Definitions for u_socket class:

u_socket::u_socket()
{
    fd = 0;
}

u_socket::u_socket(int descriptor)
{
    fd = descriptor;
    if (fd < 0) {
        int e = errno;
        throw unix_exception("Invalid descriptor error: " + std::string(strerror(e)));
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
    unsigned int length = 0;
    _NSGetExecutablePath(nullptr, &length);
    std::vector<char> buf(length);
    _NSGetExecutablePath(buf.data(), &length);
    auto path = std::string(buf.data());
    return path.substr(0, path.find_last_of("/"));
}

static void collect_dir_info(std::vector<file_props>& info) {
    file_props file;
    std::string path_to_dir = extract_path();
    struct dirent *entry = NULL;
    std::unique_ptr<DIR, decltype(derictory_obj_deleter)> server_dir {opendir(path_to_dir.c_str()), derictory_obj_deleter};
    int e = errno;
    struct stat file_info;
    std::ostringstream writer;
    
    std::cout << "Current working directory is " << path_to_dir << std::endl;
    writer << "Server's current location is: " << path_to_dir << std::endl << std::endl;
    
    if (server_dir == NULL)
    {
        throw unix_exception("Can't find such directory " + std::string(strerror(e)));
    }
    
    while ((entry = readdir(server_dir.get())) != NULL)
    {
        stat((std::string(path_to_dir) + "/" + std::string(entry->d_name)).c_str(), &file_info);
        
        file.name = entry->d_name;
        file.size = static_cast<int>(file_info.st_size);
        auto getpwuid_res = std::make_unique<passwd*>(new passwd(*getpwuid(file_info.st_uid)));
        if (getpwuid_res == NULL)
        {
            throw unix_exception("Couldn't get file's owner: getpwuid returned null");
        }
        file.owner = (*getpwuid_res)->pw_name;
        auto getgrgid_res = std::make_unique<group*>(new group(*getgrgid(file_info.st_gid)));
        if (getgrgid_res == NULL)
        {
            throw unix_exception("Couldn't get file's group: getgrgid returned null");
        }
        file.group = (*getgrgid_res)->gr_name;
        info.push_back(file);
    }
}

server::server(const std::string& bind_path)
{
    try
    {
        initialize_socket(server_fd);
    }
    catch (unix_exception& e)
    {
        std::cout << "Caught an error" << std::endl << e.what() << std::endl;
    }
    std::cout << "Server's fd is " << server_fd.get_fd() << std::endl;
    len = sizeof(client_addr);
    
    std::strcpy(server_addr.sun_path, bind_path.c_str());
    server_addr.sun_family = AF_UNIX;
    unlink(bind_path.c_str());
}

void server::bind_socket()
{
    if (bind(server_fd.get_fd(), reinterpret_cast<sockaddr*>(&server_addr), len) < 0)
    {
        throw unix_exception("Error binding to socket" + std::string(strerror(errno))); //Get bind out of constructor or return exc
    }
    std::cout << "Socket was bound to a location " << server_addr.sun_path << std::endl;
}

void server::send_message(const std::string& msg)
{
    size_t len = msg.size();
    if (send(client_fd.get_fd(), msg.c_str(), (int)len, 0) < 0)
    {
        throw unix_exception("Error while sending a message");
    }
    std::cout << "Info was sent" << std::endl;
}

void server::send_message(const std::vector<file_props>& info)
{
    std::ostringstream os;
    for (const file_props& file: info)
    {
        os << file.name << " " << file.size << " " << file.owner << " " << file.group << " ";
    }
    std::string sending_message = os.str();
    if (send(client_fd.get_fd(), sending_message.c_str(), sending_message.size(), 0) < 0)
    {
        throw unix_exception("Error while sending a message");
    }
    os.flush();
    std::cout << "Info was sent" << std::endl;
}

void server::start_accepting_connections()
{
    try
    {
        bind_socket();
        listen_for_connection();
        is_up = true;
        while (is_up)
        {
            std::cout << "Waiting for a connection..." << std::endl;
            client_fd = u_socket(accept(server_fd.get_fd(), reinterpret_cast<sockaddr *>(&client_addr), &len));
            if (client_fd.get_fd() < 0)
            {
                throw unix_exception("Error accepting connections" + std::string(strerror(errno)));
            }
            std::cout << "Accepted connection from " << getpeername(client_fd.get_fd(), (sockaddr *) &client_addr, &len) << " with fd " << client_fd.get_fd() << std::endl;
            
            receive_messages();
        }
    }
    catch (unix_exception& e)
    {
        std::cout << "Caught an error: " << std::endl << e.what() << std::endl;
    }
}

void server::listen_for_connection()
{
    if (listen(server_fd.get_fd(), 0) < 0)
    {
        throw unix_exception("Error while listening to connections" + std::string(strerror(errno)));
    }
    std::cout << "Server is listening..." << std::endl;
}

void server::receive_messages()
{
    //Read in chunks
    std::string input = "";
    char buffer[64];
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        if (read(client_fd.get_fd(),buffer, sizeof(buffer)) < sizeof(buffer))
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
}

// MARK:- Definitions for client class:

client::client(const std::string& bind_path)
{
    try
    {
        initialize_socket(client_fd);
    }
    catch(unix_exception& e)
    {
        std::cout << "Caught an error: " << std::endl << e.what() << std::endl;
        exit(0);
    }
    std::cout << "Client's fd is " << client_fd.get_fd() << std::endl;
    memset(&server_addr, 0, sizeof(sockaddr_un));
    len = sizeof(server_addr);
    
    std::strcpy(server_addr.sun_path, bind_path.c_str());
    server_addr.sun_family = AF_UNIX;
}

void client::connect_to_server()
{
    if (connect(client_fd.get_fd(), reinterpret_cast<sockaddr *>(&server_addr), len) < 0)
    {
        throw unix_exception("Error connecting to server "  + std::string(strerror(errno)));
    }
    std::cout << "Client connected to server" << std::endl;
}

void client::request_data()
{
    std::string dir_info_request = "GET dir_info";
    std::cout << "Sending a request..." << std::endl;
    if (write(client_fd.get_fd(), dir_info_request.c_str(), sizeof(dir_info_request)) < 0 )
    {
        throw unix_exception("Error sending request to the server"  + std::string(strerror(errno)));
    }
}

void client::parse_response()
{
    std::string input = "";
    char buffer[64];
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        if (read(client_fd.get_fd(),buffer, sizeof(buffer)) < sizeof(buffer))
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
    while (delim_position != input.npos)
    {
        try
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
        catch (std::exception& e)
        {
            std::cout << "Caught an error: " << std::endl << e.what() << std::endl;
        }
    }
    std::cout << "____ Received info____" << std::endl;
    for (int i = 0; i < info.size(); i++)
    {
        std::cout << info[i].name << " " << info[i].size << " " << info[i].owner << " " << info[i].group << std::endl;
    }
}

void client::ask_for_dirinfo()
{
    try
    {
        connect_to_server();
        request_data();
        std::cout << "Waiting for the server..." << std::endl;
        parse_response();
    }
    catch (unix_exception& e)
    {
        std::cout << "Caught an error: " << std::endl << e.what() << std::endl;
    }
}
