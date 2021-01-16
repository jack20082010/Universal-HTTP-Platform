cd shbin
cp -r * $HOME/shbin
cd ..

cd src/util
pwd
make clean
make install
cd ../..

cd src/dbmysql
pwd
make clean
make install
cd ../..


cd src/uhp
pwd
make clean
make install
cd ../..

cd src/sdk/seq_sdk
pwd
make clean
make install
cd ../../..

cd src/plugin/sequence/plugin_input
pwd
make clean
make install
cd ../../../..
pwd

cd src/plugin/sequence/plugin_output
pwd
make clean
make install
cd ../../../..
pwd



