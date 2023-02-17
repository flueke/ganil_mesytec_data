#include "mesytec_buffer_reader.h"
#include "../narval/zmq_compat.h"
#include <chrono>
#include <thread>
#include <ctime>
zmq::context_t context;	// for ZeroMQ communications

int main()
{
   std::string path_to_setup = "/home/eindra/ganacq_manip/e818_test_indra";
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
#if defined (ZMQ_CPP14)
      if(pub->recv(event))
#else
      if(pub->recv(&event))
#endif
      {
         MESYbuf.read_buffer_collate_events( (const uint8_t*)event.data(), event.size(),
                                             [](mesytec::event& Event,mesytec::experimental_setup& mesytec_setup){ Event.ls(mesytec_setup); });
      }
   }
}
