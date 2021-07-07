#include "awsclient.hh"
#include "certificates.hh"
#include "timer.hh"
#include "awsv4_sig.hh"
#include <iostream>
#include "file_descriptor.hh"

using namespace std;

AWSCredentials::AWSCredentials( const std::string & access_key,
                const std::string & secret_key,
                const std::string & session_token)
                :access_key_(access_key)
                ,secret_key_(secret_key)
                ,session_token_(session_token)
{}

AWSClient::AWSClient( const string & service, const string& region, const AWSCredentials & creds )
  : service_(service)
  , region_(region)
  , endpoint_hostname_( service + "." + region + ".amazonaws.com" )
  , endpoint_( [&] {
    GlobalScopeTimer<Timer::Category::DNS> timer;
    return Address { endpoint_hostname_, "https" };
  }() )
  , ssl_context_( [&] {
    SSLContext ret;
    //ret.trust_certificate( aws_root_ca_1 );
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
    [&] { ssl_session_.do_read(); },
    [&] { return ssl_session_.want_read(); } ) );

  rules_.push_back( event_loop.add_rule(
    "SSL write",
    ssl_session_.socket(),
    Direction::Out,
    [&] { ssl_session_.do_write(); },
    [&] { return ssl_session_.want_write(); } ) );

  rules_.push_back( event_loop.add_rule(
    "HTTP write",
    [&] { http_.write( ssl_session_.outbound_plaintext() ); },
    [&] {
      return ( not ssl_session_.outbound_plaintext().writable_region().empty() ) and ( not http_.requests_empty() );
    } ) );

  rules_.push_back( event_loop.add_rule(
    "HTTP read",
    [&] {
      if ( http_.read( ssl_session_.inbound_plaintext(), response_ ) ) {
        cerr << "Response received: " << response_.http_version << " " << response_.status_code << " "
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

AWSLambdaClient::AWSLambdaClient(const std::string &region, const AWSCredentials &creds)
:AWSClient("lambda",region,creds)
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

void AWSLambdaClient::invoke_function(const std::string & function, const std::string & qualifier)
{
    HTTPHeaders headers { {}, {}};
    headers.headers["host"] = endpoint_hostname_;
    headers.headers["x-amz-invocation-type"] = "RequestResponse";
    headers.headers["x-amz-log-type"] = "Tail";
    cout << "invoking lambda function " << function << " " << qualifier << endl;
    auto line = (string)"/2015-03-31/functions/" + function + (string)"/invocations";
    cout << line << endl;
    AWSv4Sig::sign_request( (string)"POST\n" + line,
                            creds_.secret_key(), creds_.access_key(),
                            region_, service_, {}, headers );
    cout << headers.headers["host"] << " auth: " << headers.headers["authorization"] << " date: " << headers.headers["x-amz-date"] << endl;
    http_.push_request(
            { "POST", line, "HTTP/1.1", headers , {} } );

}

AWSS3Client::AWSS3Client(const std::string &region, const AWSCredentials &creds)
        :AWSClient("dpc-h-1gb-marsupialtail","s3." + region,creds)
{}

void AWSS3Client::download_file(const std::string & bucket, const std::string & object,
                                const std::string & filename) {

    const string endpoint = bucket + "." + region_ + ".amazonaws.com";
    cout << endpoint << endl;
    HTTPHeaders headers { {}, {}};
    headers.headers["host"] = endpoint;
    auto line = bucket;
    cout << object << " " << filename << endl;
    AWSv4Sig::sign_request( (string)"POST\n" + line,
                            creds_.secret_key(), creds_.access_key(),
                            region_, service_, {}, headers );

    http_.push_request(
            { "POST", line, "HTTP/1.1", headers , {} } );

//    FileDescriptor file { CheckSystemCall( "open",
//                                           open( filename.string().c_str(), O_RDWR | O_TRUNC | O_CREAT,
//                                                 S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH ) ) };
//
//    while ( responses.empty() ) {
//        responses.parse( s3.read() );
//    }
//
//    if ( responses.front().first_line() != "HTTP/1.1 200 OK" ) {
//        throw runtime_error( "HTTP failure in S3Client::download_file( " + bucket + ", " + object + " ): " + responses.front().first_line() );
//    }
//    else {
//        file.write( responses.front().body(), true );
//    }

}