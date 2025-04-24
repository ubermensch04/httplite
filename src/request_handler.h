#ifndef REQUEST_HANDLER_H
#define REQUEST_HANDLER_H
#include <string>
#include <unordered_map>

std::pair<std::string, bool> handle_connection(std::string& request_line,std::string& headers,std::string& body,const std::string& directory);
std::unordered_map<std::string, std::string> parse_headers(const std::string& headers_block);
#endif