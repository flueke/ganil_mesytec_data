#include "mesytec_buffer_reader.h"
#include "zmq.hpp"
#include <chrono>
#include <thread>
#include <ctime>
using namespace std::literals::chrono_literals;

int main(int argc, char*argv[])
{
   // pass URL:PORT as argument
   std::string zmq_port{"tcp://"};
   zmq_port.append(argv[1]);

   zmq::context_t context(1);	// for ZeroMQ communications

   zmq::socket_t* pub;
   try {
      pub = new zmq::socket_t(context, ZMQ_SUB);
   } catch (zmq::error_t &e) {
      std::cout << "ERROR: " << "process_start: failed to start ZeroMQ event spy: " << e.what () << std::endl;
   }

   int timeout=500;//milliseconds
   pub->setsockopt(ZMQ_RCVTIMEO, &timeout, sizeof(int));
   try {
      pub->connect(zmq_port.c_str());
   } catch (zmq::error_t &e) {
      std::cout << "ERROR" << "process_start: failed to bind ZeroMQ endpoint " << zmq_port << ": " << e.what () << std::endl;
   }
   std::cout << "Connected to MESYTEC-Spy " << zmq_port << std::endl;
   pub->setsockopt(ZMQ_SUBSCRIBE, "", 0);

   zmq::message_t event;

   auto mesytec_setup = mesytec::define_setup
         (
            {
               {"MDPP-16", 0x0, 16, mesytec::SCP},
               {"MDPP-32", 0x10, 32, mesytec::SCP}
            }
            );

   mesytec::buffer_reader readBuf{mesytec_setup};

   while(1)
   {
      try{
         if(!pub->recv(event))
         {
            std::cout << "Got no event from ZMQ : ";
            std::time_t result = std::time(nullptr);
            std::cout << std::asctime(std::localtime(&result)) << std::endl;
            std::this_thread::sleep_for(1000ms);
         }
      }
      catch(zmq::error_t &e) {
         std::cout << "timeout on ZeroMQ endpoint : " << e.what () << std::endl;
         return 1;
      }

      readBuf.read_buffer( (const uint8_t*)event.data(), event.size(), [](mesytec::mdpp_event& Event){ Event.ls(); });
   }
}
