#include <string>
#include "../narval/zmq_compat.h"
#include <ctime>
#include <thread>
#include <chrono>
#include <iostream>
#include "boost/program_options.hpp"
#include "mesytec_buffer_reader.h"

zmq::context_t context(1);	// for ZeroMQ communications


namespace po = boost::program_options;

struct mfm_header_decoder
{
    using frame_size_t = uint32_t;
    using frame_type_t = uint16_t;
    using blob_size_t = uint32_t;
    frame_size_t frame_size;
    frame_type_t frame_type;
    blob_size_t blob_size;

    mfm_header_decoder(zmq::message_t& M)
    {
        // extract infos from message buffer, assumed to contain 1 MFMFrame
        // WARNING - endianness is not tested, it is assumed!

        if(M.size()<24){
            throw( std::runtime_error("message size < 24 bytes : not an MFM header ?") );
        }
        auto ev_dat = M.data<uint8_t>();
        frame_size = *((frame_size_t*)&ev_dat[1]) * 2;// size in 16-bit units
        assert(M.size() == frame_size);
        frame_type = *((frame_type_t*)&ev_dat[5]);
        if(frame_type != mesytec::mfm_frame_type){
            throw( std::runtime_error("not a Mesytec frame!") );
        }
        blob_size = *((blob_size_t*)&ev_dat[20]);
    }
};

int main(int argc, char *argv[])
{
    po::options_description desc("\nzmq_receiver\n\nUsage");

    desc.add_options()
            ("help", "produce this message")
            ("zmq_host", po::value<std::string>(), "url of host where mesytec_receiver_mfm_transmitter is runnning")
            ("zmq_port", po::value<int>(), "port on which to receive MFM data")
            ("run", po::value<int>(), "run number")
            ("filesize", po::value<int>(), "file size [MB] - default 1024 MB")
            ;

    po::variables_map vm;
    try
    {
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);
    }
    catch(...)
    {
        // in case of unknown options, print help & exit
        std::cout << desc << "\n";
        return 0;
    }

    if (vm.count("help")) {
        std::cout << desc << "\n";
        return 0;
    }

    if(!vm.count("zmq_port")||!vm.count("zmq_host")||!vm.count("run"))
    {
        std::cout << desc << "\n";
        return 0;
    }

    std::string zmq_port = "tcp://";
    std::string path_to_host = vm["zmq_host"].as<std::string>();
    int host_port;
    host_port = vm["zmq_port"].as<int>();
    zmq_port = zmq_port + path_to_host + ":" + std::to_string(host_port);

    auto run_number = vm["run"].as<int>();
    auto filesize = 1024;
    if(vm.count("filesize")) filesize = vm["filesize"].as<int>();

    // start zmq receiver here (probably)
    zmq::socket_t* pub{nullptr};
    try {
        pub = new zmq::socket_t(context, ZMQ_SUB);
    } catch (zmq::error_t &e) {
        std::cout << "[MESYTEC] : ERROR: " << "process_start: failed to start ZeroMQ event spy: " << e.what () << std::endl;
    }

    int timeout=100;//milliseconds
#ifdef ZMQ_SETSOCKOPT_DEPRECATED
    pub->set(zmq::sockopt::rcvtimeo,timeout);
#else
    pub->setsockopt(ZMQ_RCVTIMEO, &timeout, sizeof(int));
#endif
    try {
        pub->connect(zmq_port.c_str());
    } catch (zmq::error_t &e) {
        std::cout << "[MESYTEC] : ERROR" << "process_start: failed to bind ZeroMQ endpoint " << zmq_port << ": " << e.what () << std::endl;
    }
    std::cout << "[MESYTEC] : Connected to MESYTECSpy " << zmq_port << std::endl;
#ifdef ZMQ_SETSOCKOPT_DEPRECATED
   pub->set(zmq::sockopt::subscribe,"");
#else
    pub->setsockopt(ZMQ_SUBSCRIBE, "", 0);
#endif

    time_t current_time;
    time(&current_time);
    struct tm * timeinfo = localtime (&current_time);
    printf ("[MESYTEC] : MESYTEC-receiver beginning at: %s", asctime(timeinfo));

    uint32_t tot_events_parsed=0;
    uint32_t events_treated=0;
    zmq::message_t event;

    const int status_update_interval=5; // print infos every x seconds

    const uint32_t buffer_size = 1024*1024;
    uint8_t buffer[buffer_size];
    uint32_t buffer_used = 0;
    uint32_t frames_in_buffer = 0;

    std::ofstream output_file;
    int file_index=0;
    std::string file_name = "mesytec_run_" + std::to_string(run_number) + ".dat";
    uint64_t file_size = filesize*1024*1024;
    uint64_t file_used = 0;

    output_file.open(file_name, std::ios_base::out | std::ios_base::binary);

    /*** MAIN LOOP ***/
    while(1)
    {
        try{
#ifdef ZMQ_USE_RECV_WITH_REFERENCE
            if(!pub->recv(event))
#else
            if(!pub->recv(&event))
#endif
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                continue;
            }
        }
        catch(zmq::error_t &e) {
            std::cout << "[MESYTEC] : timeout on ZeroMQ endpoint: " << e.what () << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        ++tot_events_parsed;
        mfm_header_decoder decod(event);
        if(buffer_used+decod.frame_size > buffer_size)
        {
            // buffer is full - dump to disk
            if(file_used+buffer_used > file_size)
            {
                // current file full - close and open new file
                std::cout << "Closing file " << file_name << std::endl;
                output_file.close();
                ++file_index;
                file_name = "mesytec_run_" + std::to_string(run_number) + ".dat." + std::to_string(file_index);
                file_used = 0;
                output_file.clear();
                output_file.open(file_name, std::ios_base::out | std::ios_base::binary);
            }
            // file writing...
            output_file.write((const char*)buffer,buffer_used);
            file_used+=buffer_used;

            // reset to beginning of buffer
            buffer_used = 0;
            frames_in_buffer = 0;
        }
        // copy frame to buffer
        memcpy(buffer+buffer_used, event.data(), decod.frame_size);
        buffer_used += decod.frame_size;
        ++frames_in_buffer;

        time_t t;
        time(&t);
        double time_elapsed=difftime(t,current_time);
        if(time_elapsed>=status_update_interval)
        {
            // print infos every x seconds (defined in Merger.conf by 'Status_Update_Interval' line)
            current_time=t;
            struct tm * timeinfo = localtime (&current_time);
            std::string now = asctime(timeinfo);
            now.erase(now.size()-1);//remove new line character
            std::cout << "[MESYTEC] : " << now << " : parse rate " << tot_events_parsed/time_elapsed << " evt./sec...\n";
            tot_events_parsed=0;
        }
    }
}
