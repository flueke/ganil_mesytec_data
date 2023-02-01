# mesytec_data
## mesytec data parsing library

Website and class reference documentation: https://mesytec-ganil.pages.in2p3.fr/mesytec_data/

### Build and install

~~~~
$ mkdir build
$ cd build
$ cmake [source_dir] -DCMAKE_INSTALL_PREFIX=[install_dir] -DCMAKE_BUILD_TYPE=[Release|RelWithDebInfo|Debug]
$ make -j[ncpu] install
~~~~

replacing [source_dir], [install_dir] and [ncpu] with appropriate values

The CMAKE_BUILD_TYPE determines the amount of optimisation of code: for production versions,
use Release or RelWithDebInfo for the fastest execution.

### Define VME configuration

Parsing data requires a file containing the definitions of modules in the VME crate,
and optionally a file with a correspondence between each bus/channel and detector.

#### Example crate_map.dat

~~~~
MDPP_0,0x20,16,SCP
MDDP_1,0x1,32,MDPP_QDC
MMR_1,0x10,8,VMMR
START_READOUT,0xab,32,START_READOUT
END_READOUT,0xcd,32,END_READOUT
~~~~

Each line has the format: `name, crate-address, number_channels, firmware`

`crate-address` is the HW address of the module: remember that Mesytec data only uses the top two nibbles of these
addresses, therefore make sure all modules have addresses such as `0xXY`, not `0x0XY` or `0x00XY`.

`number_channels` is the number of channels (16 or 32) for MDPP modules, number of buses for VMMR (each bus is assumed to have 128 subaddresses).

`firmware` can be:
   + for MDPP: SCP, QDC, CSI, MDPP_QDC, MDPP_SCP, MDPP_CSI
   + for VMMR: VMMR
   
Note the two dummy modules `START_READOUT` and `END_READOUT` which must be present.
These correspond to markers which have to be inserted at the beginning and end of the MVLC readout cycle
when configuring the setup with the `mvme` software.

#### Example detector_correspondence.dat

~~~~
0x10,2,1,PISTA_DE_2_1
0x10,0,64,PISTA_DE_0_64
0x20,0,PISTA_E_0
0x20,4,PISTA_E_4
0x20,5,PISTA_E_5
~~~~

Each line has the format: `crate-address, channel_number, name` (MDPP modules)
or `crate-address, bus_number, channel_number, name` (VMMR modules) 

### Data analysis

Buffers of data can be parsed with class `mesytec::buffer_reader`. See example_analysis.cpp.

### GANIL Acquisition Interface

#### MFM encapsulation
Run the `mesytec_receiver_mfm_transmitter` executable with suitable arguments in order to encapsulate data received by ZMQ
from the mesytec `mvme` software into MFM frames which are in turn published on a ZMQ PUB socket.

#### Narval receiver
`libzmq_narval_receiver.so` is a Narval actor which can receive the MFM frames produced by `mesytec_receiver_mfm_transmitter`
in order to inject them into a Narval dataflow. Give the specification of the ZMQ port (`tcp://hostname:port`) in the `algo_path`
option of the actor.
