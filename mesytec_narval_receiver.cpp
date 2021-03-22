#include "mesytec_narval_receiver.h"
#include <iostream>

/* Functions called on "Init" */
void process_config (char *directory_path, unsigned int *error_code)
{
   printf ("\nMESYTEC-receiver::process_config\n");
   printf ("MESYTECSpy port = %s\n",directory_path);
   zmq_port = directory_path;
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
   send_last_event=false;

   auto mesytec_setup = mesytec::define_setup
         (
            {
               {"MDPP-16", 0x0, 16, mesytec::SCP},
               {"MDPP-32", 0x10, 32, mesytec::SCP}
            }
            );

   MESYbuf = new mesytec::mesytec_buffer_reader(mesytec_setup);
}

/* Functions called on "Start" */
void process_start (struct my_struct *,
                    unsigned int *error_code)
{
   need_more_data=output_buffer_full=0;

   // start zmq receiver here (probably)
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
   std::cout << "Connected to MESYTECSpy " << zmq_port << std::endl;
   pub->setsockopt(ZMQ_SUBSCRIBE, "", 0);

   time(&current_time);
   struct tm * timeinfo = localtime (&current_time);
   printf ("MESYTEC-receiver beginning at: %s", asctime(timeinfo));
   *error_code = 0;
}

void process_block (struct my_struct *,
                    void *output_buffer,
                    unsigned int size_of_output_buffer,
                    unsigned int *used_size_of_output_buffer,
                    unsigned int *error_code)
{
   *used_size_of_output_buffer =   0;
   *error_code = 0;

   mesytec_mfm_converter CONVERTER{output_buffer,size_of_output_buffer,used_size_of_output_buffer};

   // previous call ended because output buffer was full before we finished parsing events in
   // last buffer read from ZMQ
   if(MESYbuf->is_storing_last_complete_event())
   {
      // put event in output buffer
      MESYbuf->cleanup_last_complete_event(CONVERTER);
   }
   // check if previous call read to the end of the last buffer read from ZMQ
   if(MESYbuf->get_remaining_bytes_in_buffer()>0)
   {
      // continue parsing old buffer, start after end of last event
      try
      {
         MESYbuf->read_buffer(
                  (const uint8_t*)MESYbuf->get_buffer_position(),
                  MESYbuf->get_remaining_bytes_in_buffer(),
                  CONVERTER);
      }
      catch (std::exception& e)
      {
         std::string what{ e.what() };
         if(!send_last_event)
         {
            std::cout << "Got unknown exception from MESYTEC converter: " << what << std::endl;
            throw(e);
         }
         else
            return; // output buffer is full
      }
   }
   // now begin loop receiving buffers from ZMQ, parsing & filling output buffer
   // until it is full
   while( 1 )
   {
      try{
         if(!pub->recv(&event))
         {
            std::cout << "Got no event from zeromq" << std::endl;
            break;
         }
      }
      catch(zmq::error_t &e) {
         std::cout << "timeout on ZeroMQ endpoint: " << e.what () << std::endl;
         break;
      }
      try
      {
         MESYbuf->read_buffer((const uint8_t*)event.data(), event.size(), CONVERTER);
      }
      catch (std::exception& e)
      {
         std::string what{ e.what() };
         if(!send_last_event)
         {
            std::cout << "Got unknown exception from MESYTEC converter: " << what << std::endl;
            throw(e);
         }
         else
            break; // output buffer is full
      }
   } // while(1)
}

void mesytec_mfm_converter::operator()(mesytec::mdpp_event &event)
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
      send_last_event=true;
      throw(std::runtime_error("output buffer full"));
   }

   ///////////////////MFM FRAME CONVERSION////////////////////////////////////
   mfmevent[0] = 0xc1;  // little-endian, blob frame, unit block size 2 bytes (?)
   *((uint32_t*)(&mfmevent[1])) = (uint32_t)mfmeventsize/2;// frameSize in unit block size
   mfmevent[4] = 0x0;  // dataSource
   *((uint16_t*)(&mfmevent[5])) = 0x4adf; // frame type (0x4adf)
   mfmevent[7] = 0x00; // frame revision 0

   // next 6 bytes [8]-[13] are for the timestamp - implement when ready

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
void process_stop (struct my_struct *algo_data,
                   unsigned int *error_code)
{
   // delete zmq server here (probably)...
   pub->close();
   *error_code = 0;
}

/* Functions called on "BreakUp"0
    - also called on "Init" if state is "Ready" */
void process_reset (struct my_struct *algo_data,
                    unsigned int *error_code)
{
   *error_code = 0;

   delete MESYbuf;
   MESYbuf=nullptr;
}
void process_unload (struct my_struct *algo_data,
                     unsigned int *error_code)
{
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
   /* put your code here */
   *error_code = 0;
}
void process_resume (struct my_struct *,
                     unsigned int *error_code)
{
   /* put your code here */
   *error_code = 0;
}

