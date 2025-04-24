#include "request_handler.h"

#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <unistd.h>
#include <cstring>
#include <algorithm>
#include <cctype>
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
std::string handle_GET_request(const std::string& request_target, const std::string& directory, const std::string& headers, bool keep_alive) 
{
  // Storing Header data in a HashMap
  std::unordered_map<std::string,std::string> header_data;
  header_data = parse_headers(headers);
  
  std::string response_str;
  std::ostringstream oss;

  // Generating Response
  const std::string echo_prefix = "/echo/";
  const std::string file_prefix = "/files/";
  
  if (request_target == "/") 
  {
    oss << "HTTP/1.1 200 OK\r\n";
    if (!keep_alive) {
      oss << "Connection: close\r\n";
    }
    oss << "Content-Length: 0\r\n\r\n";
    response_str = oss.str();
  } 
  else if (request_target.rfind(echo_prefix, 0) == 0)
  {
    std::string req_string = request_target.substr(echo_prefix.length());
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
            if (!keep_alive) {
                oss << "Connection: close\r\n";
            }
            oss << "Content-Length: " << compressed_body.length() << "\r\n";
            oss << "\r\n"; 
            oss << compressed_body; 
        } catch (const std::runtime_error& e) 
        {
            std::cerr << "Gzip compression failed: " << e.what() << std::endl;
            // Fallback to uncompressed
            oss.str(""); 
            oss.clear(); 
            oss << "HTTP/1.1 200 OK\r\n"
                << "Content-Type: text/plain\r\n";
            if (!keep_alive) {
                oss << "Connection: close\r\n";
            }
            oss << "Content-Length: " << req_string.length() << "\r\n"
                << "\r\n"
                << req_string;
        }
    } 
    else 
    {
        // Build uncompressed response
        oss << "HTTP/1.1 200 OK\r\n"
            << "Content-Type: text/plain\r\n";
        if (!keep_alive) {
            oss << "Connection: close\r\n";
        }
        oss << "Content-Length: " << req_string.length() << "\r\n"
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
      oss << "HTTP/1.1 400 Bad Request\r\n";
      if (!keep_alive) {
        oss << "Connection: close\r\n";
      }
      oss << "Content-Length: 0\r\n\r\n";
      response_str = oss.str();
    }
    else
    {
      std::string full_file_path = directory + "/" + file_name;
      std::ifstream file(full_file_path, std::ios::binary);
      if(!file)
      {
        std::cerr << "File not found: " << full_file_path << "\n";
        oss << "HTTP/1.1 404 Not Found\r\n";
        if (!keep_alive) {
          oss << "Connection: close\r\n";
        }
        oss << "Content-Length: 0\r\n\r\n";
        response_str = oss.str();
      }
      else
      {
        // Reading the file contents
        file.seekg(0, std::ios::end);
        size_t file_size = file.tellg();
        file.seekg(0, std::ios::beg);
        std::string file_content(file_size, '\0');
        file.read(&file_content[0], file_size);

        // Generating the response
        oss << "HTTP/1.1 200 OK\r\n"
            << "Content-Type: application/octet-stream\r\n";
        if (!keep_alive) {
          oss << "Connection: close\r\n";
        }
        oss << "Content-Length: " << file_size << "\r\n"
            << "\r\n" 
            << file_content;
        response_str = oss.str();
        std::cout << "File sent: " << full_file_path << "\n";
      }
    }
  }
  else if(request_target=="/headers")
  {
    oss << "HTTP/1.1 200 OK\r\n"
        << "Content-Type: text/plain\r\n";
    if (!keep_alive) {
      oss << "Connection: close\r\n";
    }
    oss << "Content-Length: " << headers.length() << "\r\n"
        << "\r\n" 
        << headers;
    response_str = oss.str();
  }
  else if(request_target=="/user-agent")
  {
    std::string user_agent = header_data["user-agent"];
    oss << "HTTP/1.1 200 OK\r\n"
        << "Content-Type: text/plain\r\n";
    if (!keep_alive) {
      oss << "Connection: close\r\n";
    }
    oss << "Content-Length: " << user_agent.length() << "\r\n"
        << "\r\n" 
        << user_agent;
    response_str = oss.str();
  }
  else 
  {
    oss << "HTTP/1.1 404 Not Found\r\n";
    if (!keep_alive) {
      oss << "Connection: close\r\n";
    }
    oss << "Content-Length: 0\r\n\r\n";
    response_str = oss.str();
  }
  
  return response_str;
}

std::string handle_POST_request(const std::string& request_target, const std::string& directory, 
  const std::unordered_map<std::string,std::string>& header_data, 
  const std::string& body, bool keep_alive) 
{
  const std::string file_prefix = "/files/";
  std::ostringstream oss;

  // Check if request is for /files/
  if(request_target.rfind(file_prefix, 0) != 0) 
  {
    oss << "HTTP/1.1 404 Not Found\r\n";
    if (!keep_alive) 
    {
      oss << "Connection: close\r\n";
    }
    oss << "Content-Length: 0\r\n\r\n";
    return oss.str();
  }

  // Extract file name
  std::string file_name = request_target.substr(file_prefix.length());
  if(file_name.empty() || file_name.find("..") != std::string::npos)
  {
    std::cerr << "Empty filename or path traversal attempt\n";
    oss << "HTTP/1.1 400 Bad Request\r\n";
    if (!keep_alive) 
    {
      oss << "Connection: close\r\n";
    }
    oss << "Content-Length: 0\r\n\r\n";
    return oss.str();
  }

  // Check content length
  size_t content_length = 0;
  if(header_data.find("content-length") != header_data.end())
  {
    content_length = std::stoul(header_data.at("content-length"));
  }
  else
  {
    std::cerr << "Content-Length header not found\n";
    oss << "HTTP/1.1 400 Bad Request\r\n";
    if (!keep_alive) 
    {
      oss << "Connection: close\r\n";
    }
    oss << "Content-Length: 0\r\n\r\n";
    return oss.str();
  }

  // Verify body length
  if(body.length() != content_length) 
  {
    std::cerr << "Body length mismatch. Expected: " << content_length
    << ", Received: " << body.length() << "\n";
    oss << "HTTP/1.1 400 Bad Request\r\n";
    if (!keep_alive) 
    {
    oss << "Connection: close\r\n";
    }
    oss << "Content-Length: 0\r\n\r\n";
    return oss.str();
  }

  // Write to file
  std::string full_file_path = directory + "/" + file_name;
  std::ofstream file(full_file_path, std::ios::binary);

  if(file.write(body.data(), body.size())) 
  {
    oss << "HTTP/1.1 201 Created\r\n";
    if (!keep_alive) 
    {
    oss << "Connection: close\r\n";
    }
    oss << "Content-Length: 0\r\n\r\n";
    return oss.str();
  } 
  else 
  {
    std::cerr << "File write failed: " << full_file_path << "\n";
    oss << "HTTP/1.1 500 Internal Server Error\r\n";
    if (!keep_alive) 
    {
    oss << "Connection: close\r\n";
    }
    oss << "Content-Length: 0\r\n\r\n";
    return oss.str();
  }
}

std::pair<std::string, bool> handle_connection(const std::string& request_line, const std::string& headers, const std::string& body, const std::string& directory)
{
  // Parse request line to get method and target
  size_t method_end = request_line.find(' ');
  if (method_end == std::string::npos) 
  {
      return {"HTTP/1.1 400 Bad Request\r\n\r\n", false};
  }
  
  std::string method = request_line.substr(0, method_end);
  std::string request_target = parse_request_target(const_cast<std::string&>(request_line));
  
  if (request_target.empty()) 
  {
      return {"HTTP/1.1 400 Bad Request\r\n\r\n", false};
  }
  
  // Parse headers to check for Connection: close
  std::unordered_map<std::string, std::string> header_data = parse_headers(headers);
  bool keep_alive = true; // Default for HTTP/1.1
  
  auto connection_it = header_data.find("connection");
  if (connection_it != header_data.end()) 
  {
      if (connection_it->second == "close") 
      {
          keep_alive = false;
      }
  }
  
  // Handle request based on method
  std::string response;
  if (method == "GET") {
      response = handle_GET_request(request_target, directory, headers,keep_alive);
  } 
  else if (method == "POST") 
  {
      response = handle_POST_request(request_target, directory, header_data, body,keep_alive);
  } 
  else 
  {
      response = "HTTP/1.1 501 Not Implemented\r\n\r\n";
  }

  return {response, keep_alive};
}

