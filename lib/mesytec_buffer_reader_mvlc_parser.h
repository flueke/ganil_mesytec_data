#ifndef LIB_MESYTEC_BUFFER_READER_MVLC_PARSER_H
#define LIB_MESYTEC_BUFFER_READER_MVLC_PARSER_H

#include "mesytec-mvlc/mesytec-mvlc.h"
#include "mesytec_data.h"
#include "mesytec_experimental_setup.h"

namespace mesytec
{

class mvlc_parser_buffer_reader
{
    experimental_setup mesytec_setup;
    event mesy_event;
    module_data mod_data;
    uint32_t total_number_events_parsed = 0;
    mvlc::CrateConfig mvlcCrateConfig;
    mvlc::readout_parser::ReadoutParserCallbacks mvlcParserCallbacks;
    mvlc::readout_parser::ReadoutParserCounters mvlcParserCounters;
    mvlc::readout_parser::ReadoutParserState mvlcParserState;
    size_t inputBufferNumber = 0;

public:
    void reset()
    {
        // reset buffer reader to initial state, before reading any buffers
        mesy_event.clear();
        mod_data.clear();
        total_number_events_parsed = 0;
        mvlcParserCallbacks = {};
        mvlcParserCounters = {};
        mvlcParserState = {};
    }

    void read_crate_map(const std::string &map_file)
    {
        mesytec_setup.read_crate_map(map_file);
    }

    void read_mvlc_crateconfig(const std::string &conf_file)
    {
        mvlcCrateConfig = mesytec::mvlc::crate_config_from_yaml_file(conf_file);
    }

    void initialise_readout()
    {
        mvlcParserState = mvlc::readout_parser::make_readout_parser(mvlcCrateConfig.stacks);

        spdlog::info("readout structure parsed from mvlc_crateconfig.yaml:");
        for (size_t eventIndex=0; eventIndex<mvlcParserState.readoutStructure.size(); ++eventIndex)
        {
            const auto &eventStructure = mvlcParserState.readoutStructure[eventIndex];

            if (eventStructure.size())
            {
                spdlog::info("  eventIndex={}:", eventIndex);
                for (size_t moduleIndex=0; moduleIndex<eventStructure.size(); ++moduleIndex)
                {
                    const auto &moduleStructure = eventStructure[moduleIndex];

                    spdlog::info("    moduleIndex={}, prefixLen={}, hasDynamic={:5}, suffixLen={}, name={}",
                        moduleIndex, moduleStructure.prefixLen, moduleStructure.hasDynamic, moduleStructure.suffixLen,
                        moduleStructure.name);
                }
            }
        }

        using namespace std::placeholders;

        // Slightly annoying to use the member functions as callbacks: either
        // use bind(), global functions taking a context object to get back to
        // this object or lambdas.
        mvlcParserCallbacks =
        {
            std::bind(&mvlc_parser_buffer_reader::event_data_callback, this, _1, _2, _3, _4, _5),
            std::bind(&mvlc_parser_buffer_reader::system_event_callback, this, _1, _2, _3, _4)
        };
    }

    uint32_t get_total_events_parsed() const { return total_number_events_parsed; }
    mesytec::mvlc::readout_parser::ReadoutParserCounters get_mvlc_parser_counters() const
    {
        return mvlcParserCounters;
    }

    /**
     Used with a raw mvme data stream in order to sort and collate different module data
        i.e. in Narval receiver actor.

        When a complete event is ready the callback function F is called with the event and
        the description of the setup as
        arguments. Suitable signature for the callback function F is

        ~~~~{.cpp}
        void callback(mesytec::event&, mesytec::experimental_setup&);
        ~~~~

        (it can also of course be implemented with a lambda capture or a functor object).

        Straight after the call, the event will be deleted, so don't bother keeping a copy of a
        reference to it, any data must be treated/copied/moved in the callback function.

        Returns the number of complete collated events were parsed from the buffer, i.e. the number of times
        the callback function was called without throwing an exception.

        @param _buf pointer to the beginning of the buffer
        @param nbytes size of buffer in bytes
        @param F function to call each time a complete event is ready
        */

    using CallbackFunction = std::function<void (event &mesy_event, experimental_setup &mesy_setup)>;

    void event_data_callback(void *userContext, int crateIndex, int eventIndex,
                             const mvlc::readout_parser::ModuleData *moduleDataList,
                             unsigned moduleCount)
    {
        spdlog::trace("event_data_callback: userContext={}, crateIndex={}, eventIndex={}, moduleCount={}",
            fmt::ptr(userContext), crateIndex, eventIndex, moduleCount);

        CallbackFunction F = *reinterpret_cast<CallbackFunction *>(userContext);

        int tgvTimestampStartIndex = 2;
        int tgvTimestampStatusIndex = 1;

        for (unsigned moduleIndex = 0; moduleIndex < moduleCount; ++moduleIndex)
        {
            const auto &moduleData = moduleDataList[moduleIndex];

            if (moduleData.data.size>2) // not just a header+EoE, but also some data in between!
            {
                auto header = moduleData.data.data[0];

                auto moduleId = module_id(header);

                //std::cout << "got " << moduleData.data.size-1 << " data words for mod-id " << std::hex << std::showbase << (int)moduleId << std::dec << std::endl;
                if (!mesytec_setup.has_module(moduleId))
                {
                    const auto &moduleName = mvlcParserState.readoutStructure[eventIndex][moduleIndex].name;
                    spdlog::warn("event_data_callback: module '{}' (index={}) with id={:#04x} not present in experimental setup"
                                 ", data_len={}, data={:#010x}",
                        moduleName, moduleIndex, moduleId,
                        moduleData.data.size,
                        fmt::join(moduleData.data.data, moduleData.data.data+moduleData.data.size, ", "));
                    continue;
                }

                // pointer to current module being read out
                auto mod = &mesytec_setup.get_module(moduleId);

                // Special handling for TGV: data is not stored like other modules
                if (mod->is_tgv_module())
                {
                   spdlog::trace("event_data_callback:TGV: moduleData.data.size={}",moduleData.data.size);

                   // check status of TGV data
                   if(!(moduleData.data.data[tgvTimestampStatusIndex] & data_flags::tgv_data_ready_mask))
                   {
                      spdlog::warn("*** WARNING *** Got BAD TIMESTAMP from TGV (TGV NOT READY) *** WARNING *** status={:#10x}",moduleData.data.data[tgvTimestampStatusIndex]);
                   }
                   // get 3 centrum timestamp words from TGV data
                    mesy_event.tgv_ts_lo  = (moduleData.data.data[tgvTimestampStartIndex+0] & data_flags::tgv_data_mask_lo);
                    mesy_event.tgv_ts_mid = (moduleData.data.data[tgvTimestampStartIndex+1] & data_flags::tgv_data_mask_lo);
                    mesy_event.tgv_ts_hi  = (moduleData.data.data[tgvTimestampStartIndex+2] & data_flags::tgv_data_mask_lo);
                    spdlog::trace("event_data_callback:TGV: lo={} mid={} hi={}",mesy_event.tgv_ts_lo,mesy_event.tgv_ts_mid,mesy_event.tgv_ts_hi);

                    continue;
                }

                mod_data.set_header_word(header, mod->firmware); // also clears mod_data prior to setting the header word

                // process all the remaining non-header data words that are part of this modules readout
                for (size_t di=1; di<moduleData.data.size; ++di)
                {
                   // special treatment for MVLC Scaler fake modules
                   // all scalers are read out as a single block, we must check data words for headers
                   // corresponding to different scalers and change the current module
                   if(mod->is_mvlc_scaler())
                   {
                      if(is_module_header(moduleData.data.data[di]))
                      {
                         header = moduleData.data.data[di];
                         moduleId = module_id(header);
                         mod = &mesytec_setup.get_module(moduleId);
                         // add data from previous scaler to event
                         mesy_event.add_module_data(mod_data);
                         // initialise new scaler module data
                         mod_data.set_header_word(header, mod->firmware);
                         // skip to next data word
                         continue;
                      }
                   }
                   if(!is_end_of_event(moduleData.data.data[di])                                 // WARNING! 0xc..... end of event word is the last data word
                         && !(mod->is_mesytec_module() && is_fill_word(moduleData.data.data[di])) // WARNING2! for Mesytec modules fill words (0) may be included here!
                         )
                   {
                      mod_data.add_data(moduleData.data.data[di]);
                   }
                }

                mesy_event.add_module_data(mod_data);
            }
        }

        // wait until data from all readout stacks have been collated before calling callback function
        if(eventIndex+1 == mvlcParserState.readoutStructure.size())
        {
           F(mesy_event, mesytec_setup); // invoke the output callback
           mesy_event.clear();
           ++total_number_events_parsed;
        }
    }

    void system_event_callback(void *userContext, int crateIndex, const u32 *header, u32 size)
    {
        spdlog::trace("system_event_callback: userContext={}, ci={}, size={}, sysEventHeader={:#10x}, sysEventSize={}",
            fmt::ptr(userContext), crateIndex, size, *header, size);
        CallbackFunction F = *reinterpret_cast<CallbackFunction *>(userContext);
    };

    uint32_t read_buffer_collate_events(const uint8_t *_buf, size_t nbytes, CallbackFunction F)
    {
        assert(nbytes % 4 == 0); // the buffer should only contain 32-bit words

        const uint32_t *buf = reinterpret_cast<const uint32_t *>(_buf);
        const size_t bufWords = nbytes / 4;

        // very verbose buffer debug output
        //spdlog::trace(">>> incoming buffer:");
        //spdlog::trace("{:#010x}", fmt::join(buf, buf+bufWords, ", "));
        //spdlog::trace("<<< end of incoming buffer");

        spdlog::trace("read_buffer_collate_events: callback function is {}", fmt::ptr(&F));

        mvlcParserState.userContext = reinterpret_cast<void *>(&F);

        mesytec::mvlc::readout_parser::parse_readout_buffer(
            mvlcCrateConfig.connectionType,
            mvlcParserState,
            mvlcParserCallbacks,
            mvlcParserCounters,
            ++inputBufferNumber,
            buf, bufWords);

        return total_number_events_parsed;
    }
};

}

#endif // LIB_MESYTEC_BUFFER_READER_MVLC_PARSER_H
