#include <iostream>
#include <cstdlib>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include<string>
#include<sstream>

int main(int argc, char **argv) 
{
  // Flush after every std::cout / std::cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;
  
  // You can use print statements as follows for debugging, they'll be visible when running tests.
  std::cout << "Logs from your program will appear here!\n";

  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) 
  {
   std::cerr << "Failed to create server socket\n";
   return 1;
  }
  
  // Since the tester restarts your program quite often, setting SO_REUSEADDR
  // ensures that we don't run into 'Address already in use' errors
  int reuse = 1;
  if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
    std::cerr << "setsockopt failed\n";
    return 1;
  }
  
  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_addr.s_addr = INADDR_ANY;
  server_addr.sin_port = htons(4221);
  
  if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) {
    std::cerr << "Failed to bind to port 4221\n";
    return 1;
  }
  
  int connection_backlog = 5;
  if (listen(server_fd, connection_backlog) != 0) {
    std::cerr << "listen failed\n";
    return 1;
  }
  
  //Handling Clients
  while(true)
  {
    //Connecting to Clients
    struct sockaddr_in client_addr;
    int client_addr_len = sizeof(client_addr);
    std::cout << "Waiting for a client to connect...\n";
    int client_fd = accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_len);
    std::cout << "Client connected, FD = "<< client_fd<< std::endl;
    if(client_fd<0)
    {
      std::cerr<<"Accept Failed \n";
      continue;
    }

    //Reading the request sent by client
    char buffer[1024];
    ssize_t bytes_received= read(client_fd,buffer,sizeof(buffer)-1);
    if(bytes_received>0)
    {
      buffer[bytes_received] = '\0';
      std::cout << "Received: " << buffer << std::endl;
    }
    else if (bytes_received == 0) 
    {
      std::cout << "Client closed connection." << std::endl;
    } 
    else 
    {
      std::cerr << "Read failed." << std::endl;
    }

    std::string request(buffer);
    std::string request_line=request.substr(0,request.find('\n'));
    
    size_t start = request.find("/");
    size_t end = request.find(" HTTP");
    std::string path;
    if (start != std::string::npos && end != std::string::npos && start < end) {
         path = request.substr(start, end - start);
    } else {
        std::cerr << "Could not parse path from request line.\n";
        close(client_fd);
        continue;
    }
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
    //Sending response to connected client
    const char* response;
    if(req_string.length()==0)
    {
      std::string status_line = "HTTP/1.1 400 Bad Request\r\n\r\n";
      response=status_line.c_str();
    }
    else
    { 
      std::string status_line= "HTTP/1.1 200 OK\r\n";
      std::string content_type="Content-Type: text/plain\r\n";
      std::string content_length="Content-Length: "+ std::to_string(req_string.length())+"\r\n";
      std::ostringstream oss;
      oss<<status_line<<content_type<<content_length<<"\r\n"<<req_string;
      std::string response_str=oss.str();
      response=response_str.c_str();
    }

    if(send(client_fd,response,strlen(response),0)<0)
    {
      std::cerr<<"Failed to send the HTTP response"<<std::endl;
    }
    close(client_fd);
  }
  

  close(server_fd);

  return 0;
}
