#include <iostream>
#include <string>

#include "awsclient.hh"
#include "eventloop.hh"
#include "timer.hh"

using namespace std;

void program_body()
{
  ios::sync_with_stdio( false );

  AWSCredentials creds {(string)key1,(string)keyw};
    EventLoop event_loop;
   AWSLambdaClient aws { "us-west-2" ,creds};
//
//
  aws.install_rules( event_loop );
//  aws.print_response();
// //aws.get_account_settings();
  aws.invoke_function("test");

//    AWSS3Client aws { "us-west-2" ,creds};
//    aws.install_rules( event_loop );
//    aws.download_file("dpc-h-1gb-marsupialtail","/customer/customer.tbl","customer.csv");

    while ( event_loop.wait_next_event( -1 ) != EventLoop::Result::Exit ) {
  }

  cout << event_loop.summary() << "\n";
}

int main()
{
  try {
    program_body();
    cout << global_timer().summary() << "\n";
  } catch ( const exception& e ) {
    cout << "Exception: " << e.what() << endl;
    cout << global_timer().summary() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
