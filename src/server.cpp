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
#include"request_handler.h"
#include<thread>


void client_thread(int client_fd,const std::string& directory)
{
  std::cout << "[Thread " << std::this_thread::get_id() << "] Started for FD=" << client_fd << std::endl;
  
  std::string response;
  response=handle_connection(client_fd,directory);

  if (!response.empty())
  {
    if(send(client_fd,response.c_str(),response.length(),0)<0)
    {
      std::cerr << "[Thread " << std::this_thread::get_id() << "] Failed to send response for FD=" << client_fd << std::endl;
    }
    else 
    {
      std::cout << "[Thread " << std::this_thread::get_id() << "] Sent response to FD=" << client_fd << std::endl;
    }
  }
  else
  {
    std::cerr << "[Thread " << std::this_thread::get_id() << "] Handler failed for FD=" << client_fd << ", no response sent." << std::endl;
  }
  close(client_fd);
  std::cout << "[Thread " << std::this_thread::get_id() << "] Finished, closed FD=" << client_fd << std::endl;
}

int main(int argc, char **argv) 
{
  // Flush after every std::cout / std::cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;
  
  std::cout << "Logs from your program will appear here!\n";

  std::cout << "Argument count: " << argc << "\n";

  std::string directory;
  if (argc == 3) { 
    if (std::strcmp(argv[1], "--directory") == 0) {
        directory = argv[2];
        std::cout << "Serving files from directory: " << directory << "\n";
    } else {
        std::cerr << "Error: Invalid flag '" << argv[1] << "'. Expected '--directory <path>'.\n";
        return 1;
    }
  } 
  else if (argc == 1) 
  { // No arguments provided
     std::cerr << "Warning: No --directory argument provided. Defaulting to '.' but file serving might fail.\n";
     directory = ".";
  } 
  else 
  { // Incorrect number of arguments
    std::cerr << "Error: Invalid arguments. Usage: ./server --directory <path>\n";
    return 1;
  }

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
    if(client_fd<0)
    {
      std::cerr << "Main thread: Accept Failed\n";
      continue;
    }
    std::cout << "Main thread: Client connected, FD = " << client_fd << std::endl;

    //Creating and detaching thread for each client
    std::thread t(client_thread, client_fd,directory);
    std::cout << "Thread created for FD=" << client_fd << std::endl;
    t.detach();
    std::cout << "Main thread: Detached thread for FD=" << client_fd << std::endl;

  }
  

  close(server_fd);

  return 0;
}
