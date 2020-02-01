#include "awsclient.hh"
#include "certificates.hh"
#include "timer.hh"

#include <iostream>

using namespace std;

AWSClient::AWSClient( const string& region )
  : endpoint_hostname_( "lambda." + region + ".amazonaws.com" )
  , endpoint_( [&] {
    GlobalScopeTimer<Timer::Category::DNS> timer;
    return Address { endpoint_hostname_, "https" };
  }() )
  , ssl_context_( [&] {
    SSLContext ret;
    ret.trust_certificate( aws_root_ca_1 );
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
{}

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
             << response_.reason_phrase << response_.body << "\n";
      }
    },
    [&] { return not ssl_session_.inbound_plaintext().readable_region().empty(); } ) );
}

void AWSClient::get_account_settings()
{
  http_.push_request( { "GET", "/2016-08-19/account-settings/ HTTP/1.1", "HTTP/1.1", { {}, endpoint_hostname_ }, {} } );
}
