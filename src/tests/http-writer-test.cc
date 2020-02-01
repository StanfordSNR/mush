#include <algorithm>
#include <cstdlib>
#include <iostream>

#include "http_writer.hh"
#include "random.hh"
#include "ring_buffer.hh"

using namespace std;

void require_fn( unsigned int line_no, const int condition )
{
  if ( not condition ) {
    throw runtime_error( "test failure at line " + to_string( line_no ) );
  }
}

#define require( C ) require_fn( __LINE__, C )

void copy_some_chars( mt19937& rng, RingBuffer& rb, string& str )
{
  uniform_int_distribution<> dist_ { 0, 19 };
  const size_t num_bytes = dist_( rng );
  const string_view buffer = rb.readable_region().substr( 0, num_bytes );
  str.append( buffer );
  rb.pop( buffer.size() );
}

HTTPRequest make_random_request( mt19937& rng )
{
  HTTPRequest req;
  req.method = "POST";
  req.request_target = "/hello.html";
  req.http_version = "HTTP/1.1";
  req.headers.content_length.emplace( 50 );
  req.headers.host = "www.stanford.edu";

  uniform_int_distribution<> dist_ { 0, 255 };
  req.body = string( 200123, '\0' );

  const auto randchar = [&]() { return static_cast<char>( dist_( rng ) ); };

  generate( req.body.begin(), req.body.end(), randchar );
  return req;
}

void http_test()
{
  mt19937 rng = get_random_generator();

  cerr << "Making random request... ";
  const HTTPRequest request = make_random_request( rng );
  cerr << "done.\n";

  cerr << "Making serialized gold standard... ";
  const string serialized_gold_standard = [&]() {
    RingBuffer rb { 1048576 * 20 };
    HTTPRequestWriter writer( HTTPRequest { request } );
    writer.write_to( rb );
    require( writer.finished() );
    return string( rb.readable_region() );
  }();

  cerr << "done.\n";

  {
    cerr << "Test 1... ";
    RingBuffer rb { 4096 };
    HTTPRequestWriter writer( HTTPRequest { request } );
    string serialized;
    while ( not writer.finished() ) {
      writer.write_to( rb );
      copy_some_chars( rng, rb, serialized );
    }
    require( writer.finished() );
    while ( not rb.readable_region().empty() ) {
      copy_some_chars( rng, rb, serialized );
    }
    require( rb.readable_region().empty() );
    require( serialized == serialized_gold_standard );
    cerr << "done.\n";
  }

  {
    cerr << "Test 2... ";
    RingBuffer rb { 12288 };
    HTTPRequestWriter writer( HTTPRequest { request } );
    string serialized;
    while ( not writer.finished() ) {
      writer.write_to( rb );
      copy_some_chars( rng, rb, serialized );
    }
    require( writer.finished() );
    while ( not rb.readable_region().empty() ) {
      copy_some_chars( rng, rb, serialized );
    }
    require( rb.readable_region().empty() );
    require( serialized == serialized_gold_standard );
    cerr << "done.\n";
  }

  {
    cerr << "Test 3... ";
    RingBuffer rb { 12288 };
    HTTPRequestWriter writer( HTTPRequest { request } );
    writer.write_to( rb );
    require( not writer.finished() );
    require( rb.readable_region() == serialized_gold_standard.substr( 0, 12288 ) );
    cerr << "done.\n";
  }
}

int main()
{
  try {
    http_test();
  } catch ( const exception& e ) {
    cerr << "Exception: " << e.what() << endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
