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
#include <memory.h>

std::string server::extract_path() {
    char buffer[256];
    unsigned int length = sizeof(buffer);
    _NSGetExecutablePath(buffer, &length);
    std::string path(buffer);
    return path.substr(0, path.find_last_of("/"));
}

std::string server::collect_dir_info() {
    std::string info = "";
    std::string path_to_dir = extract_path();
    struct dirent *entry = NULL;
    DIR *server_dir = opendir(path_to_dir.c_str());
    struct stat file_info;
    std::ostringstream writer;
    
    std::cout << "Current working directory is " << path_to_dir << std::endl;
    writer << "Server's current location is: " << path_to_dir << std::endl << std::endl;
    
    if (server_dir == NULL) {
        std::cout << "Error finding such a directory: " << strerror(errno) << std::endl;
        writer << "Error with getting info";
        return writer.str();
    }
    
    while((entry = readdir(server_dir)) != NULL) {
        stat((std::string(path_to_dir) + "/" + std::string(entry->d_name)).c_str(), &file_info);
        writer << (entry->d_name) << " " << file_info.st_mode<< " " << std::to_string(file_info.st_size) + " bytes" << " " << "owner: " << getpwuid(file_info.st_uid)->pw_name<< " group: " << getgrgid(file_info.st_gid)->gr_name << std::endl << std::endl;
    }
    closedir(server_dir);
    info = writer.str();
    return info;
}

void initialize_socket(unsigned int &fd) {
    std::cout<< "SOME CHANGES IN LIB TO CHECK IF IT WILL RECOMPILE" << std::endl;
    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        std::cout << "Error creating a socket! " << strerror(errno) << std::endl;
        close(fd);
        return;
    }
}

server::server(std::string bind_path) {
    initialize_socket(server_fd);
    std::cout << "Server's fd is " << server_fd << std::endl;
    memset(&server_addr, 0, sizeof(sockaddr_un));
    memset(&client_addr, 0, sizeof(sockaddr_un));
    len = sizeof(client_addr);
    
    std::strcpy(server_addr.sun_path, bind_path.c_str());
    server_addr.sun_family = AF_UNIX;
    unlink(bind_path.c_str());
    
    if (bind(server_fd, (struct sockaddr *)&server_addr, len) < 0) {
        std::cout << "Error binding the socket! " << strerror(errno) << std::endl;
        close(server_fd);
        return;
    }
    std::cout << "Socket was bound to a location " << server_addr.sun_path << std::endl;
}

void server::send_message(std::string msg) {
    size_t len = std::strlen(msg.c_str());
    if (send(client_fd, msg.c_str(), (int)len, 0) < 0) {
        std::cout << "Error while sending info to a client " << strerror(errno) << std::endl;
        close(server_fd);
        return;
    }
    std::cout << "Info was sent" << std::endl;
}

void server::get_connections() {
    listen_for_connection();
    this -> is_up = true;
    while(is_up) {
        std::cout << "Waiting for a connection..." << std::endl;
        client_fd = accept(server_fd, (struct sockaddr *) &client_addr, &len);
        if (client_fd < 0) {
            std::cout << "Error accepting a connection! " << strerror(errno) << std::endl;
            close(server_fd);
            return;
        }
        std::cout << "Accepted connection from " << getpeername(client_fd, (struct sockaddr *) &client_addr, &len) << " with fd " << client_fd << std::endl;
        receive_messages();
    }
}

void server::listen_for_connection() {
    if(listen(server_fd, 5) < 0) {
        std::cout << "Error starting listening to connections! " << strerror(errno) << std::endl;
        close(server_fd);
        return;
    }
    std::cout << "Server is listening..." << std::endl;
}

void server::receive_messages() {
    char buffer[256];
    if (read(client_fd, buffer , sizeof(buffer)) < 0) {
        std::cout << "Error reading a message! " << strerror(errno) << std::endl;
        close(server_fd);
        return;
    }
    if (std::string(buffer).compare("GET dir_info") == 0) {
        send_message(collect_dir_info());
    }
    else {
        send_message("Unknown request");
    }
}

client::client(std::string bind_path) {
    initialize_socket(client_fd);
    std::cout << "Client's fd is " << client_fd << std::endl;
    memset(&server_addr, 0, sizeof(sockaddr_un));
    len = sizeof(server_addr);
    
    std::strcpy(server_addr.sun_path, bind_path.c_str());
    server_addr.sun_family = AF_UNIX;
}

int client::connect_to_server() {
    if(connect(client_fd, (struct sockaddr *) &server_addr, len) < 0) {
         std::cout << "Error connecting to the server with message: "  << strerror(errno)<< std::endl;
        return -1;
    }
    std::cout << "Client connected to server" << std::endl;
    return 1;
}

void client::request_data() {
   std::string dir_info_request = "GET dir_info";
    std::cout << "Sending a request..." << std::endl;
    if (write(client_fd, dir_info_request.c_str(), sizeof(dir_info_request)) < 0 ) {
        std::cout << "Error while sending request to the server " << strerror(errno) << std::endl;
        close_client();
        return;
    }
}

void client::parse_response() {
    std::cout << "____ Received info____" << std::endl;
    char buffer[256];
    while(true) {
        memset(buffer, 0, sizeof(buffer));
        if(read(client_fd, buffer, sizeof(buffer)) < sizeof(buffer)) {
            printf("%s", buffer);
            break;
        }
        printf("%s", buffer);
    }
    close(client_fd);
    }

void client::get_dir_info() {
    if(connect_to_server() < 0) {
        close_client();
        return;
    }
    request_data();
    std::cout << "Waiting for the server..." << std::endl;
    parse_response();
}

void client::close_client() {
    close(client_fd);
}
