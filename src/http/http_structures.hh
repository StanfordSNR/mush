#pragma once

#include <optional>
#include <string>
#include <map>
struct HTTPHeaders
{
  std::optional<size_t> content_length {};
//  std::string host {};
//  std::string authorization {};
//  std::string x_amz_date {};
  std::map<std::string, std::string> headers{};
  bool connection_close {};
};

struct HTTPRequest
{
  std::string method {}, request_target {}, http_version {};
  HTTPHeaders headers {};
  std::string body {};
};

struct HTTPResponse
{
  std::string http_version {}, status_code {}, reason_phrase {};
  HTTPHeaders headers {};
  std::string body {};
};
