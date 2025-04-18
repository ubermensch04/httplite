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
#include<fstream>
#include<zlib.h>

std::string trim(const std::string& str) 
{
  size_t first = str.find_first_not_of(" \t\r\n");
  if (std::string::npos == first) {
      return "";
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

std::string gzip_compress(const std::string& str) 
{
    if(str.empty()) 
    {
        return str;
    }

    z_stream zs;
    memset(&zs, 0, sizeof(zs));

    if (deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 | 16, 8, Z_DEFAULT_STRATEGY) != Z_OK) 
    {
      throw std::runtime_error("deflateInit2 failed");
    }

    zs.next_in = (Bytef*)str.data();
    zs.avail_in = static_cast<uInt>(str.size());

    int ret;
    std::vector<char> compressed_buffer(deflateBound(&zs, str.size()));
    zs.next_out=(Bytef*)compressed_buffer.data();
    zs.avail_out = static_cast<uInt>(compressed_buffer.size());

    ret = deflate(&zs, Z_FINISH);
    if (ret != Z_STREAM_END) 
    {
        deflateEnd(&zs);
        throw std::runtime_error("deflate failed");
    }

    if (deflateEnd(&zs) != Z_OK) 
    {
       std::cerr << "Warning: deflateEnd failed." << std::endl;
    }
    size_t compressed_size = zs.total_out;

    // Return the compressed data as a string
    return std::string(compressed_buffer.data(), compressed_size);

}
std::string handle_GET_request(const std::string& request_target, const std::string& directory, const std::string& headers) 
{
  //Storing Header data in a HashMap
  std::unordered_map<std::string,std::string> header_data;
  header_data=parse_headers(headers);
  
  std::string response_str;

  //Generating Response
  const std::string echo_prefix = "/echo/";
  const std::string file_prefix = "/files/";
  if (request_target == "/") 
  {
    response_str = "HTTP/1.1 200 OK\r\n\r\n";
  } 
  else if (request_target.rfind(echo_prefix, 0) == 0)
  {
    
    std::string req_string = request_target.substr(echo_prefix.length());
    std::ostringstream oss;
    bool apply_gzip = false;

    auto it = header_data.find("accept-encoding");
    if (it != header_data.end()) 
    {

        std::string supported_encodings = it->second;
        if (supported_encodings.find("gzip") != std::string::npos) {
            apply_gzip = true;
            std::cout << "Client accepts gzip. Applying compression." << std::endl;
        } else {
            std::cout << "Client does not accept gzip (Accept-Encoding: " << supported_encodings << ")" << std::endl;
        }
    } else 
    {
        std::cout << "No Accept-Encoding header received." << std::endl;
    }

    if (apply_gzip) 
    {
        try {
            std::string compressed_body = gzip_compress(req_string);
            oss << "HTTP/1.1 200 OK\r\n";
            oss << "Content-Type: text/plain\r\n"; 
            oss << "Content-Encoding: gzip\r\n";
            oss << "Content-Length: " << compressed_body.length() << "\r\n";
            oss << "\r\n"; 
            oss << compressed_body; 
        } catch (const std::runtime_error& e) 
        {
            std::cerr << "Gzip compression failed: " << e.what() << std::endl;
            // For now, let's send uncompressed as a fallback:
            oss.str(""); 
            oss.clear(); 
            oss << "HTTP/1.1 200 OK\r\n"
                << "Content-Type: text/plain\r\n"
                << "Content-Length: " << req_string.length() << "\r\n"
                << "\r\n"
                << req_string;
        }
    } 
    else 
    {
        // Build uncompressed response
        oss << "HTTP/1.1 200 OK\r\n"
            << "Content-Type: text/plain\r\n"
            << "Content-Length: " << req_string.length() << "\r\n"
            << "\r\n"
            << req_string;
    }

    response_str = oss.str();
  }
  else if(request_target.rfind(file_prefix, 0) == 0)
  {
    std::string file_name = request_target.substr(file_prefix.length());
    if (file_name.empty() || file_name.find("..") != std::string::npos) 
    {
      std::cerr << "Attempted path traversal or empty filename: " << file_name << "\n";
      response_str = "HTTP/1.1 400 Bad Request\r\n\r\n";
    }
    else
    {
      std::string full_file_path = directory + "/" + file_name;
      std::ifstream file(full_file_path,std::ios::binary);
      if(!file)
      {
        std::cerr << "File not found: " << full_file_path << "\n";
        response_str = "HTTP/1.1 404 Not Found\r\n\r\n";
      }
      else
      {
        //Reading the file contents
        file.seekg(0, std::ios::end);
        size_t file_size = file.tellg();
        file.seekg(0, std::ios::beg);
        std::string file_content(file_size, '\0');
        file.read(&file_content[0], file_size);

        //Generating the response
        std::ostringstream oss;
        oss << "HTTP/1.1 200 OK\r\n"
            << "Content-Type: application/octet-stream\r\n"
            << "Content-Length: " << file_size << "\r\n"
            << "\r\n" 
            << file_content;
        response_str = oss.str();
        std::cout << "File sent: " << full_file_path << "\n";
      }
    }
  }
  else if(request_target=="/headers")
  {
    std::ostringstream oss;
    oss << "HTTP/1.1 200 OK\r\n"
        << "Content-Type: text/plain\r\n"
        << "Content-Length: " << headers.length() << "\r\n"
        << "\r\n" 
        << headers;
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

std::string handle_POST_request(const std::string& request_target, const std::string& directory, const std::unordered_map<std::string,std::string> header_data, const std::string& body) 
{
  
  const std::string file_prefix = "/files/";

  if(request_target.rfind(file_prefix, 0) != 0) 
  {
    return "HTTP/1.1 404 Not Found\r\n\r\n";
  }
  
  std::string file_name = request_target.substr(file_prefix.length());

  if(file_name.empty()|| file_name.find("..") != std::string::npos)
  {
    std::cerr << "Empty filename in PUT request\n";
    return "HTTP/1.1 400 Bad Request\r\n\r\n";
  }
  
  size_t content_length=0;
  if(header_data.find("content-length")!=header_data.end())
  {
    content_length=std::stoul(header_data.at("content-length"));
  }
  else
  {
    std::cerr << "Content-Length header not found\n";
    return "HTTP/1.1 400 Bad Request\r\n\r\n";
  }

  if(body.length() != content_length) {
    std::cerr << "Body length mismatch. Expected: " << content_length
              << ", Received: " << body.length() << "\n";
    return "HTTP/1.1 400 Bad Request\r\n\r\n";
}

  std::string full_file_path=directory+"/"+file_name;
  std::ofstream file(full_file_path,std::ios::binary);
  std::cout<<"Writing to file: "<<full_file_path<<"\n";
  std::cout<<"Body : "<<body<<"\n";

  if(file.write(body.data(), body.size())) 
  {
    return "HTTP/1.1 201 Created\r\n\r\n";
  } 
  else 
  {
    std::cerr << "File write failed: " << full_file_path << "\n";
    return "HTTP/1.1 500 Internal Server Error\r\n\r\n";
  }
  
}
std::string handle_connection(int client_fd,const std::string& directory) 
{
  std::string request_data;
  char buffer[1024];
  const std::string EOH="\r\n\r\n";
  ssize_t bytes_received;
  // Read the request data from the client

  size_t eoh_pos;
  while ((eoh_pos = request_data.find("\r\n\r\n")) == std::string::npos) {
      bytes_received = read(client_fd, buffer, sizeof(buffer));
      if (bytes_received <= 0) {
          std::cerr << "Connection closed before headers\n";
          return "";
      }
      request_data.append(buffer, bytes_received);
  }

  // Now parsing components
  size_t first_crlf = request_data.find("\r\n");
  std::string request_line = request_data.substr(0, first_crlf);
  std::string headers = request_data.substr(first_crlf + 2, eoh_pos - (first_crlf + 2));

  //Finding the request method
  size_t method_end = request_line.find(' ');
  if (method_end == std::string::npos) 
  {
      std::cerr << "Invalid request line\n";
      return "";
  }
  std::string method = request_line.substr(0, method_end);

  //Finding Request Target
  std::string request_target=parse_request_target(request_line);
  if (request_target.empty()) 
  { 
    return ""; 
  }

  std::string response_str;
  if(strcmp(method.c_str(),"GET")==0)
  {
    response_str=handle_GET_request(request_target,directory,headers);
  }
  else if(strcmp(method.c_str(),"POST")==0)
  {
    std::cout << "POST request received\n";
    std::unordered_map<std::string,std::string> header_data = parse_headers(headers);
    // Get content length
    size_t content_length = 0;
    if (header_data.count("content-length")) {
        try {
            content_length = std::stoul(header_data["content-length"]);
        } catch (...) {
            return "HTTP/1.1 400 Bad Request\r\n\r\n";
        }
    }
    else {
        std::cerr << "Content-Length header not found\n";
        return "HTTP/1.1 400 Bad Request\r\n\r\n";
    }

    // Reading remaining body data
    size_t body_bytes_received = request_data.size() - (eoh_pos + 4);
    while (body_bytes_received < content_length) 
    {
      bytes_received = read(client_fd, buffer, sizeof(buffer));
      if (bytes_received <= 0) {
          std::cerr << "Connection closed during body\n";
          return "";
      }
      request_data.append(buffer, bytes_received);
      body_bytes_received += bytes_received;
    }
    std::string body = request_data.substr(eoh_pos + 4, content_length);
    response_str=handle_POST_request(request_target,directory,header_data,body);
  }
  else
  {
    std::cerr << "Unsupported HTTP method: " << method << "\n";
    return "";
  }
  
  return response_str;
}
