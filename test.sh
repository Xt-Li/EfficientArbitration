# e.g. bash test.sh 04 foo

PN=${1?Error: no partition given}
BN=${2:-foo}
# build EASY

touch ~/legup/llvm/lib/Transforms/easy/EASY.cpp
cd ~/legup/llvm/lib/Transforms/easy
make

# run c code
cd ~/legup/examples/partition/$BN/partition$PN
sed --in-place '/REPLICATE_PTHREAD_FUNCTIONS/d' config.tcl
make

# run pass
~/legup/llvm/Release+Asserts/bin/opt -load=EASY.so -EASY -S $BN.bc > sliced.ll

# generate input file
echo "set_parameter REPLICATE_PTHREAD_FUNCTIONS 1" >> config.tcl
make

# boogie run
cp op.bpl ~/EASY
cp $BN.ll ~/EASY/input.ll
cd ~/EASY
bash boogie_run.sh

cd ~/legup/examples/partition/$BN/partition$PN/
cp ~/EASY/output.ll $BN.ll
llvm-as $BN.ll
make Backend

