#include <cstdlib>
#include <iostream>
#include <malloc.h>

#include "http_reader.hh"

using namespace std;

void require_fn( unsigned int line_no, const int condition )
{
  if ( not condition ) {
    throw runtime_error( "test failure at line " + to_string( line_no ) );
  }
}

#define require( C ) require_fn( __LINE__, C )

void* ( *old_malloc_hook )( size_t, const void* );
void* ( *old_realloc_hook )( void*, size_t, const void* );
void* ( *old_memalign_hook )( size_t, size_t, const void* );
void ( *old_free_hook )( void*, const void* );
void allow_malloc();

void* my_malloc_hook( size_t, const void* )
{
  allow_malloc();
  throw runtime_error( "malloc called in forbidden region" );
}

void* my_realloc_hook( void*, size_t, const void* )
{
  allow_malloc();
  throw runtime_error( "realloc called in forbidden region" );
}

void* my_memalign_hook( size_t, size_t, const void* )
{
  allow_malloc();
  throw runtime_error( "memalign called in forbidden region" );
}

void my_free_hook( void*, const void* )
{
  allow_malloc();
  throw runtime_error( "free called in forbidden region" );
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

void save_hooks()
{
  old_malloc_hook = __malloc_hook;
  old_realloc_hook = __realloc_hook;
  old_memalign_hook = __memalign_hook;
  old_free_hook = __free_hook;
}

void allow_malloc()
{
  __malloc_hook = old_malloc_hook;
  __realloc_hook = old_realloc_hook;
  __memalign_hook = old_memalign_hook;
  __free_hook = old_free_hook;
}

void allow_free_only()
{
  __free_hook = old_free_hook;
}

void forbid_malloc()
{
  __malloc_hook = my_malloc_hook;
  __realloc_hook = my_realloc_hook;
  __memalign_hook = my_memalign_hook;
  __free_hook = my_free_hook;
}
#pragma GCC diagnostic pop

void http_test()
{
  string s = "This is a very long string. There will not be any more memory allocations required after this!";

  save_hooks();
  forbid_malloc();

  {
    StringReader sr { move( s ) };
    require( 22 == sr.read( "Mary had a little lamb" ) );
    s = sr.release();
    require( s == "Mary had a little lamb" );
  }

  {
    LengthReader lr { move( s ), 10 };
    require( 10 == lr.read( "Mary had a little lamb" ) );
    s = lr.release();
    require( s == "Mary had a" );
  }

  {
    LineReader<StringReader> lr { move( s ) };
    require( 7 == lr.read( "Hello\r\nGoodbye\r\nYoyo" ) );
    s = lr.release();
    require( s == "Hello" );
  }

  {
    LineReader<StringReader> lr { move( s ) };
    require( 3 == lr.read( "Hel" ) );
    require( 2 == lr.read( "lo" ) );
    require( 1 == lr.read( "\r" ) );
    require( 1 == lr.read( "\n" ) );
    require( 0 == lr.read( "Goodbye" ) );
    s = lr.release();
    require( s == "Hello" );
  }

  {
    LineReader<StringReader> lr { move( s ) };
    require( 3 == lr.read( "Hel" ) );
    require( 3 == lr.read( "lo\r" ) );
    require( 1 == lr.read( "\nGoodbye" ) );
    require( 0 == lr.read( "\n" ) );
    s = lr.release();
    require( s == "Hello" );
  }

  {
    LineReader<StringReader> lr { move( s ) };
    require( 3 == lr.read( "Hel" ) );
    require( 3 == lr.read( "lo\r" ) );
    require( 1 == lr.read( "\nGoodbye" ) );
    require( 0 == lr.read( "\n" ) );
    s = lr.release();
    require( s == "Hello" );
  }

  {
    LineReader<StringReader> lr { move( s ) };
    require( 3 == lr.read( "Hel" ) );
    require( 3 == lr.read( "lo\r" ) );
    require( 21 == lr.read( "\rGoodbye\nHow are you?" ) );
    s = lr.release();
    require( s == "Hello\r\rGoodbye\nHow are you?" );
  }

  {
    ColonReader<StringReader> cr { move( s ) };
    require( 6 == cr.read( "Hello: goodbye" ) );
    s = cr.release();
    require( s == "Hello" );
  }

  {
    IgnoreInitialWhitespaceReader<StringReader> wsr { move( s ) };
    require( 3 == wsr.read( "   " ) );
    require( 10 == wsr.read( "  Hello   " ) );
    require( 9 == wsr.read( "Goodbye  " ) );
    s = wsr.release();
    require( s == "Hello   Goodbye  " );
  }

  {
    IgnoreTrailingWhitespaceReader<StringReader> wsr { move( s ) };
    require( 3 == wsr.read( "   " ) );
    require( 10 == wsr.read( "  Hello   " ) );
    require( 9 == wsr.read( "Goodbye  " ) );
    require( 5 == wsr.read( "  \t  " ) );
    s = wsr.release();
    require( s == "     Hello   Goodbye" );
  }

  {
    IgnoreTrailingWhitespaceReader<StringReader> wsr { move( s ) };
    require( 3 == wsr.read( "   " ) );
    require( 2 == wsr.read( "  " ) );
    s = wsr.release();
    require( s == "" );
  }

  {
    IgnoreSurroundingWhitespaceReader<StringReader> wsr { move( s ) };
    require( 10 == wsr.read( "   Hello  " ) );
    s = wsr.release();
    require( s == "Hello" );
  }

  pair<string, string> s2;
  {
    LineReader<PairReader<ColonReader<StringReader>, IgnoreSurroundingWhitespaceReader<StringReader>>> hline { move(
      s2 ) };
    require( hline.read( " field : Value  \r\ntest" ) == 18 );
    pair<string, string> expected { " field ", "Value" };
    s2 = hline.release();
    require( s2 == expected );
  }

  {
    HTTPHeaderReader reader { move( s2 ) };
    require( not reader.finished() );
    require( reader.read( "x: y \r\ncontent-Length: 12345 \r\nnobody: side\r\n\r\nabc: def\r\n" ) == 47 );
    require( reader.finished() );
    HTTPHeaders headers = reader.release();
    s2 = reader.release_extra_state();
    require( headers.content_length.has_value() );
    require( headers.content_length.value() == 12345 );
  }

  HTTPResponse response;

  {
    HTTPResponseReader reader { false, move( response ), move( s2 ) };
    require( not reader.finished() );
    require( reader.read( "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: 5\r\n\r\nHellothere\r\n" )
             == 68 );
    require( reader.finished() );
    response = reader.release();
    s2 = reader.release_extra_state();
    require( response.http_version == "HTTP/1.1" );
    require( response.status_code == "200" );
    require( response.reason_phrase == "OK" );
    require( response.headers.content_length.has_value() );
    require( response.headers.content_length.value() == 5 );
    require( response.body == "Hello" );
  }

  {
    HTTPResponseReader reader { true, move( response ), move( s2 ) };
    require( not reader.finished() );
    require( reader.read( "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: 7\r\n\r\nHellothere\r\n" )
             == 63 );
    require( reader.finished() );
    response = reader.release();
    s2 = reader.release_extra_state();
    require( response.http_version == "HTTP/1.1" );
    require( response.status_code == "200" );
    require( response.reason_phrase == "OK" );
    require( response.headers.content_length.has_value() );
    require( response.headers.content_length.value() == 7 );
    require( response.body.empty() );
  }

  {
    HTTPResponseReader reader { false, move( response ), move( s2 ) };
    require( not reader.finished() );
    require( reader.read( "HTTP/1.1 200 OK\r" ) == 16 );
    require( reader.read( "\nContent-T" ) == 10 );
    require( reader.read( "ype: text/html\r\nContent-Length" ) == 30 );
    require( reader.read( ":  5  \r\n\r\nHellothere\r\n" ) == 15 );
    require( reader.finished() );
    response = reader.release();
    s2 = reader.release_extra_state();
    require( response.http_version == "HTTP/1.1" );
    require( response.status_code == "200" );
    require( response.reason_phrase == "OK" );
    require( response.headers.content_length.has_value() );
    require( response.headers.content_length.value() == 5 );
    require( response.body == "Hello" );
  }

  {
    HTTPResponseReader reader { false, move( response ), move( s2 ) };
    require( not reader.finished() );
    require( reader.read( "HTTP/1.1 199 INFO\r\nContent-Type: text/html\r\nContent-Length: 7\r\n\r\nHellothere\r\n" )
             == 65 );
    require( reader.finished() );
    response = reader.release();
    s2 = reader.release_extra_state();
    require( response.http_version == "HTTP/1.1" );
    require( response.status_code == "199" );
    require( response.reason_phrase == "INFO" );
    require( response.headers.content_length.has_value() );
    require( response.headers.content_length.value() == 7 );
    require( response.body.empty() );
  }

  allow_free_only();
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
