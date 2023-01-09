#ifndef MESYTEC_MODULE_H
#define MESYTEC_MODULE_H

#include <cstdint>
#include <vector>
#include <iostream>
#include <map>
#include <unordered_map>
#include <string>

namespace mesytec
{
   typedef uint8_t  u8;
   typedef uint16_t u16;
   typedef uint32_t u32;
   typedef uint64_t u64;

   // Constants for working with incoming data frames.
   // copied from mvlc_constants.h
   namespace frame_headers
   {
       enum FrameTypes: u8
       {
           SuperFrame          = 0xF1, // Outermost command buffer response frame.
           StackFrame          = 0xF3, // Outermost frame for readout data produced by command stack execution.
           BlockRead           = 0xF5, // Inner frame for block reads. Always contained within a StackFrame.
           StackError          = 0xF7, // Error notification frame embedded either between readout data or
                                       // sent to the command port for monitoring.
           StackContinuation   = 0xF9, // Continuation frame for BlockRead frames with the Continue bit set.
                                       // The last F9 frame in a sequence has the Continue bit cleared.
           SystemEvent         = 0xFA, // Software generated frames used for transporting additional
                                       // information. See the system_event namespace below for details.
       };

       // Header: Type[7:0] Continue[0:0] ErrorFlags[2:0] StackNum[3:0] CtrlId[2:0] Length[12:0]
       // The Continue bit and the ErrorFlags are combined into a 4 bit
       // FrameFlags field.

       static const u8 TypeShift           = 24;
       static const u8 TypeMask            = 0xff;

       static const u8 FrameFlagsMask      = 0xf;
       static const u8 FrameFlagsShift     = 20;

       static const u8 StackNumShift       = 16;
       static const u8 StackNumMask        = 0xf;

       static const u8 CtrlIdShift         = 13;
       static const u8 CtrlIdMask          = 0b111;

       static const u16 LengthShift        = 0;
       static const u16 LengthMask         = 0x1fff;
   }

   inline u8 get_frame_type(u32 header)
   {
       return (header >> frame_headers::TypeShift) & frame_headers::TypeMask;
   }

   // Constants describing the Continue and ErrorFlag bits present in StackFrames
   // and StackContinuations.
   namespace frame_flags
   {
       // These shifts are relative to the beginning of the FrameFlags field.
       namespace shifts
       {
           static const u8 Timeout     = 0;
           static const u8 BusError    = 1;
           static const u8 SyntaxError = 2;
           static const u8 Continue    = 3;
       }

       static const u8 Timeout     = 1u << shifts::Timeout;
       static const u8 BusError    = 1u << shifts::BusError;
       static const u8 SyntaxError = 1u << shifts::SyntaxError;
       static const u8 Continue    = 1u << shifts::Continue;

       static const u8 AllErrorFlags = (
           frame_flags::Timeout |
           frame_flags::BusError |
           frame_flags::SyntaxError);
   }

   // Software generated system events which do not collide with the MVLCs framing
   // format.
   namespace system_event
   {
       // TTTT TTTT CUUU SSSS SSSL LLLL LLLL LLLL
       // Type     [ 7:0] set to 0xFA
       // Continue [ 0:0] continue bit set for all but the last part
       // Unused   [ 2:0] 3 unused flag bits
       // Subtype  [ 6:0] 7 bit system event SubType
       // Length   [12:0] 13 bit length counted in 32-bit words

       static const u8 ContinueShift = 23;
       static const u8 ContinueMask  = 0b1;

       static const u8 CtrlIdShift   = 20;
       static const u8 CtrlIdMask    = 0b111;

       static const u8 SubtypeShift  = 13;
       static const u8 SubtypeMask   = 0x7f;

       static const u16 LengthShift  = 0;
       static const u16 LengthMask   = 0x1fff;

       static const u32 EndianMarkerValue = 0x12345678u;

       namespace subtype
       {
           static const u8 EndianMarker    = 0x01;

           // Written right before a DAQ run starts. Contains a software timestamp.
           static const u8 BeginRun        = 0x02;

           // Written right before a DAQ run ends. Contains a software timestamp.
           static const u8 EndRun          = 0x03;

           // For compatibility with existing mvme-generated listfiles. This
           // section contains a JSON encoded version of the mvme VME setup.
           // This section is not directly used by the library.
           static const u8 MVMEConfig      = 0x10;

           // Software generated low-accuracy timestamp, written once per second.
           // Contains a software timestamp
           static const u8 UnixTimetick    = 0x11;

           // Written when the DAQ is paused. Contains a software timestamp.
           static const u8 Pause           = 0x12;

           // Written when the DAQ is resumed. Contains a software timestamp.
           static const u8 Resume          = 0x13;

           // The config section generated by this library.
           static const u8 MVLCCrateConfig = 0x14;

           // Written before closing the listfile.
           static const u8 EndOfFile       = 0x77;

           static const u8 SubtypeMax      = SubtypeMask;
       }

       inline u8 extract_subtype(u32 header)
       {
           return (header >> SubtypeShift) & SubtypeMask;
       }

   }
   // added by me
   inline bool is_system_unix_time(u32 header)
   {
      return (get_frame_type(header)==frame_headers::SystemEvent
              && system_event::extract_subtype(header)==system_event::subtype::UnixTimetick);
   }

   // end of mvlc_constants.h copy

   // copied from string_util.h
   namespace util
   {
      inline std::string join(const std::vector<std::string> &parts, const std::string &sep = ", ")
      {
          std::string result;

          auto it = parts.begin();

          while (it != parts.end())
          {
              result += *it++;

              if (it < parts.end())
                  result += sep;
          }

          return result;
      }
   }

   // copied from mvlc_util.h/cc
   struct FrameInfo
   {
       u16 len;
       u8 type;
       u8 flags;
       u8 stack;
       u8 ctrl;
   };

   inline FrameInfo extract_frame_info(u32 header)
   {
       using namespace frame_headers;

       FrameInfo result;

       result.len   = (header >> LengthShift) & LengthMask;
       result.type  = (header >> TypeShift) & TypeMask;
       result.flags = (header >> FrameFlagsShift) & FrameFlagsMask;
       result.stack = (header >> StackNumShift) & StackNumMask;

       if (result.type == frame_headers::SystemEvent)
           result.ctrl = (header >> system_event::CtrlIdShift) & system_event::CtrlIdMask;
       else
           result.ctrl = (header >> CtrlIdShift) & CtrlIdMask;

       return result;
   }
   inline bool is_frame_header(u32 header)
   {
      using namespace frame_headers;
      u8 type  = (header >> TypeShift) & TypeMask;
      return (type==SuperFrame || type==StackFrame || type==BlockRead || type==StackError || type==StackContinuation || type==SystemEvent);
   }

   std::string decode_frame_header(u32 header);
   std::string format_frame_flags(u8 frameFlags);
   inline bool has_error_flag_set(u8 frameFlags)
   {
       return (frameFlags & frame_flags::AllErrorFlags) != 0u;
   }

   const char *get_frame_flag_shift_name(u8 flag_shift);
   // String representation for the known system_event::subtype flags.
   // Returns "unknown/custom" for user defined flags.
   std::string system_event_type_to_string(u8 eventType);


   namespace data_flags
   {
      const uint32_t header_found_mask = 0xF0000000;
      const uint32_t header_found = 0x40000000;
      const uint32_t mdpp_data_length_mask = 0x000003FF;
      const uint32_t module_setng_mask = 0x0000FC00;
      const uint32_t module_setng_div = 0x400;
      const uint32_t module_id_mask = 0x00FF0000;
      const uint32_t module_id_div = 0x10000;
      const uint32_t mdpp_data_mask = 0xF0000000;
      const uint32_t tgv_data_mask_hi = 0xFFFF0000;
      const uint32_t tgv_data_ready_mask = 0x00000004;
      const uint32_t tgv_data_mask_lo = 0x0000FFFF;
      const uint32_t madc_data_mask = 0xFF800000;
      const uint32_t mdpp_data = 0x10000000;
      const uint32_t madc_data = 0x04000000;
      const uint32_t channel_mask_mdpp16 = 0x000F0000;
      const uint32_t channel_mask_mdpp32 = 0x001F0000;
      const uint32_t channel_div = 0x0010000;
      const uint32_t channel_flag_mask_mdpp32 = 0x00600000;
      const uint32_t channel_flag_div_mdpp32 = 0x00200000;
      const uint32_t channel_flag_mask_mdpp16 = 0x00300000;
      const uint32_t channel_flag_div_mdpp16 = 0x00100000;
      const uint32_t mdpp_flags_mask = 0x0FC00000;
      const uint32_t mdpp_flags_div = 0x0040000;
      const uint32_t eoe_found_mask = 0xC0000000;
      const uint32_t eoe_event_counter_mask = 0x3FFFFFFF;
      const uint32_t data_mask = 0x0000FFFF;
      const uint32_t fill_word_found = 0x00000000;
      const uint32_t extended_ts_mask = 0xF0000000;
      const uint32_t extended_ts = 0x20000000;
      const uint32_t exts_friend_mask = 0xFFFF0000;
      const uint32_t exts_friend = 0x10000;
      const uint32_t vmmr_data_mask = 0xF0000000;
      const uint32_t vmmr_data_length_mask = 0x00000FFF;
      const uint32_t vmmr_data_adc = 0x10000000;
      const uint32_t vmmr_data_tdc = 0x30000000;
      const uint32_t vmmr_bus_mask = 0x0f000000;
      const uint32_t vmmr_bus_div = 0x01000000;
      const uint32_t vmmr_channel_mask = 0x00fff000;
      const uint32_t vmmr_channel_div = 0x00001000;
      const uint32_t vmmr_adc_mask = 0x00000fff;
      const uint32_t vmmr_tdc_mask = 0x0000ffff;
   };

   enum firmware_t
   {
      MDPP_SCP,
      MDPP_QDC,
      MDPP_CSI,
      MMR,
      TGV,
      START_READOUT,
      END_READOUT,
      MVLC_SCALER
   };

   struct end_of_buffer : public std::runtime_error
   {
      end_of_buffer(const std::string& what)
         : std::runtime_error(what)
      {}
   };

   bool is_vmmr_tdc_data(uint32_t DATA);
   bool is_vmmr_adc_data(uint32_t DATA);

   struct module
   {
      std::string name;
      uint8_t id;
      firmware_t firmware;
      uint32_t channel_mask,channel_flag_mask,channel_flag_div;
      uint32_t DATA;
      std::map<uint8_t,std::string> channel_map;

      static std::unordered_map<std::string,std::string> data_type_aliases;
      /// Change the name of an existing data type
      static void set_data_type_alias(const std::string& type, const std::string& alias)
      {
         data_type_aliases[type]=alias;
      }

      module() = default;
      module(const std::string& _name, uint8_t _id, uint8_t nchan, firmware_t F)
         : name{_name}, id{_id}, firmware{F}
      {
         if(nchan==16)
         {
            // mdpp-16
            channel_mask = data_flags::channel_mask_mdpp16;
            channel_flag_mask = data_flags::channel_flag_mask_mdpp16;
            channel_flag_div = data_flags::channel_flag_div_mdpp16;
         }
         else
         {
            // mdpp-32
            channel_mask = data_flags::channel_mask_mdpp32;
            channel_flag_mask = data_flags::channel_flag_mask_mdpp32;
            channel_flag_div = data_flags::channel_flag_div_mdpp32;
         }
      }
      void set_data_word(uint32_t data){ DATA = data; }
      uint8_t channel_number() const
      {
          if(firmware==MMR)
          {
              if(is_vmmr_adc_data(DATA)) return (DATA & data_flags::vmmr_channel_mask) / data_flags::vmmr_channel_div;
              return 0;
          }
          return (DATA & channel_mask) / data_flags::channel_div;
      }
      uint8_t bus_number() const
      {
          return (DATA & data_flags::vmmr_bus_mask) / data_flags::vmmr_bus_div;
      }

      unsigned int channel_data() const
      {
          if(firmware==MMR)
          {
              if(is_vmmr_adc_data(DATA)) return (DATA & data_flags::vmmr_adc_mask);
              if(is_vmmr_tdc_data(DATA)) return (DATA & data_flags::vmmr_tdc_mask);
          }
          return (DATA & data_flags::data_mask);
      }
      uint8_t channel_flags() const
      {
         // =0 : data is ADC or QDC_long
         // =1 : data is TDC
         // =3 : data is QDC_short
         // =2 : data is trigger time
          return (DATA & channel_flag_mask)/channel_flag_div;
      }
      void print_mdpp_data() const
      {
          if(firmware == MMR)
          {
//              printf("== VMMR-DATA ::  %s [%#04x]  bus_number = %02d   chan_number = %02d    %s = %5d\n",
//                         name.c_str(), id, bus_number(), channel_number(), data_type().c_str(), channel_data());
//              if(data_type()=="TDC") printf("TDC: %02d %5d\n",
//                        bus_number(), channel_data());
              if(data_type()=="ADC") printf("%02d %02d %5d\n",
                        bus_number(), channel_number(), channel_data());
          }
//          else
//          printf("== MDPP-DATA :: %s [%#04x]  chan_number = %02d    %s = %5d\n",
//                     name.c_str(), id, channel_number(), data_type().c_str(), channel_data());
      }
      std::string data_type() const
      {
         // =0 : data is ADC or QDC_long
         // =1 : data is TDC
         // =3 : data is QDC_short
         // =2 : data is trigger time

          if(firmware==MMR)
          {
              if(is_vmmr_adc_data(DATA)) return "ADC";
              else return "TDC";
          }
          switch(channel_flags())
         {
         case 0:
            return firmware==MDPP_QDC ? data_type_aliases["qdc_long"] : data_type_aliases["adc"];
         case 1:
            return data_type_aliases["tdc"];
         case 2:
            return data_type_aliases["trig"];
         case 3:
            return data_type_aliases["qdc_short"];
         }
         return "unknown";
      }

      std::map<uint8_t,std::string>& get_channel_map() { return channel_map; }

      /// Get name of detector associated with channel number
      std::string operator[](uint8_t nchan){ return channel_map[nchan]; }

      bool is_mdpp_module() const { return (firmware==MDPP_QDC) || (firmware==MDPP_SCP); }
      bool is_vmmr_module() const { return (firmware==MMR); }
      bool is_mvlc_scaler() const { return firmware==MVLC_SCALER; }
   };

   std::map<uint8_t, module> define_setup(std::vector<module>&& modules);
   uint32_t read_data_word(std::istream& data);
   uint32_t read_data_word(const uint8_t* data);
   bool is_event_header(uint32_t DATA);
   bool is_end_of_event(uint32_t DATA);
   bool is_end_of_event_tgv(uint32_t DATA);
   bool is_mdpp_data(uint32_t DATA);
   bool is_vmmr_data(uint32_t DATA);
   bool is_tgv_data(uint32_t DATA);
   bool is_fill_word(uint32_t DATA);
   bool is_extended_ts(uint32_t DATA);
   bool is_exts_friend(uint32_t DATA);
   void print_type(uint32_t DATA);
   std::string decode_type(uint32_t DATA);
   uint16_t length_of_data_mdpp(uint32_t DATA);
   uint16_t length_of_data_vmmr(uint32_t DATA);
   uint8_t module_id(uint32_t DATA);
   unsigned int module_setting(uint32_t DATA);
   void print_header(uint32_t DATA);
   unsigned int extended_timestamp(uint32_t DATA);
   unsigned int event_counter(uint32_t DATA);
   void print_eoe(uint32_t DATA);
   void print_ext_ts(uint32_t DATA);
}
#endif // MESYTEC_MODULE_H
