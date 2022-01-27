#include "mesytec_module.h"

std::unordered_map<std::string,std::string> mesytec::module::data_type_aliases
      = {
        {"qdc_long", "qdc_long"}, {"adc", "adc"}, {"tdc", "tdc"}, {"trig","trig"},{"qdc_short","qdc_short"}
      };

std::map<uint8_t, mesytec::module> mesytec::define_setup(std::vector<mesytec::module> &&modules)
{
   std::map<uint8_t, module> setup;
   for(auto& mod : modules) setup[mod.id] = std::move(mod);
   return setup;
}

uint32_t mesytec::read_data_word(std::istream &data)
{
   // Read 4 bytes from stream and return 32-bit little-endian data word
   //
   // Throws end_of_buffer exception if 4 bytes cannot be read.

   uint32_t DATA;
   data.read(reinterpret_cast<char*>(&DATA), 4);
   if (!data.good()) throw end_of_buffer("failed to read 4 bytes from buffer");
   return DATA;
}

uint32_t mesytec::read_data_word(const uint8_t *data)
{
   // Read 4 bytes from buffer and return 32-bit little-endian data word

   uint32_t x = data[0]+(data[1]<<8)+(data[2]<<16)+(data[3]<<24);
//   printf("%02x %02x %02x %02x : %#04x %#04x\n",
//          data[0],data[1],data[2],data[3],x>>16,(x<<16)/0x10000);
   return x;
}

bool mesytec::is_header(uint32_t DATA)
{
   return ((DATA & data_flags::header_found_mask) == data_flags::header_found);
}

bool mesytec::is_end_of_event(uint32_t DATA)
{
   return ((DATA & data_flags::eoe_found_mask) == data_flags::eoe_found_mask);
}

bool mesytec::is_mdpp_data(uint32_t DATA)
{
   return ((DATA & data_flags::mdpp_data_mask) == data_flags::mdpp_data);
}

bool mesytec::is_fill_word(uint32_t DATA)
{
   return DATA == data_flags::fill_word_found;
}

bool mesytec::is_extended_ts(uint32_t DATA)
{
   return ((DATA & data_flags::extended_ts_mask) == data_flags::extended_ts);
}

std::string mesytec::decode_type(uint32_t DATA)
{
   if(is_header(DATA))
      return "HEADER";
   else if(is_end_of_event(DATA))
      return "EOE";
   else if(is_fill_word(DATA))
      return "FILL-WORD";
   else if(is_mdpp_data(DATA))
      return "MDPP-DATA";
   else if(is_extended_ts(DATA))
      return "EXT-TS";
   else
      return "(unknown)";
}

void mesytec::print_type(uint32_t DATA)
{
   std::cout << decode_type(DATA);
}

uint16_t mesytec::length_of_data(uint32_t DATA)
{
   return (DATA & data_flags::data_length_mask);
}

uint8_t mesytec::module_id(uint32_t DATA)
{
   return (DATA & data_flags::module_id_mask)/data_flags::module_id_div;
}

unsigned int mesytec::module_setting(uint32_t DATA)
{
   return (DATA & data_flags::module_setng_mask)/data_flags::module_setng_div;
}

void mesytec::print_header(uint32_t DATA)
{
   printf("== header_found ==\n data_length = %d words\n", length_of_data(DATA));
   printf(" module_id = %#2x  module_setting = %#2x\n", module_id(DATA), module_setting(DATA));
}

unsigned int mesytec::extended_timestamp(uint32_t DATA)
{
   return (DATA & data_flags::data_mask);
}

unsigned int mesytec::event_counter(uint32_t DATA)
{
   return (DATA & data_flags::eoe_event_counter_mask);
}

void mesytec::print_eoe(uint32_t DATA)
{
   printf("== EOE found ==\n event_counter = %d\n", event_counter(DATA));
}

void mesytec::print_ext_ts(uint32_t DATA)
{
   printf("== EXT-TS :: high_stamp = %5d\n", extended_timestamp(DATA));
}

bool mesytec::is_tgv_data(uint32_t DATA)
{
   return ((DATA&data_flags::tgv_data_mask_hi)==0);
}
