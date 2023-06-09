#ifndef ZMQ_NARVAL_RECEIVER_H
#define ZMQ_NARVAL_RECEIVER_H

#include "zmq_compat.h"
#include <ctime>

static int next_id = 0;
struct my_struct
{
  int id;
};

time_t current_time;
const int status_update_interval=5; // print infos every x seconds
std::string zmq_port;
zmq::context_t context(1);	// for ZeroMQ communications
zmq::socket_t *pub;
zmq::message_t event;
bool send_last_event=false;

/* you must have the following symbols */
/* see John Cresswell document for details : */
/* AGATA PSA and Tracking Algorithm Integration*/
extern "C" {
  void process_config (char *directory_path, unsigned int *error_code);
  struct my_struct *process_register (unsigned int *error_code);
  void process_block (struct my_struct *,
                      void *output_buffer,
                      unsigned int size_of_output_buffer,
                      unsigned int *used_size_of_output_buffer,
                      unsigned int *error_code);
  /* optionnal symbols */
  void process_start (struct my_struct *algo_data,
                      unsigned int *error_code);
  void process_stop (struct my_struct *algo_data,
                     unsigned int *error_code);

  void process_unload (struct my_struct *algo_data,
                       unsigned int *error_code);

 void process_initialise (struct my_struct *,
                          unsigned int *error_code);
  void process_reset (struct my_struct *algo_data,
                      unsigned int *error_code);
  void process_pause (struct my_struct *algo_data,
                      unsigned int *error_code);
  void process_resume (struct my_struct *algo_data,
                       unsigned int *error_code);
  //void ada_log_message(int id, char* message, int level);
}

/* coding region */
#endif // ZMQ_NARVAL_RECEIVER_H
