#include "mesytec_module.h"
#include <sstream>

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

bool mesytec::is_event_header(uint32_t DATA)
{
   return ((DATA & data_flags::header_found_mask) == data_flags::header_found);
}

bool mesytec::is_end_of_event(uint32_t DATA)
{
   return ((DATA & data_flags::eoe_found_mask) == data_flags::eoe_found_mask) && !is_frame_header(DATA);
}

bool mesytec::is_end_of_event_tgv(uint32_t DATA)
{
   // TGV & MVLC scaler data use exact 0xC0000000 end of event marker
   return (DATA == data_flags::eoe_found_mask);
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

bool mesytec::is_exts_friend(uint32_t DATA)
{
   return ((DATA & data_flags::exts_friend_mask) == data_flags::exts_friend);
}

std::string mesytec::decode_type(uint32_t DATA)
{
   std::ostringstream ss;

   if(is_frame_header(DATA))
      return decode_frame_header(DATA);
   else if(is_event_header(DATA))
   {
      if(module_id(DATA)==0x1) ss << "TGV";
      else
         ss << "EVENT-HEADER: Module-ID=" << std::hex << std::showbase << (int)module_id(DATA);
      return ss.str();
   }
   else if(is_end_of_event(DATA))
      return "END-OF-EVENT";
   else if(is_fill_word(DATA))
      return "FILL-WORD";
   else if(is_mdpp_data(DATA))
      return "MDPP-DATA";
   else if(is_extended_ts(DATA))
      return "EXT-TS";
   else if(is_exts_friend(DATA))
      return "EXT-TS-FRIEND";
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

std::string mesytec::decode_frame_header(mesytec::u32 header)
{
   std::ostringstream ss;

   ss << std::hex << std::showbase << header << std::dec << " FRAME-HEADER : ";
   auto headerInfo = extract_frame_info(header);

   switch (static_cast<frame_headers::FrameTypes>(headerInfo.type))
   {
       case frame_headers::SuperFrame:
           ss << "Super Frame (len=" << headerInfo.len;
           break;

       case frame_headers::StackFrame:
           ss << "Stack Result Frame (len=" << headerInfo.len;
           break;

       case frame_headers::BlockRead:
           ss << "Block Read Frame (len=" << headerInfo.len;
           break;

       case frame_headers::StackError:
           ss << "Stack Error Frame (len=" << headerInfo.len;
           break;

       case frame_headers::StackContinuation:
           ss << "Stack Result Continuation Frame (len=" << headerInfo.len;
           break;

       case frame_headers::SystemEvent:
           ss << "System Event : ";
           ss << system_event_type_to_string(system_event::extract_subtype(header));
           ss << " (len=" << headerInfo.len;
           break;
   }

   switch (static_cast<frame_headers::FrameTypes>(headerInfo.type))
   {
       case frame_headers::StackFrame:
       case frame_headers::BlockRead:
       case frame_headers::StackError:
       case frame_headers::StackContinuation:
           {
               u16 stackNum = (header >> frame_headers::StackNumShift) & frame_headers::StackNumMask;
               ss << ", stackNum=" << stackNum;
           }
           break;

       case frame_headers::SuperFrame:
       case frame_headers::SystemEvent:
           break;
   }

   u8 frameFlags = (header >> frame_headers::FrameFlagsShift) & frame_headers::FrameFlagsMask;

   ss << ", frameFlags=" << format_frame_flags(frameFlags) << ")";

   return ss.str();
}

std::string mesytec::format_frame_flags(mesytec::u8 frameFlags)
{
   if (!frameFlags)
       return "none";

   std::vector<std::string> buffer;

   if (frameFlags & frame_flags::Continue)
       buffer.emplace_back("continue");

   if (frameFlags & frame_flags::SyntaxError)
       buffer.emplace_back("syntax");

   if (frameFlags & frame_flags::BusError)
       buffer.emplace_back("BERR");

   if (frameFlags & frame_flags::Timeout)
       buffer.emplace_back("timeout");

   return util::join(buffer, ", ");
}

const char*mesytec::get_frame_flag_shift_name(mesytec::u8 flag_shift)
{
   if (flag_shift == frame_flags::shifts::Timeout)
       return "Timeout";

   if (flag_shift == frame_flags::shifts::BusError)
       return "BusError";

   if (flag_shift == frame_flags::shifts::SyntaxError)
       return "SyntaxError";

   if (flag_shift == frame_flags::shifts::Continue)
       return "Continue";

   return "Unknown";
}

std::string mesytec::system_event_type_to_string(mesytec::u8 eventType)
{
   namespace T = system_event::subtype;

   switch (eventType)
   {
       case T::EndianMarker:
           return "EndianMarker";
       case T::BeginRun:
           return "BeginRun";
       case T::EndRun:
           return "EndRun";
       case T::MVMEConfig:
           return "MVMEConfig";
       case T::UnixTimetick:
           return "UnixTimetick";
       case T::Pause:
           return "Pause";
       case T::Resume:
           return "Resume";
       case T::MVLCCrateConfig:
           return "MVLCCrateConfig";
       case T::EndOfFile:
           return "EndOfFile";
       default:
           break;
   }

   std::ostringstream output;
   output << "unknown/custom (" << std::hex << std::showbase << eventType << ")";
   return output.str();
}
