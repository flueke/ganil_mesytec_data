#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <map>

#include "mesytec_buffer_reader.h"
#include "mesytec_data.h"

using namespace mesytec;

void callback_function(event &mesy_event,experimental_setup&)
{
   // called for each complete event parsed from the mesytec stream
   //
   // this builds an MFMFrame for each event and copies it into the output stream,
   // unless there is no room left in the buffer, in which case it will be treated
   // the next time that process_block is called

   if(mesy_event.size_of_buffer() == 0)
   {
      std::cout << "********************* Event Size ZERO ***************************" << std::endl;
      throw( std::runtime_error("event size is zero") );
   }
}

int main(int argc, char* argv[])
{
   if(argc<3)
   {
      std::cout << "Usage:\n";
      std::cout << "read_listfile [path to crate_map.dat & detector_correspondence.dat] [name of file]\n";
      std::exit(0);
   }
   std::string path_to_config = argv[1];
   std::string filename = argv[2];

   std::string crate_map = path_to_config + "/crate_map.dat";

   std::ifstream istrm(filename, std::ios::binary);
   if (!istrm.is_open()) {
      std::cout << "failed to open " << filename << '\n';
      return 1;
   }

   uint64_t bytes_read=0;
   unsigned char bite;
   while( 1 )
   {
      istrm.read(reinterpret_cast<char*>(&bite), sizeof bite);
      ++bytes_read;

      //std::cout << std::hex << std::showbase << (int)bite << " : " << (char)bite << std::endl;

      if(bite == 0xcd) break;
   }
   istrm.read(reinterpret_cast<char*>(&bite), sizeof bite);

   buffer_reader buf_rdr;
   buf_rdr.read_crate_map(crate_map);
   buf_rdr.initialise_readout();

   uint8_t buffer[0x1000];
   while( istrm.good() )
   {
      istrm.read(reinterpret_cast<char*>(buffer), 0x1000);
      if(!istrm.good()) break;

      buf_rdr.read_buffer_collate_events(buffer,0x1000,callback_function);
   }
}
