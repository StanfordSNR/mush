#pragma once

#include "file_descriptor.hh"

#include <functional>
#include <list>
#include <string_view>

#include <poll.h>

//! Waits for events on file descriptors and executes corresponding callbacks.
class EventLoop
{
public:
  //! Indicates interest in reading (In) or writing (Out) a polled fd.
  enum class Direction : short
  {
    In = POLLIN,  //!< Callback will be triggered when Rule::fd is readable.
    Out = POLLOUT //!< Callback will be triggered when Rule::fd is writable.
  };

private:
  using CallbackT = std::function<void( void )>;
  using InterestT = std::function<bool( void )>;

  struct FDRule
  {
    std::string name;
    FileDescriptor fd;   //!< FileDescriptor to monitor for activity.
    Direction direction; //!< Direction::In for reading from fd, Direction::Out for writing to fd.
    CallbackT callback;  //!< A callback that reads or writes fd.
    InterestT interest;  //!< A callback that returns `true` whenever fd should be polled.
    CallbackT cancel;    //!< A callback that is called when the rule is cancelled (e.g. on hangup)

    //! Returns the number of times fd has been read or written, depending on the value of Rule::direction.
    //! \details This function is used internally by EventLoop; you will not need to call it
    unsigned int service_count() const;
  };

  struct NonFDRule
  {
    std::string name;
    CallbackT callback;
    InterestT interest;
  };

  std::list<FDRule> _fd_rules {};
  std::list<NonFDRule> _non_fd_rules {};

public:
  //! Returned by each call to EventLoop::wait_next_event.
  enum class Result
  {
    Success, //!< At least one Rule was triggered.
    Timeout, //!< No rules were triggered before timeout.
    Exit //!< All rules have been canceled or were uninterested; make no further calls to EventLoop::wait_next_event.
  };

  //! Add a rule whose callback will be called when `fd` is ready in the specified Direction.
  void add_rule(
    const std::string& name,
    const FileDescriptor& fd,
    const Direction direction,
    const CallbackT& callback,
    const InterestT& interest = [] { return true; },
    const CallbackT& cancel = [] {} );

  void add_rule(
    const std::string& name,
    const CallbackT& callback,
    const InterestT& interest = [] { return true; } );

  //! Calls [poll(2)](\ref man2::poll) and then executes callback for each ready fd.
  Result wait_next_event( const int timeout_ms );
};

using Direction = EventLoop::Direction;
