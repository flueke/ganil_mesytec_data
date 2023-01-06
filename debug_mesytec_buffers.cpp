#include "mesytec_buffer_reader.h"
#include <fstream>
#include <iostream>
#include <sstream>

int main(int argc, char* argv[])
{
   std::string path_to_setup = "/home/eindra/ganacq_manip/e818_test_indra";

   std::string file_to_read = argv[1];

   mesytec::experimental_setup mesytec_setup;
   mesytec_setup.read_crate_map(path_to_setup + "/crate_map.dat");

   mesytec::buffer_reader MESYbuf{mesytec_setup};

   std::ifstream read_file;
   read_file.open(file_to_read);
#define MAX_BUF_SIZE 2000000
   std::array<uint8_t,MAX_BUF_SIZE> buffer;
   size_t buf_size=0;

   bool in_a_buffer=false;

   union x_p { uint32_t x; uint8_t X[4]; } endian;

   while(read_file.good())
   {
      std::istringstream lineass;
      std::string linea;
      std::getline(read_file,linea);
      if(!read_file.good()) break;
      if(linea.find("===========")!=std::string::npos) continue;
      if(linea.find("Time is now")!=std::string::npos) continue;
      if(linea.find("NEW BUFFER")!=std::string::npos)
      {
         in_a_buffer=true;
         std::getline(read_file,linea);
         if(!read_file.good()) break;
         std::getline(read_file,linea);
         if(!read_file.good()) break;
         buf_size=0;
      }
      if(linea.find("END OF BUFFER")!=std::string::npos)
      {
         in_a_buffer=false;
         std::cout << "got buffer of " << std::dec << buf_size << " bytes" << std::endl;
         MESYbuf.read_buffer_collate_events(buffer.data(),1000000,[&mesytec_setup](mesytec::event& Event){ /*Event.ls(mesytec_setup);*/ });
      }
      if(in_a_buffer)
      {
         lineass.str(linea);
         std::string word;
         std::getline(lineass,word,' ');

         endian.x=std::stoul(word,nullptr,16);
         for(auto q : endian.X){
            buffer[buf_size]=q;
            ++buf_size;
            if(buf_size>MAX_BUF_SIZE) throw(std::runtime_error("buffer overflow"));
         }

         if(linea.find("FRAME-HEADER")!=std::string::npos)
         {
            // frame headers are doubled; ignore second
            std::getline(read_file,linea);
            if(!read_file.good()) break;
         }
      }
      //std::cout << linea << std::endl;
   }
}
