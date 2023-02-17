#include <iostream>
#include <chrono>
#include <thread>
#include <fstream>
//using namespace std::chrono_literals;
#include "../narval/zmq_compat.h"
#include "mesytec_buffer_reader.h"
zmq::context_t context(1);	// for ZeroMQ communications

int main()
{
   std::string path_to_setup = "/home/eindra/ganacq_manip/e818";
   std::string zmq_port = "tcp://mesytecPC:5575";

   mesytec::buffer_reader MESYbuf;
   MESYbuf.read_crate_map(path_to_setup + "/crate_map.dat");

   zmq::socket_t *pub;
   try {
      pub = new zmq::socket_t(context, ZMQ_SUB);
   } catch (zmq::error_t &e) {
      std::cout << "[MESYTEC] : ERROR: " << "process_start: failed to start ZeroMQ event spy: " << e.what () << std::endl;
   }

   int timeout=100;//milliseconds
#ifdef ZMQ_SETSOCKOPT_DEPRECATED
   pub->set(zmq::sockopt::rcvtimeo,timeout);
#else
   pub->setsockopt(ZMQ_RCVTIMEO, &timeout, sizeof(int));
#endif
   try {
      pub->connect(zmq_port.c_str());
   } catch (zmq::error_t &e) {
      std::cout << "[MESYTEC] : ERROR" << "process_start: failed to bind ZeroMQ endpoint " << zmq_port << ": " << e.what () << std::endl;
   }
   std::cout << "[MESYTEC] : Connected to MESYTECSpy " << zmq_port << std::endl;
#ifdef ZMQ_SETSOCKOPT_DEPRECATED
   pub->set(zmq::sockopt::subscribe,"");
#else
   pub->setsockopt(ZMQ_SUBSCRIBE, "", 0);
#endif

   zmq::message_t event;

   while(1)
   {
#ifdef ZMQ_USE_RECV_WITH_REFERENCE
      if(pub->recv(event))
#else
      if(pub->recv(&event)) ;
#endif
      {
         std::cout << "BUFFER SIZE = " << std::dec << event.size() << " BYTES" << std::endl;
         MESYbuf.dump_data_stream((const uint8_t*)event.data(), event.size());
      }
   }

   pub->close();

   return 0;
}
