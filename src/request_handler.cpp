#include "request_handler.h"

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <unistd.h>
#include <cstring>
#include <algorithm>
#include <cctype>
#include <unordered_map>


std::string trim(const std::string& str) 
{
  size_t first = str.find_first_not_of(" \t\r\n");
  if (std::string::npos == first) {
      return ""; // Return empty if all whitespace
  }
  size_t last = str.find_last_not_of(" \t\r\n");
  return str.substr(first, (last - first + 1));
}

//Function that returns the request target
std::string parse_request_target(std::string& request_line) 
{
    size_t start=request_line.find("/");
    size_t end=request_line.find(" HTTP");
    if(start==std::string::npos || end==std::string::npos)
    {
        std::cerr<<"Invalid request line\n";
        return "";
    }
    return request_line.substr(start,end-start);
}

std::unordered_map<std::string, std::string> parse_headers(const std::string& headers_block) // Takes the string block
{
    std::unordered_map<std::string,std::string> header_data;
    // Use the input parameter string here
    std::istringstream header_stream(headers_block);
    std::string line;
    while (std::getline(header_stream, line))
    {
        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos)
        {
            std::string key = line.substr(0, colon_pos);
            std::string value = line.substr(colon_pos + 1);

            key = trim(key);
            value = trim(value);

            std::transform(key.begin(), key.end(), key.begin(),[](unsigned char c){ return std::tolower(c); });

            if (!key.empty()) 
            {
              header_data[key] = value;
            }
        }
    }
    return header_data;
}

std::string handle_connection(int client_fd) 
{
    std::string request_data;
    char buffer[1024];
    const std::string EOH="\r\n\r\n";
    ssize_t bytes_received;

    // Read the request data from the client
    while (true)
    {
        bytes_received=read(client_fd,buffer,sizeof(buffer));
        if(bytes_received<=0)
        {
          std::cerr<<"Error reading from client socket or connection closed\n";
          return "";
        }
        else
        {
          request_data.append(buffer,bytes_received);
          if(request_data.find(EOH)!=std::string::npos)
          {
            request_data.resize(request_data.find(EOH) + EOH.length());
            break;
          }
        }
    }

    //Storing the request line
    size_t first_crlf = request_data.find("\r\n");
    if (first_crlf == std::string::npos) 
    {
        std::cerr << "Invalid request data\n";
        return "";
    }
    std::string request_line=request_data.substr(0,first_crlf);
    size_t EOH_pos = request_data.find(EOH);
    std::string headers=request_data.substr(first_crlf+2,EOH_pos-first_crlf-2);
    
    //Finding Request Target
    std::string request_target=parse_request_target(request_line);
    if (request_target.empty()) 
    { 
      return ""; 
    }
    //Storing Header data in a HashMap
    std::unordered_map<std::string,std::string> header_data;
    header_data=parse_headers(headers);

    //Generating Response
    std::string response_str;
    const std::string echo_prefix = "/echo/";
    if (request_target == "/") 
    {
      response_str = "HTTP/1.1 200 OK\r\n\r\n";
    } 
    else if (request_target.rfind(echo_prefix, 0) == 0)
    {
      
      std::string req_string = request_target.substr(echo_prefix.length());
      std::ostringstream oss;
      oss << "HTTP/1.1 200 OK\r\n"
          << "Content-Type: text/plain\r\n"
          << "Content-Length: " << req_string.length() << "\r\n"
          << "\r\n"
          << req_string;
      response_str = oss.str();
    }
    else if(request_target=="/user-agent")
    {
      std::string user_agent = header_data["user-agent"];
      std::string status_line = "HTTP/1.1 200 OK\r\n";
      std::string content_type = "Content-Type: text/plain\r\n";
      std::string content_length = "Content-Length: " + std::to_string(user_agent.length()) + "\r\n";
      std::ostringstream oss;
      oss << status_line << content_type << content_length << "\r\n" << user_agent;
      response_str = oss.str();
    }
    else 
    {
      response_str = "HTTP/1.1 404 Not Found\r\n\r\n";
    }
    return response_str;
}
