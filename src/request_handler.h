#ifndef REQUEST_HANDLER_H
#define REQUEST_HANDLER_H
#include <string>

std::string handle_connection(int client_fd,const std::string& directory);

#endif
