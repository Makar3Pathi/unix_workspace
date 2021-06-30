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

//Change the sturct of the file to make it symmetric with hpp
//Use str.size() or length instead of std::strlen
//Compare to coding standard

std::string server::extract_path() { //Change blocks, { should be on the new line
    char buffer[0];
    unsigned int length = sizeof(buffer);
    _NSGetExecutablePath(buffer, &length);//An empty call should return expected buffer length?
    //check this out, somehow, this should return expected length
    std::vector<char> buf(length);
    _NSGetExecutablePath(buf.data(), &length);
    auto new_path = std::string(buf.data());
    std::string path(buffer);
    return path.substr(0, path.find_last_of("/"));
}

std::string server::collect_dir_info() {
    std::string info = "";
    //pass and or save data in the array of structs
    std::string path_to_dir = extract_path();
    struct dirent *entry = NULL;
    std::unique_ptr<DIR> server_dir = std::make_unique<DIR>(opendir(path_to_dir.c_str()));
    struct stat file_info;
    std::ostringstream writer;
    
    std::cout << "Current working directory is " << path_to_dir << std::endl;
    writer << "Server's current location is: " << path_to_dir << std::endl << std::endl;
    
    if (server_dir == NULL) {
        std::cout << "Error finding such a directory: " << strerror(errno) << std::endl;
        writer << "Error with getting info";
        return writer.str();
    }
    
    while((entry = readdir(server_dir.get())) != NULL) { //unique_ptr.get() returns an inside object of a unique_ptr
        stat((std::string(path_to_dir) + "/" + std::string(entry->d_name)).c_str(), &file_info);
        writer << (entry->d_name) << " " << file_info.st_mode<< " " << std::to_string(file_info.st_size) + " bytes" << " " << "owner: " << getpwuid(file_info.st_uid)->pw_name<< " group: " << getgrgid(file_info.st_gid)->gr_name << std::endl << std::endl;
    }
    //add custom deleter for unique_ptr, so it will closedir instead of me
    info = writer.str();
    return info;
}

static int initialize_socket(unsigned int &fd) {
    std::cout<< "SOME CHANGES IN LIB TO CHECK IF IT WILL RECOMPILE" << std::endl;
    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        std::cout << "Error creating a socket! " << strerror(errno) << std::endl;
        return -1;
    }
    return fd; //A socket wrapper should handle close(fd) everywhere in the project. Make RAII object for socket
}

server::server(const std::string& bind_path) {
    //Make RAII object for socket
    initialize_socket(server_fd);
    std::cout << "Server's fd is " << server_fd << std::endl;
    len = sizeof(client_addr);
    
    std::strcpy(server_addr.sun_path, bind_path.c_str());
    server_addr.sun_family = AF_UNIX;
    unlink(bind_path.c_str());
    
    if (bind(server_fd, (struct sockaddr *)&server_addr, len) < 0) {
        std::cout << "Error binding the socket! " << strerror(errno) << std::endl;
        close(server_fd);
        return; //Get bind out of constructor or return exc
    }
    std::cout << "Socket was bound to a location " << server_addr.sun_path << std::endl;
}

void server::send_message(const std::string& msg) {
    size_t len = msg.size();
    if (send(client_fd, msg.c_str(), (int)len, 0) < 0) {
        std::cout << "Error while sending info to a client " << strerror(errno) << std::endl;
        close(server_fd);
        return;
    }
    std::cout << "Info was sent" << std::endl;
}

void server::get_connections() {
    listen_for_connection();
    is_up = true;
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
    if(listen(server_fd, 0) < 0) { //Name number constants
         std::cout << "Error starting listening to connections! " << strerror(errno) << std::endl;
        close(server_fd);
        return;
    }
    std::cout << "Server is listening..." << std::endl;
}

void server::receive_messages() {
    //Read in chunks
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

client::client(const std::string& bind_path) { 
    initialize_socket(client_fd);
    std::cout << "Client's fd is " << client_fd << std::endl;
    memset(&server_addr, 0, sizeof(sockaddr_un));
    len = sizeof(server_addr);
    
    std::strcpy(server_addr.sun_path, bind_path.c_str());
    server_addr.sun_family = AF_UNIX;
}

int client::connect_to_server()
{
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
            //Store read objects (in the array of structs)
        }
        printf("%s", buffer);
    }
    close(client_fd);
    }

void client::get_dir_info() {//change to a more suitable name
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
