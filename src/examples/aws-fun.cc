#include <iostream>
#include <string>

#include "awsclient.hh"
#include "eventloop.hh"
#include "timerfd.hh"

using namespace std;

void program_body(char * argv[])
{
  ios::sync_with_stdio( false );

  AWSCredentials creds {std::getenv("AWS_ACCESS_KEY"), std::getenv("AWS_SECRET_KEY")};
    EventLoop event_loop;
    std::list<AWSLambdaClient> awses;

    bool terminated = false;
    TimerFD termination_timer { chrono::seconds { 60} };



    event_loop.add_rule(
      "termination",
      termination_timer,
      Direction::In,
      [&] { terminated = true; },
      [&] { return not terminated; } );

    Address address {"lambda.us-west-2.amazonaws.com" , "https" }; 

    for(int i = 0; i < atoi(argv[1]); i ++)
    {
        awses.emplace_back("us-west-2" ,creds,address);
        awses.back().install_rules(event_loop);
        
        awses.back().invoke_function("generic",{"54.187.85.209", "14007", to_string(i+1), "1", "8", "x3"});
    }

//  aws.print_response();
// //aws.get_account_settings();

//   aws.invoke_function("tcp-B");
//   aws.invoke_function("tcp-C");

//    AWSLambdaClient aws2 { "us-west-2" ,creds};
//    aws2.install_rules( event_loop );
    //aws.invoke_function("tcp-B");
    //AWSS3Client aws { "us-west-2" ,creds};
    //aws.install_rules( event_loop );
   // aws.download_file("dpc-h-1gb-marsupialtail","/nation/nation.tbl","customer.csv");

    while ( event_loop.wait_next_event( -1 ) != EventLoop::Result::Exit and not terminated ) {
  }

    //aws.read_downloaded_file();

  cout << event_loop.summary() << "\n";
}

int main(int argc, char * argv[])
{
  cout << argc << endl;
  try {
    program_body(argv);
    cout << global_timer().summary() << "\n";
  } catch ( const exception& e ) {
    cout << "Exception: " << e.what() << endl;
    cout << global_timer().summary() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
