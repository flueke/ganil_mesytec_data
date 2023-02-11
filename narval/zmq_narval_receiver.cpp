#include "zmq_narval_receiver.h"
#include <iostream>

/* Functions called on "Init" */
void process_config (char *directory_path, unsigned int *error_code)
{
   zmq_port = directory_path; // pass ZMQ port to subscribe to in 'algo_path'
   printf ("\n[ZMQ] : ***process_config*** called\n");
   printf ("[ZMQ] : MESYTECSpy port = %s\n",zmq_port.c_str());
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
}

/* Functions called on "Start" */
void process_start (struct my_struct *,
                    unsigned int *error_code)
{
   std::cout << "[ZMQ] : ***process_start*** called\n";

   // start zmq receiver here (probably)
   try {
      pub = new zmq::socket_t(context, ZMQ_SUB);
   } catch (zmq::error_t &e) {
      std::cout << "[ZMQ] : ERROR: " << "process_start: failed to start ZeroMQ event spy: " << e.what () << std::endl;
   }

   int timeout=500;//milliseconds
#if defined (ZMQ_CPP14)
   pub->setsockopt(ZMQ_RCVTIMEO, &timeout, sizeof(int));
#else
   pub->set(zmq::sockopt::rcvtimeo,timeout);
#endif
   try {
      pub->connect(zmq_port.c_str());
   } catch (zmq::error_t &e) {
      std::cout << "[ZMQ] : ERROR" << "process_start: failed to bind ZeroMQ endpoint " << zmq_port << ": " << e.what () << std::endl;
   }
#if defined (ZMQ_CPP14)
  pub->setsockopt(ZMQ_SUBSCRIBE, "", 0);
#else
   pub->set(zmq::sockopt::subscribe,"");

#endif
   std::cout << "[ZMQ] : SUBSCRIBED to ZMQ PUBlisher " << zmq_port << std::endl;

   time(&current_time);
   struct tm * timeinfo = localtime (&current_time);
   printf ("[ZMQ] : ZMQ Narval receiver beginning at: %s", asctime(timeinfo));
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

   if(!send_last_event)
   {
      // get first event from ZMQ
      try{
#if defined (ZMQ_CPP14)
            if(!pub->recv(event))
#else
            if(!pub->recv(&event))
#endif
         {
            //std::cout << "Got no event from ZMQ" << std::endl;
            return;
         }
      }
      catch(zmq::error_t &e) {
         std::cout << "timeout on ZeroMQ endpoint : " << e.what () << std::endl;
         return;
      }
   }

   send_last_event=false;

   while(1)
   {
      // copy events to output buffer until full
      if(*used_size_of_output_buffer+event.size() > size_of_output_buffer)
      {
         send_last_event = true; // put current event at start of next output buffer, this one is full
         break;
      }

      // add event to output buffer
      memcpy((unsigned char*)output_buffer + *used_size_of_output_buffer, event.data(), event.size());
      *used_size_of_output_buffer += event.size();

      // get next event from ZMQ
      try{
#ifdef ZMQ_CPP11
         if(!pub->recv(event))
#else
         if(!pub->recv(&event))
#endif
         {
            //std::cout << "Got no event from ZMQ" << std::endl;
            return;
         }
      }
      catch(zmq::error_t &e) {
         std::cout << "timeout on ZeroMQ endpoint : " << e.what () << std::endl;
         return;
      }
   }
}

/* Functions called on "Stop" */
void process_stop (struct my_struct *,
                   unsigned int *error_code)
{
   std::cout << "[ZMQ] : ***process_stop*** called\n";
   // delete zmq server here (probably)...
   pub->close();
   *error_code = 0;
   std::cout << "[ZMQ] : shut down of ZMQ socket\n";
}

/* Functions called on "BreakUp"0
    - also called on "Init" if state is "Ready" */
void process_reset (struct my_struct *,
                    unsigned int *error_code)
{
   std::cout << "[ZMQ] : ***process_reset*** called\n";
   *error_code = 0;
}
void process_unload (struct my_struct *algo_data,
                     unsigned int *error_code)
{
   std::cout << "[ZMQ] : ***process_unload*** called\n";
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
   std::cout << "[ZMQ] : ***process_pause*** called\n";
   /* put your code here */
   *error_code = 0;
}
void process_resume (struct my_struct *,
                     unsigned int *error_code)
{
   std::cout << "[ZMQ] : ***process_resume*** called\n";
   /* put your code here */
   *error_code = 0;
}

