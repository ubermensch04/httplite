#include "request_handler.h"

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <unistd.h>
#include <cstring>


std::string handle_connection(int client_fd) 
{
    std::cout << "Handling connection for FD = " << client_fd << std::endl;

    char buffer[1024];
    ssize_t bytes_received = read(client_fd, buffer, sizeof(buffer) - 1);
    if (bytes_received <= 0) 
    {
        if (bytes_received == 0) 
        {
           std::cout << "Client closed connection before request." << std::endl;
        } 
        else 
        {
           std::cerr << "Read failed for FD=" << client_fd << std::endl;
        }

        return "";
    }
    buffer[bytes_received] = '\0';

    std::string request(buffer);

    //Parsing the path
    size_t start = request.find("/");
    size_t end = request.find(" HTTP");
    std::string path;
     if (start == std::string::npos || end == std::string::npos || start >= end) 
     {
        std::cerr << "Could not parse path from request line.\n";
        return "";
    }
    path = request.substr(start, end - start);
    std::cout << "Parsed path: " << path << std::endl;

    const std::string echo_prefix = "/echo/";
    std::string req_string="";
    bool is_echo_path = false;

    if (path.rfind(echo_prefix, 0) == 0) 
    {
        is_echo_path = true;
        size_t start_content = echo_prefix.length();
        if (path.length() > start_content) 
        {
            req_string = path.substr(start_content);
        }
    }

    std::string response_str;
    if (path == "/") 
    {
      response_str = "HTTP/1.1 200 OK\r\n\r\n";
    } 
    else if (is_echo_path) 
    {
      // Build the echo response
      std::string status_line = "HTTP/1.1 200 OK\r\n";
      std::string content_type = "Content-Type: text/plain\r\n";
      std::string content_length = "Content-Length: " + std::to_string(req_string.length()) + "\r\n";
      std::ostringstream oss;
      oss << status_line << content_type << content_length << "\r\n" << req_string;
      response_str = oss.str(); 
    }
    else 
    {
      response_str = "HTTP/1.1 404 Not Found\r\n\r\n";
    }
    return response_str;
}
