#ifndef ZMQ_COMPAT_H
#define ZMQ_COMPAT_H

// Handle deprecated methods in cppzmq

#include <zmq.hpp>

// From CPPZMQ_VERSION 4.7.0 onwards:
//    warning: ‘void zmq::detail::socket_base::setsockopt(int, const void*, size_t)’ is deprecated: from 4.7.0, use `set` taking option from zmq::sockopt
#if CPPZMQ_VERSION >= ZMQ_MAKE_VERSION(4,7,0)
#define ZMQ_SETSOCKOPT_DEPRECATED
#endif

// From CPPZMQ_VERSION 4.3.1 onwards:
//    warning: ‘bool zmq::detail::socket_base::send(zmq::message_t&, int)’ is deprecated: from 4.3.1, use send taking message_t and send_flags
#if CPPZMQ_VERSION >= ZMQ_MAKE_VERSION(4,3,1)
#define ZMQ_USE_SEND_FLAGS
#endif

// From CPPZMQ_VERSION 4.3.1 onwards:
//    warning: ‘bool zmq::detail::socket_base::recv(zmq::message_t*, int)’ is deprecated: from 4.3.1, use recv taking a reference to message_t and recv_flags
#if CPPZMQ_VERSION >= ZMQ_MAKE_VERSION(4,3,1)
#define ZMQ_USE_RECV_WITH_REFERENCE
#endif

#endif // ZMQ_COMPAT_H
