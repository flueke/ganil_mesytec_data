#include "mesytec_narval_receiver.h"
#include <iostream>
#include <chrono>
#include <thread>
using namespace std::chrono_literals;

int main(int argc, char* argv[])
{
   // arg[1]= host:port

   // pass URL:PORT as argument
   std::string zmq_port{"tcp://"};
   zmq_port.append(argv[1]);

   unsigned int error_code;

   process_config((char*)zmq_port.c_str(),&error_code);

   if(error_code!=0) std::cerr << "error in process_config" << std::endl;

   auto algo_dat = process_register(&error_code);

   if(error_code!=0) std::cerr << "error in process_register" << std::endl;

   process_initialise(algo_dat, &error_code);

   if(error_code!=0) std::cerr << "error in process_initialise" << std::endl;

   process_start(algo_dat, &error_code);

   if(error_code!=0) std::cerr << "error in process_start" << std::endl;

   const int bufsize = 400000;
   unsigned char buffer[bufsize];
   auto mesytec_setup = mesytec::define_setup
         (
            {
               {"MDPP-16", 0x0, 16, mesytec::SCP},
               {"MDPP-32", 0x10, 32, mesytec::SCP}
            }
            );
   mesytec::mesytec_buffer_reader readBuf{mesytec_setup};

   while(1)
   {
      unsigned int buff_used=0;
      process_block(algo_dat,
                    buffer,
                    bufsize,
                    &buff_used,
                    &error_code);

      if(error_code!=0){
         std::cerr << "error in process_block" << std::endl;
         break;
      }
      else std::cout << "Used " << buff_used << " bytes of output buffer size " << bufsize << " bytes";

      auto tot_num = readBuf.read_buffer(
               (const uint8_t*)buffer, buff_used, [](mesytec::mdpp_event&){ }
      );
      std::cout << " : containing " << tot_num << " events\n";

      std::this_thread::sleep_for(1000ms);
   }
}
