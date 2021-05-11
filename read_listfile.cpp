#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <map>

#include "mesytec_data.h"
#include "mesytec_buffer_reader.h"

int main(int argc, char* argv[])
{
   if(argc<4)
   {
      std::cout << "Usage:\n";
      std::cout << "read_listfile [path to crate_map.dat & detector_correspondence.dat] [name of file] [number of events to read]\n";
      std::exit(0);
   }
   std::string path_to_config = argv[1];
   std::string filename = argv[2];
   int nevents = std::stoi(std::string(argv[3]));

   std::string crate_map = path_to_config + "/crate_map.dat";
   std::string det_cor = path_to_config + "/detector_correspondence.dat";

   mesytec::experimental_setup mesytec_setup;
   mesytec_setup.read_crate_map(crate_map);
   mesytec_setup.read_detector_correspondence(det_cor);

   std::ifstream istrm(filename, std::ios::binary);
   if (!istrm.is_open()) {
      std::cout << "failed to open " << filename << '\n';
      return 1;
   }
   else {
      std::cout << "I opened file " << filename << std::endl;

      // look for magic sequence
      const std::vector<unsigned char> first_sequence {0x7d, 0xa, 0x20, 0x20, 0x20, 0x20, 0x7d, 0xa, 0x7d, 0xa};
      const std::vector<unsigned char> second_sequence {0x2, 0x40, 0, 0xfa};
      size_t sequence_size = first_sequence.size();
      size_t sequence_found = 0;
      uint64_t bytes_read=0;
      while( sequence_found < sequence_size )
      {
         unsigned char bite;
         istrm.read(reinterpret_cast<char*>(&bite), sizeof bite);
         ++bytes_read;
         if( bite == first_sequence[sequence_found] ) {
            ++sequence_found;
         }
         else {
            sequence_found = 0;
         }
      }
      std::cout << "I found the first sequence after reading " << bytes_read << " bytes\n";
      std::cout << "which is a multiple of 4 byte words? " << bytes_read%4 << " (if 0, true)\n";
      sequence_size = second_sequence.size();
      sequence_found = 0;
      bytes_read=0;
      while( sequence_found < sequence_size )
      {
         unsigned char bite;
         istrm.read(reinterpret_cast<char*>(&bite), sizeof bite);
         ++bytes_read;
         if( bite == second_sequence[sequence_found] ) {
            ++sequence_found;
         }
         else {
            sequence_found = 0;
         }
      }
      std::cout << "I found the second sequence after reading " << bytes_read << " bytes\n";
      std::cout << "which is a multiple of 4 byte words? " << bytes_read%4 << " (if 0, true)\n";

      // beginning of data stream ? nearly...

      // need to read 2 consecutive EOE to find start of data (EOE - EOE - HEADER..)
      int number_eoe=0;
      for(int words=0; words<50; ++words){
         uint32_t DATA = mesytec::read_data_word(istrm);
         if(mesytec::is_end_of_event(DATA)) ++number_eoe;
         if(number_eoe==2) break;
      }
      // next word should be header
      auto next_word = mesytec::read_data_word(istrm);
      if(!mesytec::is_header(next_word))
      {
         printf("Fatal error. next word should be header\n");
         throw(std::runtime_error("data corruption"));
      }

      std::map<uint32_t,mesytec::mdpp::event> event_map;

      mesytec_setup.readout.begin_readout();

      while(nevents--){
         // next_word is header for module data...
         mesytec::mdpp::module_data mod_data{next_word};
         mesytec_setup.readout.accept_module_for_readout(mod_data.module_id);
         auto& mmodule = mesytec_setup.get_module(mod_data.module_id);
         auto words_to_read = mod_data.data_words;  // (words_to_read-1) data words + 1 EOE
         bool got_eoe=false;
         while(words_to_read--)
         {
            next_word = mesytec::read_data_word(istrm);
            if(mesytec::is_mdpp_data(next_word)) {
               mmodule.set_data_word(next_word);
               //module.print_mdpp_data();
               mod_data.add_data( mmodule.data_type(), mmodule.channel_number(), mmodule.channel_data(), next_word);
            }
            else if(mesytec::is_end_of_event(next_word))
            {
               if(words_to_read){
                  // too early - missing data
                  printf("Fatal error. EOE read before end of data: words_to_read=%d\n", words_to_read);
                  throw(std::runtime_error("incomplete data buffer"));
               }
               else {
                  got_eoe=true;
                  //MTEC.print_eoe(next_word);
                  mod_data.event_counter = mesytec::event_counter(next_word);
                  mod_data.eoe_word = next_word;
                  mesytec::mdpp::event& event = event_map[mod_data.event_counter];
                  event.event_counter = mod_data.event_counter;
                  event.add_module_data(mod_data);
                  if(mesytec_setup.readout.readout_complete())
                  {
                     // event is complete, can be sent to buffer etc.
                     event.ls(mesytec_setup);
//                     auto out_buf = event.get_output_buffer();
//                     std::cout << "Output buffer should contain " << event.size_of_buffer() << " 32-bit words" << std::endl;
//                     std::cout << "Output buffer contains " << out_buf.size() << " 32-bit words" << std::endl;
//                     std::cout << "Size of buffer in bytes: " << 4*out_buf.size() << std::endl;

//                     std::cout << "=====READING FROM BUFFER=====" << std::endl;
//                     mesytec::buffer_reader buf_read(mesytec_setup);
//                     buf_read.read_event_in_buffer((const uint8_t*)out_buf.data(), 4*out_buf.size(),
//                                           [](mesytec::mdpp::event& Event){ Event.ls(); });
//                     std::cout << "========END OF BUFFER========" << std::endl;

                     // delete event after sending/printing
                     auto it = event_map.find(event.event_counter);
                     event_map.erase(it);
                     mesytec_setup.readout.begin_readout();
                  }
               }
            }
         }
         // should only exit after reading EOE
         if(!got_eoe)
         {
            printf("Fatal error. never read EOE in data stream\n");
            throw(std::runtime_error("data corruption"));
         }
         // now read to next header
         bool got_header=false;
         do{
            next_word = mesytec::read_data_word(istrm);
            got_header=mesytec::is_header(next_word);
         } while(!got_header);
      }
   }
}
