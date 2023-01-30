DOXY_FILE=Doxyfile

if [[ -v MESYTEC_SOURCE ]]; then
echo $MESYTEC_SOURCE
else
MESYTEC_SOURCE=$PWD/..
echo $MESYTEC_SOURCE
fi

cp $MESYTEC_SOURCE/documentation/$DOXY_FILE ./
cp $MESYTEC_SOURCE/documentation/*.css ./
cp $MESYTEC_SOURCE/documentation/mesytec.png ./
cp -r $MESYTEC_SOURCE/lib ./
cp $MESYTEC_SOURCE/README.md ./
cp $MESYTEC_SOURCE/tests/example_analysis.cpp ./

find ./lib \( -name '*.in' -o -name '*.txt' \) -exec rm -rf {} +

doxygen $DOXY_FILE
