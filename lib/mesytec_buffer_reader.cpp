#include "mesytec_buffer_reader.h"

/** \example mesytec_narval_receiver.cpp

This is an example of use of the mesytec::event & mesytec::buffer_reader classes.

It implements a Narval actor which:

 + connects to the ZMQ transmitter of the Mesytec DAQ;
 + encapsulates each DAQ event inside an MFMMesytecFrame GANIL data frame;
 + copies the data frames into the output buffer of the actor

\include mesytec_narval_receiver.h

*/
