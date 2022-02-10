#include "mesytec_narval_receiver.h"
#include <iostream>

/* Functions called on "Init" */
void process_config (char *directory_path, unsigned int *error_code)
{
   path_to_setup = directory_path;
   printf ("\n[MESYTEC] : ***process_config*** called\n");
   zmq_port = "tcp://mesytecPC:5575";
   printf ("[MESYTEC] : MESYTECSpy port = %s\n",zmq_port.c_str());
   printf ("[MESYTEC] :  - will read crate map in = %s/crate_map.dat\n", path_to_setup.c_str());
   *error_code = 0;
}

struct my_struct *process_register (unsigned int *error_code)
{
   struct my_struct *algo_data;

   algo_data = (struct my_struct *) malloc (sizeof (struct my_struct));
   algo_data->id = next_id;
   next_id++;

   *error_code=0;
   return algo_data;
}

void process_initialise (struct my_struct *,
                         unsigned int *error_code)
{
   /* put your code here */
   *error_code = 0;

   // read crate map
   mesytec::experimental_setup mesytec_setup;
   mesytec_setup.read_crate_map(path_to_setup + "/crate_map.dat");

   MESYbuf = new mesytec::buffer_reader(mesytec_setup);
   printf ("\n[MESYTEC] : ***process_initialise*** called\n");
   printf ("[MESYTEC] : new mesytec_buffer_reader intialised = %p\n", MESYbuf);

   log_parse_errors.open("/data/eindraX/e818_test_indra/acquisition/log/mesytec_parse_errors.log");
   printf ("[MESYTEC] : parse error buffers will be written in /data/eindraX/e818_test_indra/acquisition/log/mesytec_parse_errors.log\n");
}

/* Functions called on "Start" */
void process_start (struct my_struct *,
                    unsigned int *error_code)
{
   std::cout << "[MESYTEC] : ***process_start*** called\n";

   MESYbuf->initialise_readout();

   // start zmq receiver here (probably)
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

   time(&current_time);
   struct tm * timeinfo = localtime (&current_time);
   printf ("[MESYTEC] : MESYTEC-receiver beginning at: %s", asctime(timeinfo));
   *error_code = 0;

   tot_events_parsed=0;
}

void process_block (struct my_struct *,
                    void *output_buffer,
                    unsigned int size_of_output_buffer,
                    unsigned int *used_size_of_output_buffer,
                    unsigned int *error_code)
{
   //std::cout << "[MESYTEC] : ***process_block*** called\n";
   *used_size_of_output_buffer =   0;
   *error_code = 0;

   output_buffer_is_full = false;

   mesytec_mfm_converter CONVERTER{output_buffer,size_of_output_buffer,used_size_of_output_buffer};

   // previous call ended because output buffer was full before we finished parsing events in
   // last buffer read from ZMQ
   if(MESYbuf->is_storing_last_complete_event())
   {
      // put event in output buffer
      //std::cout << "[MESYTEC] : sending event from last buffer\n";
      MESYbuf->cleanup_last_complete_event(CONVERTER);
      ++tot_events_parsed;
   }
   // check if previous call read to the end of the last buffer read from ZMQ
   if(MESYbuf->get_remaining_bytes_in_buffer()>0)
   {
      // continue parsing old buffer, start after end of last event
      //std::cout << "[MESYTEC] : continuing to treat last buffer - bytes remaining " << MESYbuf->get_remaining_bytes_in_buffer() << "\n";
      try
      {
         MESYbuf->read_buffer_collate_events(
                  (const uint8_t*)MESYbuf->get_buffer_position(),
                  MESYbuf->get_remaining_bytes_in_buffer(),
                  CONVERTER);
      }
      catch (std::exception& e)
      {
         std::string what{ e.what() };
         if(!output_buffer_is_full) // error parsing buffer
         {
            std::cout << "[MESYTEC] : Error parsing STORED Mesytec buffer : " << what << std::endl;
            // abandon buffer & try next one
            MESYbuf->reset();
            return;
         }
         else { // output buffer is full
            //std::cout << "[MESYTEC] : output buffer is full after parsing " << MESYbuf->get_total_events_parsed() << " events\n";
            //std::cout << "[MESYTEC] : Used size of buffer = " << *used_size_of_output_buffer << "\n";
            tot_events_parsed+=MESYbuf->get_total_events_parsed();
            return;
         }
      }
      tot_events_parsed+=MESYbuf->get_total_events_parsed();
   }
   // now begin loop receiving buffers from ZMQ, parsing & filling output buffer
   // until it is full
   uint32_t events_treated=0;
   //std::cout << "[MESYTEC] : beginning receive-treat loop\n";

   int iterations = max_iterations_of_parse_loop;
   while( iterations-- )
   {
      try{
#if defined (ZMQ_CPP14)
         if(!pub->recv(event))
#else
         if(!pub->recv(&event))
#endif
         {
            //std::cout << "[MESYTEC] : Got no event from zeromq" << std::endl;
            break;
         }
      }
      catch(zmq::error_t &e) {
         std::cout << "[MESYTEC] : timeout on ZeroMQ endpoint: " << e.what () << std::endl;
         break;
      }
      //std::cout << "[MESYTEC] : Received buffer of " << event.size() << " bytes from MVLC\n";
      if(!first_buffer_has_been_read)
      {
         log_parse_errors << "BEGINNING OF FIRST BUFFER:" << std::endl;
         MESYbuf->dump_buffer((const uint8_t*)event.data(), event.size(), event.size()/4, log_parse_errors, "", true);
         log_parse_errors << std::endl;
         first_buffer_has_been_read = true;
      }
      try
      {
         events_treated = MESYbuf->read_buffer_collate_events((const uint8_t*)event.data(), event.size(), CONVERTER);
      }
      catch (std::exception& e)
      {
         std::string what{ e.what() };
         if(!output_buffer_is_full) // error parsing buffer
         {
            std::cout << "[MESYTEC] : Error parsing Mesytec buffer : " << what << std::endl;
            time_t t;
            time(&t);
            struct tm * timeinfo = localtime (&t);
            std::string now = asctime(timeinfo);
            now.erase(now.size()-1);//remove new line character
            log_parse_errors << now << std::endl;
            log_parse_errors << "DUMPING END OF PREVIOUS BUFFER:" << std::endl;
            MESYbuf->dump_end_last_buffer(log_parse_errors);
            log_parse_errors << "\n\nDUMPING CURRENT BUFFER:" << std::endl;
            MESYbuf->dump_buffer((const uint8_t*)event.data(), event.size(), 100, log_parse_errors, what);
            log_parse_errors << std::endl << std::endl;
            std::cout << "[MESYTEC] : see /data/eindraX/e818_test_indra/acquisition/log/mesytec_parse_errors.log" << std::endl;
            // abandon buffer & try next one
            MESYbuf->reset();
            return;
         }
         else { // output buffer is full
            //std::cout << "[MESYTEC] : Buffer full after treating " << MESYbuf->get_total_events_parsed() << " events\n";
            //std::cout << "[MESYTEC] : Used size of buffer = " << *used_size_of_output_buffer << "\n";
            tot_events_parsed+=MESYbuf->get_total_events_parsed();
            break; // output buffer is full
         }
      }
      tot_events_parsed+=MESYbuf->get_total_events_parsed();
      //std::cout << "[MESYTEC] : Treated " << events_treated << " events without filling buffer...\n";
   } // while(1)
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
   //std::cout << "[MESYTEC] : exiting receive-treat loop, " << MESYbuf->get_total_events_parsed() << " events were parsed\n";
}

void mesytec_mfm_converter::operator()(mesytec::mdpp::event &event)
{
   // called for each complete event parsed from the mesytec stream
   //
   // this builds an MFMFrame for each event and copies it into the output stream,
   // unless there is no room left in the buffer, in which case it will be treated
   // the next time that process_block is called

   // 24 bytes for MFM header, plus the Mesytec data buffer
   size_t mfmeventsize = 24 + event.size_of_buffer()*4;
   // check we still have room in the output buffer
   if(*used_size_of_output_buffer+mfmeventsize>size_of_output_buffer)
   {
      output_buffer_is_full=true;
      throw(std::runtime_error("output buffer full"));
   }

   ///////////////////MFM FRAME CONVERSION////////////////////////////////////
   mfmevent[0] = 0xc1;  // little-endian, blob frame, unit block size 2 bytes (?)
   *((uint32_t*)(&mfmevent[1])) = (uint32_t)mfmeventsize/2;// frameSize in unit block size
   mfmevent[4] = 0x0;  // dataSource
   *((uint16_t*)(&mfmevent[5])) = mesytec::mdpp::mfm_frame_type; // frame type
   mfmevent[7] = 0x00; // frame revision 0

   // next 6 bytes [8]-[13] are for the timestamp
   *((uint16_t*)(&mfmevent[8])) = event.tgv_ts_lo;
   *((uint16_t*)(&mfmevent[10])) = event.tgv_ts_mid;
   *((uint16_t*)(&mfmevent[12])) = event.tgv_ts_hi;
   //printf("mfmframe: ts_lo %#06x  ts_mid %#06x  ts_hi %#06x\n",*((uint16_t*)(&mfmevent[8])),*((uint16_t*)(&mfmevent[10])),*((uint16_t*)(&mfmevent[12])));

   // bytes [14]-[17]: event number (event counter from mesytec EOE)
   *((uint32_t*)(&mfmevent[14])) = event.event_counter;
   // bytes [20]-[23] number of bytes in mesytec data blob
   *((uint32_t*)(&mfmevent[20])) = (uint32_t)event.size_of_buffer()*4;

   // copy mesytec data into mfm frame 'blob'
   memcpy(mfmevent+24, event.get_output_buffer().data(), mfmeventsize-24);
   ///////////////////MFM FRAME CONVERSION////////////////////////////////////

   // add frame to output buffer
   memcpy((unsigned char*)output_buffer + *used_size_of_output_buffer,
          mfmevent, mfmeventsize);
   *used_size_of_output_buffer += mfmeventsize;
}

/* Functions called on "Stop" */
void process_stop (struct my_struct *,
                   unsigned int *error_code)
{
   std::cout << "[MESYTEC] : ***process_stop*** called\n";
   // delete zmq server here (probably)...
   pub->close();
   *error_code = 0;
   std::cout << "[MESYTEC] : shut down of ZMQ socket\n";
   log_parse_errors.close();
}

/* Functions called on "BreakUp"0
    - also called on "Init" if state is "Ready" */
void process_reset (struct my_struct *,
                    unsigned int *error_code)
{
   std::cout << "[MESYTEC] : ***process_reset*** called\n";
   *error_code = 0;

   delete MESYbuf;
   MESYbuf=nullptr;
}
void process_unload (struct my_struct *algo_data,
                     unsigned int *error_code)
{
   std::cout << "[MESYTEC] : ***process_unload*** called\n";
   if(algo_data)
   {
      free(algo_data);
      algo_data = NULL;
   }

   *error_code = 0;
}

/* Functions called ... When? */
void process_pause (struct my_struct *,
                    unsigned int *error_code)
{
   std::cout << "[MESYTEC] : ***process_pause*** called\n";
   /* put your code here */
   *error_code = 0;
}
void process_resume (struct my_struct *,
                     unsigned int *error_code)
{
   std::cout << "[MESYTEC] : ***process_resume*** called\n";
   /* put your code here */
   *error_code = 0;
}

