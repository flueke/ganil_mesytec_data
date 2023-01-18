#include "mesytec_buffer_reader.h"
#include "mesytec_experimental_setup.h"
#include <string>
#include "zmq.hpp"
#include <ctime>
#include <thread>
#include <chrono>
#include "boost/program_options.hpp"

unsigned char mfmevent[0x400000]; // 4 MB buffer
zmq::context_t context(1);	// for ZeroMQ communications

struct mesytec_mfm_converter
{
   zmq::socket_t* pub;
   //std::string zmq_spy_port = "tcp://*:9097";
   std::string zmq_spy_port = "tcp://*:";
   std::string spytype = "ZMQ_PUB";

   mesytec_mfm_converter(int port)
   {
      zmq_spy_port = zmq_spy_port + std::to_string(port);
      try {
         pub=new zmq::socket_t(context, ZMQ_PUB);
         int linger = 0;
         pub->setsockopt(ZMQ_LINGER, &linger, sizeof(linger));   // linger equal to 0 for a fast socket shutdown
      } catch (zmq::error_t &e) {
         std::cout << "ERROR: " << "process_initialise: failed to start " << spytype << " event spy: " << e.what () << std::endl;
      }
      try {
         pub->bind(zmq_spy_port.c_str());
      } catch (zmq::error_t &e) {
           std::cout << "ERROR" << "process_start: failed to bind " << spytype << " endpoint " << zmq_spy_port << ": " << e.what () << std::endl;
      }

   }

   mesytec_mfm_converter(const mesytec_mfm_converter&) = default;

   void shutdown()
   {
      std::cout << "Shutting down transmitter" << std::endl;
      pub->close();
      delete pub;
   }

   void operator()(mesytec::event &mesy_event)
   {
      // called for each complete event parsed from the mesytec stream
      //
      // this builds an MFMFrame for each event and copies it into the output stream,
      // unless there is no room left in the buffer, in which case it will be treated
      // the next time that process_block is called

      // 24 bytes for MFM header, plus the Mesytec data buffer
      size_t mfmeventsize = 24 + mesy_event.size_of_buffer()*4;

      ///////////////////MFM FRAME CONVERSION////////////////////////////////////
      mfmevent[0] = 0xc1;  // little-endian, blob frame, unit block size 2 bytes (?)
      *((uint32_t*)(&mfmevent[1])) = (uint32_t)mfmeventsize/2;// frameSize in unit block size
      mfmevent[4] = 0x0;  // dataSource
      *((uint16_t*)(&mfmevent[5])) = mesytec::mfm_frame_type; // frame type
      mfmevent[7] = 0x1; // frame revision 1

      // next 6 bytes [8]-[13] are for the timestamp
      *((uint16_t*)(&mfmevent[8])) = mesy_event.tgv_ts_lo;
      *((uint16_t*)(&mfmevent[10])) = mesy_event.tgv_ts_mid;
      *((uint16_t*)(&mfmevent[12])) = mesy_event.tgv_ts_hi;
      //printf("mfmframe: ts_lo %#06x  ts_mid %#06x  ts_hi %#06x\n",*((uint16_t*)(&mfmevent[8])),*((uint16_t*)(&mfmevent[10])),*((uint16_t*)(&mfmevent[12])));

      // bytes [14]-[17]: event number (event counter from mesytec EOE)
      *((uint32_t*)(&mfmevent[14])) = mesy_event.event_counter;
      // bytes [20]-[23] number of bytes in mesytec data blob
      *((uint32_t*)(&mfmevent[20])) = (uint32_t)mesy_event.size_of_buffer()*4;

      // copy mesytec data into mfm frame 'blob'
      memcpy(mfmevent+24, mesy_event.get_output_buffer().data(), mfmeventsize-24);
      ///////////////////MFM FRAME CONVERSION////////////////////////////////////

      // Now send frame on ZMQ socket
      zmq::message_t msg(mfmeventsize);
      memcpy(msg.data(), mfmevent, mfmeventsize);
      pub->send(msg);
   }
};

namespace po = boost::program_options;

int main(int argc, char *argv[])
{
   po::options_description desc("\nmesytec_receiver_mfm_transmitter\n\nUsage");

   desc.add_options()
         ("help", "produce this message")
         ("config_dir", po::value<std::string>(),  "directory with crate_map.dat and detector_correspondence.dat files")
         ("mvme_host", po::value<std::string>(), "url of host where mvme-zmq is runnning")
         ("mvme_port", po::value<int>(), "[option] port number of mvme-zmq host (default: 5575)")
         ("zmq_port", po::value<int>(), "[option] port on which to publish MFM data (default: 9097)");

   po::variables_map vm;
   try
   {
      po::store(po::parse_command_line(argc, argv, desc), vm);
      po::notify(vm);
   }
   catch(...)
   {
      // in case of unknown options, print help & exit
      std::cout << desc << "\n";
      return 0;
   }

   if (vm.count("help")) {
      std::cout << desc << "\n";
      return 0;
   }

   if(!vm.count("config_dir")||!vm.count("mvme_host"))
   {
      std::cout << desc << "\n";
      return 0;
   }

   //std::string path_to_setup = "/shareacq/eindra/ganacq_manip/e818";
   std::string path_to_setup = vm["config_dir"].as<std::string>();

   //std::string zmq_port = "tcp://mesytecPC:5575";
   std::string zmq_port = "tcp://";
   std::string path_to_host = vm["mvme_host"].as<std::string>();
   int host_port = 5575;
   if(vm.count("mvme_port")) host_port = vm["mvme_port"].as<int>();
   zmq_port = zmq_port + path_to_host + ":" + std::to_string(host_port);

   int spy_port = 9097;
   if(vm.count("zmq_port")) spy_port = vm["zmq_port"].as<int>();

   printf ("[MESYTEC] : MESYTECSpy port = %s\n",zmq_port.c_str());
   printf ("[MESYTEC] :  - will read crate map in = %s/crate_map.dat\n", path_to_setup.c_str());

   // read crate map
   mesytec::experimental_setup mesytec_setup;
   mesytec_setup.read_crate_map(path_to_setup + "/crate_map.dat");
   mesytec_setup.read_detector_correspondence(path_to_setup + "/detector_correspondence.dat");

   auto MESYbuf = new mesytec::buffer_reader(mesytec_setup);
   printf ("\n[MESYTEC] : ***process_initialise*** called\n");
   printf ("[MESYTEC] : new mesytec_buffer_reader intialised = %p\n", MESYbuf);

   MESYbuf->initialise_readout();

   // start zmq receiver here (probably)
   zmq::socket_t* pub{nullptr};
   try {
      pub = new zmq::socket_t(context, ZMQ_SUB);
   } catch (zmq::error_t &e) {
      std::cout << "[MESYTEC] : ERROR: " << "process_start: failed to start ZeroMQ event spy: " << e.what () << std::endl;
   }

   int timeout=100;//milliseconds
   pub->setsockopt(ZMQ_RCVTIMEO, &timeout, sizeof(int));
   try {
      pub->connect(zmq_port.c_str());
   } catch (zmq::error_t &e) {
      std::cout << "[MESYTEC] : ERROR" << "process_start: failed to bind ZeroMQ endpoint " << zmq_port << ": " << e.what () << std::endl;
   }
   std::cout << "[MESYTEC] : Connected to MESYTECSpy " << zmq_port << std::endl;
   pub->setsockopt(ZMQ_SUBSCRIBE, "", 0);

   time_t current_time;
   time(&current_time);
   struct tm * timeinfo = localtime (&current_time);
   printf ("[MESYTEC] : MESYTEC-receiver beginning at: %s", asctime(timeinfo));

   uint32_t tot_events_parsed=0;
   uint32_t events_treated=0;
   zmq::message_t event;

   mesytec_mfm_converter CONVERTER(spy_port);
   const int status_update_interval=5; // print infos every x seconds

   /*** MAIN LOOP ***/
   while(1)
   {
      try{
#if defined (ZMQ_CPP14)
         if(!pub->recv(event))
#else
         if(!pub->recv(&event))
#endif
         {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
         }
      }
      catch(zmq::error_t &e) {
         std::cout << "[MESYTEC] : timeout on ZeroMQ endpoint: " << e.what () << std::endl;
         std::this_thread::sleep_for(std::chrono::milliseconds(100));
         continue;
      }

      try
      {
         events_treated = MESYbuf->read_buffer_collate_events((const uint8_t*)event.data(), event.size(), CONVERTER);
      }
      catch (std::exception& e)
      {
         std::string what{ e.what() };
         std::cout << "[MESYTEC] : Error parsing Mesytec buffer : " << what << std::endl;
         // abandon buffer & try next one
         MESYbuf->reset();
         continue;
      }
      tot_events_parsed+=MESYbuf->get_total_events_parsed();
      time_t t;
      time(&t);
      double time_elapsed=difftime(t,current_time);
      if(time_elapsed>=status_update_interval)
      {
         // print infos every x seconds (defined in Merger.conf by 'Status_Update_Interval' line)
         current_time=t;
         struct tm * timeinfo = localtime (&current_time);
         std::string now = asctime(timeinfo);
         now.erase(now.size()-1);//remove new line character
         std::cout << "[MESYTEC] : " << now << " : parse rate " << tot_events_parsed/time_elapsed << " evt./sec...\n";
         tot_events_parsed=0;
      }
   }
}
