#pragma once

#include <string_view>

#include "address.hh"
#include "eventloop.hh"
#include "http_client.hh"
#include "secure_socket.hh"


class AWSCredentials
{
private:
    std::string access_key_;
    std::string secret_key_;
    std::optional<std::string> session_token_ {};

public:
    AWSCredentials( const std::string & access_key,
                    const std::string & secret_key,
                    const std::string & session_token = {} );

    const std::string & access_key() const { return access_key_; }
    const std::string & secret_key() const { return secret_key_; }
    const std::optional<std::string> & session_token() const { return session_token_; }
};

class AWSClient
{
protected:
  std::string service_;
  std::string region_;
  std::string endpoint_hostname_;
  Address endpoint_;
  SSLContext ssl_context_;
  SSLSession ssl_session_;
  HTTPClient http_;
  HTTPResponse response_;
  AWSCredentials creds_;
  std::vector<EventLoop::RuleHandle> rules_ {};

public:
  AWSClient( const std::string & service, const std::string& region, const AWSCredentials & creds  );
  void install_rules( EventLoop& event_loop );
  void print_response();
};

class AWSLambdaClient: public AWSClient
{
public:
    AWSLambdaClient(const std::string & region, const AWSCredentials & creds);
    void invoke_function(const std::string & function, const std::string & qualifier = "");
    void get_account_settings();
};

class AWSS3Client: public AWSClient
{
public:
    AWSS3Client(const std::string & region, const AWSCredentials & creds);
    void download_file(const std::string & bucket, const std::string & object,
                       const std::string & filename);
};