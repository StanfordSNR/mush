#include "awsclient.hh"
#include "certificates.hh"
#include "timer.hh"
#include "awsv4_sig.hh"
#include <iostream>
#include "file_descriptor.hh"
#include <string>
using namespace std;

AWSCredentials::AWSCredentials( const std::string & access_key,
                const std::string & secret_key,
                const std::string & session_token)
                :access_key_(access_key)
                ,secret_key_(secret_key)
                ,session_token_(session_token)
{}

AWSClient::AWSClient( const string & service, const string& region, const AWSCredentials & creds , const Address & address )
  : service_(service)
  , region_(region)
  , endpoint_hostname_( service + "." + region + ".amazonaws.com" )
  , endpoint_( address )
  , ssl_context_( [&] {
    SSLContext ret;
    ret.trust_certificate( aws_root_ca_1 );
    ret.trust_certificate( digicert);


    return ret;
  }() )
  , ssl_session_( [&] {
    TCPSocket tcp_sock;
    tcp_sock.set_blocking( false );
    tcp_sock.connect( endpoint_ );
    return SSLSession { ssl_context_.make_SSL_handle(), move( tcp_sock ), endpoint_hostname_ };
  }() )
  , http_()
  , response_()
  , creds_ (creds.access_key(),creds.secret_key())
{cout << endpoint_hostname_ << endl;}

void AWSClient::install_rules( EventLoop& event_loop )
{
  rules_.push_back( event_loop.add_rule(
    "SSL read",
    ssl_session_.socket(),
    Direction::In,
    [&] { //cout << "doing read" << endl;
    ssl_session_.do_read(); },
    [&] { return ssl_session_.want_read(); } ) );

  rules_.push_back( event_loop.add_rule(
    "SSL write",
    ssl_session_.socket(),
    Direction::Out,
    [&] { //cout << "doing write" << endl;
        ssl_session_.do_write(); },
    [&] { return ssl_session_.want_write(); } ) );

 /* rules_.push_back( event_loop.add_rule(
        "HTTP write",
        [&] { RingBuffer a; http_.write(a); a.write_to(tcp_sock)},
        [&] {
            return ( not ssl_session_.outbound_plaintext().writable_region().empty() ) and ( not http_.requests_empty() );
        } ) );
*/
  rules_.push_back( event_loop.add_rule(
    "HTTPS write",
    [&] {//cout << "doing https write" << endl;
    http_.write( ssl_session_.outbound_plaintext() ); },
    [&] {
      return ( not ssl_session_.outbound_plaintext().writable_region().empty() ) and ( not http_.requests_empty() );
    } ) );

  rules_.push_back( event_loop.add_rule(
    "HTTPS read",
    [&] {//cout << "doing https read" << endl;
        if ( http_.read( ssl_session_.inbound_plaintext(), response_ ) ) {
        cout << "Response received: " << response_.http_version << " " << response_.status_code << " "
             << response_.reason_phrase << "\n"
             << response_.body << "\n";
      }
    },
    [&] { return not ssl_session_.inbound_plaintext().readable_region().empty(); } ) );
}

void AWSClient::print_response() {
    cout << "printing response" << endl;

    cout << response_.http_version << " " << response_.reason_phrase << " " << response_.body << endl;
}

AWSLambdaClient::AWSLambdaClient(const std::string &region, const AWSCredentials &creds, const Address & address )
:AWSClient("lambda",region,creds, address )
{}

void AWSLambdaClient::get_account_settings()
{
    HTTPHeaders headers { {}, {}};
    headers.headers["host"] = endpoint_hostname_;

    AWSv4Sig::sign_request( "GET\n/2016-08-19/account-settings/",
                            creds_.secret_key(), creds_.access_key(),
                            region_, service_, {}, headers );
    cout << headers.headers["host"] << " auth: " << headers.headers["authorization"] << " date: " << headers.headers["x-amz-date"] << endl;
    http_.push_request(
            { "GET", "/2016-08-19/account-settings/", "HTTP/1.1", headers , {} } );

}

void AWSLambdaClient::invoke_function(const std::string & function, std::vector<std::string> arguments)
{
    HTTPHeaders headers { {}, {}};
    headers.headers["host"] = endpoint_hostname_;
    headers.headers["x-amz-invocation-type"] = "RequestResponse";
    headers.headers["x-amz-log-type"] = "Tail";
    std::string payload = "{";
    for(size_t i = 0; i < arguments.size(); i ++)
    {
      payload += "\"arg" + to_string(i) + "\":\"" + arguments[i] + ((i == arguments.size() - 1) ? "\"":"\",");
    }
    payload += "}";
    std::cout << payload << std::endl;
    headers.headers["content-length"] = to_string(payload.length());
    cout << "invoking lambda function " << function << endl;
    auto line = (string)"/2015-03-31/functions/" + function + (string)"/invocations";
    cout << line << endl;
    AWSv4Sig::sign_request( (string)"POST\n" + line,
                            creds_.secret_key(), creds_.access_key(),
                            region_, service_, payload, headers );
    cout << headers.headers["host"] << " auth: " << headers.headers["authorization"] << " date: " << headers.headers["x-amz-date"] << endl;
    http_.push_request(
            { "POST", line, "HTTP/1.1", headers , payload } );
//    http_.push_request(
//            { "POST", line, "HTTP/1.1", headers , {} } );

}

AWSS3Client::AWSS3Client(const std::string &region, const AWSCredentials &creds, const Address & address )
        :AWSClient("dpc-h-1gb-marsupialtail","s3." + region,creds, address)
{}

void AWSS3Client::download_file(const std::string & bucket, const std::string & object,
                                const std::string & filename) {


    const string endpoint = bucket + "." + region_ + ".amazonaws.com";

    cout << bucket << " " << endpoint << " " << object <<  endl;
    HTTPHeaders headers { {}, {}};
    headers.headers["host"] = endpoint;
    auto line = object;
    cout << object << " " << filename << endl;
    AWSv4Sig::sign_request( (string)"GET\n" + line,
                            creds_.secret_key(), creds_.access_key(),
                            "us-west-2", "s3", {}, headers );
    for(auto hd : headers.headers)
    {
        cout << hd.first << " " << hd.second << endl;
    }

    http_.push_request(
            { "GET", line, "HTTP/1.1", headers , {} } );
//    http_.write( ssl_session_.outbound_plaintext() );
//    while (not ssl_session_.want_write()) {}
//    ssl_session_.do_write();
//    while ( ssl_session_.inbound_plaintext().readable_region().empty() ){cout << "waiting " << endl;}
//
//    if ( http_.read( ssl_session_.inbound_plaintext(), response_ ) ) {
//        cerr << "Response received: " << response_.http_version << " " << response_.status_code << " "
//             << response_.reason_phrase << "\n"
//             << response_.body << "\n";
//    }

//    FileDescriptor file { CheckSystemCall( "open",
//                                           open( filename.string().c_str(), O_RDWR | O_TRUNC | O_CREAT,
//                                                 S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH ) ) };
//

//
//    if ( responses.front().first_line() != "HTTP/1.1 200 OK" ) {
//        throw runtime_error( "HTTP failure in S3Client::download_file( " + bucket + ", " + object + " ): " + responses.front().first_line() );
//    }
//    else {
//        file.write( responses.front().body(), true );
//    }

}

void AWSS3Client::read_downloaded_file()
{
    cout << response_.body << endl;
}