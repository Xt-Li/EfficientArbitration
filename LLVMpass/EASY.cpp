// version: 2020-07-29
#include "EASY.h"
#include "llvm/Pass.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Use.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/AliasAnalysis.h"
//#include "../../IR/ConstantsContext.h"
//#include "llvm/IR/ConstantRange.h"
//#include "utils.h"
#include <vector>
#include <map>
#include <string>
#include <fstream>
//#include <sstream>
#include <iostream>
#include "llvm/Support/raw_ostream.h"

#include "Slice.h"
#include "CFSlice.h"
#include "ISlice.h"
#include "LoadRunaheadAnalysis.h"
#include "MemSlice.h"
#include "SliceArrayRefs.h"
#include "SliceCriterion.h"
#include "SliceHelpers.h"
#include "SliceMem.h"

#include "llvm/IR/Dominators.h"

using namespace llvm;

namespace EASY {
    /*
    bool printAPF = true;
    std::string FileError;
    raw_fd_ostream apf("partition.legup.rpt", FileError, llvm::sys::fs::F_None);
    std::string FileError2;
    raw_fd_ostream debugIRFile("IR.legup.ll", FileError2, llvm::sys::fs::F_None);
    */
    bool EASY::runOnModule(Module &M)
    {
        //initialization();
        Mod = &M;
        partition_flag = 0;
        bpl.open("op.bpl", std::fstream::out);            // 'fstream::out' write on file bpl
        errs() << "\nBegin Analysis: \n";

        // Step 0: Extract of function threads
        analyzeThreadInfo(M);
        // Step 1: Extraction of partitioned array
        findPartitionedArrays(M);
        // Step 2: Extraction of global memory accesses in threadfunction
        sliceThreadFunction(M);

        // Here is assumed that the input arguements of thread function are preprocessed to constant in IR code
        // Step 3: LLVM IR Code to Boogie Conversion
        interpretToBoogie(M);

        // Step 3.5: LLVM IR Code to C simulator
        //interpretToCSimulator(M);

        bpl.close();
        errs() << "\n---------------- Transform to Boogie Program Finished ----------------\n";

        return false;
    }

// -------------------------------------------------------step 0-----------------------------------------------------

    void EASY::analyzeThreadInfo(Module &M) {
        errs() << "\n*********************************************\n";
        errs() << "0. Thread Function Analysis...\n";
        errs() << "*********************************************\n\n";

        std::vector<Instruction *> call;

        // Thread function search
        for (auto F = M.begin(); F != M.end(); ++F) {                               // Function level
            for (auto BB = F->begin(); BB != F->end(); ++BB) {                      // Basic block level
                for (auto I = BB->begin(); I != BB->end(); ++I) {                   // Instruction level
                    if (isa<GetElementPtrInst>(I)) {
                        // getelementptrs - This is related to global array
                        if ((F->getName() != "main") && (std::find(threadFunc.begin(), threadFunc.end(), F) == threadFunc.end())) {
                            // main function would not be considered as no memory contention in single thread; F is not in threadFunc set
                            threadFunc.push_back(F);
                            threadNum.push_back(0);
                        }
                        //}

                    }
                    else if (isa<CallInst>(I)) {
                        call.push_back(I);  // collect all call instructions for thread counting
                    }
                }                                                               // End of instruction level analysis
            }                                                                   // End of basic block level analysis
        }                                                                       // End of function level analysis

                                                                                // Thread number counting
        int threadcount = -1;
        for (auto threadFunc_it = threadFunc.begin(); threadFunc_it != threadFunc.end(); ++threadFunc_it) {
            threadcount++;
            Function *threadFunc_temp = *threadFunc_it;
            for (auto call_it = call.begin(); call_it != call.end(); ++call_it)
            {
                Instruction *call_temp = *call_it;
                if (call_temp->getOperand(call_temp->getNumOperands() - 1) == threadFunc_temp)
                    // check if it is calling the thread fucntion
                    threadNum[threadcount]++;
            }    // end of instruction level

            errs() << "Function name: " << threadFunc_temp->getName() << "\n";
            errs() << "Number of threads: " << threadNum[threadcount] << "\n\n";


        }    // end of function level
        errs() << "Total number of multi-threaded functions: " << threadcount + 1 << "\n";
        // errs()<<threadFunc.size()<<"\n";
    }       // end of analyzeThreadInfo

// -------------------------------------------------------step 1-----------------------------------------------------

            // Print Boogie code header
    void EASY::printBoogieHeader(void) {
        bpl << "\n//*********************************************\n";
        bpl << "//    Boogie code generated from LLVM\n";
        bpl << "//*********************************************\n";

        bpl << "// Bit vector function prototypes\n";
        bpl << "// Arithmetic\n";
        bpl << "function {:bvbuiltin \"bvadd\"} bv32add(bv32,bv32) returns(bv32);\n";
        bpl << "function {:bvbuiltin \"bvsub\"} bv32sub(bv32,bv32) returns(bv32);\n";
        bpl << "function {:bvbuiltin \"bvmul\"} bv32mul(bv32,bv32) returns(bv32);\n";
        bpl << "function {:bvbuiltin \"bvudiv\"} bv32udiv(bv32,bv32) returns(bv32);\n";
        bpl << "function {:bvbuiltin \"bvurem\"} bv32urem(bv32,bv32) returns(bv32);\n";
        bpl << "function {:bvbuiltin \"bvsdiv\"} bv32sdiv(bv32,bv32) returns(bv32);\n";
        bpl << "function {:bvbuiltin \"bvsrem\"} bv32srem(bv32,bv32) returns(bv32);\n";
        bpl << "function {:bvbuiltin \"bvsmod\"} bv32smod(bv32,bv32) returns(bv32);\n";
        bpl << "function {:bvbuiltin \"bvneg\"} bv32neg(bv32) returns(bv32);\n";
        bpl << "// Bitwise operations\n";
        bpl << "function {:bvbuiltin \"bvand\"} bv32and(bv32,bv32) returns(bv32);\n";
        bpl << "function {:bvbuiltin \"bvor\"} bv32or(bv32,bv32) returns(bv32);\n";
        bpl << "function {:bvbuiltin \"bvnot\"} bv32not(bv32) returns(bv32);\n";
        bpl << "function {:bvbuiltin \"bvxor\"} bv32xor(bv32,bv32) returns(bv32);\n";
        bpl << "function {:bvbuiltin \"bvnand\"} bv32nand(bv32,bv32) returns(bv32);\n";
        bpl << "function {:bvbuiltin \"bvnor\"} bv32nor(bv32,bv32) returns(bv32);\n";
        bpl << "function {:bvbuiltin \"bvxnor\"} bv32xnor(bv32,bv32) returns(bv32);\n";
        bpl << "// Bit shifting\n";
        bpl << "function {:bvbuiltin \"bvshl\"} bv32shl(bv32,bv32) returns(bv32);\n";
        bpl << "function {:bvbuiltin \"bvlshr\"} bv32lshr(bv32,bv32) returns(bv32);\n";
        bpl << "function {:bvbuiltin \"bvashr\"} bv32ashr(bv32,bv32) returns(bv32);\n";
        bpl << "// Unsigned comparison\n";
        bpl << "function {:bvbuiltin \"bvult\"} bv32ult(bv32,bv32) returns(bool);\n";
        bpl << "function {:bvbuiltin \"bvule\"} bv32ule(bv32,bv32) returns(bool);\n";
        bpl << "function {:bvbuiltin \"bvugt\"} bv32ugt(bv32,bv32) returns(bool);\n";
        bpl << "function {:bvbuiltin \"bvuge\"} bv32uge(bv32,bv32) returns(bool);\n";
        bpl << "// Signed comparison\n";
        bpl << "function {:bvbuiltin \"bvslt\"} bv32slt(bv32,bv32) returns(bool);\n";
        bpl << "function {:bvbuiltin \"bvsle\"} bv32sle(bv32,bv32) returns(bool);\n";
        bpl << "function {:bvbuiltin \"bvsgt\"} bv32sgt(bv32,bv32) returns(bool);\n";
        bpl << "function {:bvbuiltin \"bvsge\"} bv32sge(bv32,bv32) returns(bool);\n\n";
    }

      // Collects and prints out statistics on partitioned arrays in the program
    void EASY::findPartitionedArrays(Module &M) {
        errs() << "\n*********************************************\n";
        errs() << "1. Finding Partitioned Arrays...\n";
        errs() << "*********************************************\n\n";

        printBoogieHeader();
        bpl << "// global array in the program:\n";

        // read partition.config e.g. "global - input_array b4|"
        std::ifstream cFile("partition.config");
        if (cFile.is_open()){
            std::string line;
            int countT = 0;
            int par_i;
            std:: string method;
            std:: string Pnumber;
            while (getline(cFile, line)){
                if (line.empty() || line[0] == '#'){
                    continue;
                }
                countT++;
                auto delimiterPos = line.find(" ");
                auto type = line.substr(0, delimiterPos);                                         // global
                auto delimiterPos2 = line.find(" ", delimiterPos + 3);                              // input_array b4| 
                auto name = line.substr(delimiterPos + 3, delimiterPos2 - delimiterPos - 3);        // input_array
                //auto method = line.substr(delimiterPos2 + 1, 1);                                  // block - b
                //auto Pnumber = line.substr(delimiterPos2 + 2, 1);                                     // 4

                // partition letters and numbers
                auto delimiterPos3 = line.find(" ", delimiterPos2 + 1);
                auto pstr = line. substr(delimiterPos2 + 1, delimiterPos3 - delimiterPos2 - 1);


                if (pstr[1] >= 'a')  //two letters
                {
                    // errs() << "two letters\n";
                    method = pstr.substr(0,2);
                    Pnumber = pstr.substr(2);
                }
                else   // one letter
                {
                    // errs() << "one letter\n";
                    method = pstr[0];
                    Pnumber = pstr.substr(1);
                } 


                partitionNum = stoi(Pnumber);                   

                errs() << "Partitioned array " << countT << ":\n";
                errs() << "    Type: " << type << "\n";
                errs() << "    Name: " << name << "\n";
                errs() << "    Method of partition: " << method << "\n";                // block; cyclic;
                errs() << "    Number of partitions: " << partitionNum << "\n\n";

                for (par_i = 0; par_i < partitionNum; par_i++){                   // write arrays into bpl
                 bpl<<"var @sub_"<< name << "_"<<par_i<<": [bv32]bv32;\n"; //array_size = 8192 
             }

         }
     }
     else {
        errs() << "Couldn't open config file for reading.\n";
    }
    }   // end of void findPartitionedArrays


// -------------------------------------------------------step 2-----------------------------------------------------

    void EASY::sliceThreadFunction(Module &M) {

        errs() << "\n*********************************************\n";
        errs() << "2. Program Slicing...\n";
        errs() << "*********************************************\n";
        int threadfunc_num = -1; //  threadfunc number
        std::vector<Instruction *> bankAddressInstrList;

        for (auto F = M.begin(); F != M.end(); ++F) {                               // Function level

            if ( std::find(threadFunc.begin(), threadFunc.end(), (Function *)F) != threadFunc.end())    // only slice global memory access in thread function
            {
                threadfunc_num++;
                bankAddressInstrList.clear();
                Slice *threadFuncSlice = new Slice((Function*)F);
                for (auto BB = F->begin(); BB != F->end(); ++BB) {                      // Basic block level
                    for (auto I = BB->begin(); I != BB->end(); ++I) {                   // Instruction level
                        if (I->getOpcode() == 50) {     // select instruction
                            auto backIcmp = I;
                            auto backIcmpBB = BB;
                            while (backIcmp != F->begin()->begin())   // instruction level
                            {
                                if (backIcmp->getOpcode() == 46 && dyn_cast<Instruction>(I->getOperand(0)) == backIcmp) //icmp
                                {
                                    auto backBankAddr = backIcmp;
                                    auto backBankAddrBB = backIcmpBB;
                                    while (backBankAddr != F->begin()->begin())
                                    {
                                        if (dyn_cast<Instruction>(backIcmp->getOperand(0)) == backBankAddr)
                                        {
                                            if (backBankAddr->getOpcode() == 22 || backBankAddr->getOpcode() == 23) //ashr or and
                                            {
                                                if (std::find(bankAddressInstrList.begin(), bankAddressInstrList.end(), backBankAddr) == bankAddressInstrList.end()){
                                                    bankAddressInstrList.push_back(backBankAddr);
                                                    errs()<<"1--------------\n\n";
                                                }
                                                
                                            }
                                        }
                                        if (backBankAddr == backBankAddrBB->begin())
                                        {
                                            backBankAddrBB--;
                                            backBankAddr = backBankAddrBB->end();
                                            backBankAddr--;
                                        }
                                        else
                                            backBankAddr--;
                                    }
                                }
                                if (backIcmp == backIcmpBB->begin())
                                {
                                    backIcmpBB--;
                                    backIcmp = backIcmpBB->end();
                                    backIcmp--;
                                }
                                else
                                    backIcmp--;
                            }
                            threadFuncSlice->addCriterion(I);   // added into critia
                        }   // end if == 50
                    }              // End of instruction level
                }                  // End of basic block level
                                
                ISlice *inversedThreadFuncSlice = new ISlice((Function*)F);
                inversedThreadFuncSlice->inverse(threadFuncSlice);
                removeSlice((Function *)F, inversedThreadFuncSlice);
                errs() << "=> Removed " << inversedThreadFuncSlice->instructionCount() << " instructions from function (" << F->getName() << ").\n";

                bankAddressInstrList2d.push_back(bankAddressInstrList);

            } // end of a threadfunc
        } //end of threadfunc loop
        errs() << bankAddressInstrList.size() << " -- size of  array\n";
        errs() << bankAddressInstrList2d.size() << " -- size of 2d array\n";
        errs() << bankAddressInstrList2d[0].size() << "--- size of [0].size\n";
        errs() << bankAddressInstrList2d[1].size() << "--- size of [1].size\n";
    }   // end of sliceThreadFunction

// -------------------------------------------------------step 3-----------------------------------------------------

   // Translate IR code to Boogie
    void EASY::interpretToBoogie(Module &M) {
        errs() << "\n*********************************************\n";
        errs() << "3. Converting LLVM IR Code to Boogie Code...\n";
        errs() << "*********************************************\n";
        
        bpl << "\n// Datatype conversion from bool to bv32\n";
        bpl << "procedure {:inline 1} bool2bv32 (i: bool) returns ( o: bv32)\n";
        bpl << "{\n";
        bpl << "\tif (i == true)\n";
        bpl << "\t{\n";
        bpl << "\t\to := 1bv32;\n";
        bpl << "\t}\n";
        bpl << "\telse\n";
        bpl << "\t{\n";
        bpl << "\t\to := 0bv32;\n";
        bpl << "\t}\n";
        bpl << "}\n";

        
        // search phi instructions for insertion
        anlyzePhiInstr(M);
        
        int threadfunc_count = -1;
        // get all pointer values (interprocedural)      
        for (auto F = M.begin(); F != M.end(); ++F) {    // Function level
            if (F->size() != 0)                          // ignore all empty functions
            {
                threadfunc_count++;
                if (F->getName() == "main")                
                {
                    errs()<<"check main\n\n";
                    bpl << "\n// For function: " << static_cast<std::string>((F->getName()));
                    // print function prototypes
                    printFunctionPrototype(F, &bpl, threadfunc_count);
                    // just print thread function information in main
                    printVarDeclarationsInMain(F, &bpl, threadNum);
                    bpl << "\n";
                    instrDecodingInMain(F, &bpl, threadNum);
                    bpl << "}\n";   // indicate end of function

                }
                else if ( (std::find(threadFunc.begin(), threadFunc.end(), (Function *)F) != threadFunc.end()))
                {
                    errs()<<"check threadfunc print function prototype\n\n";
                    bpl << "\n// For function: " << static_cast<std::string>((F->getName()));
                    // print function prototypes
                    printFunctionPrototype(F, &bpl, threadfunc_count);
                    // variable definitions
                    errs()<<"check threadfunc print var delcarations\n\n";
                    printVarDeclarations(F, &bpl);
                    bpl << "\n";
                    // decode all instructions
                    errs()<<"check threadfunc instrdecoding\n\n";
                    instrDecoding(F, &bpl, threadfunc_count);
                    bpl << "}\n";   // indicate end of function
                    
                }

                
                // errs() << "check1\n";
            }
            else
                errs() << "Function: " << static_cast<std::string>((F->getName())) << " is empty so ignored in Boogie\n";
        }             // End of function level analysis
        //}             // End of threadFunc loop                                             
        // errs() << "check\n";
        errs() << "\nTransfering to Boogie finished. \n";
    }   // end of interpretToBoogie

    // print argument of the function in prototype
    void EASY::printFunctionPrototype(Function *F, std::fstream *fout, int &count){
        if (static_cast<std::string>((F->getName())) == "main")
            // main function prototype
            *fout << "\nprocedure main() \n"; 
        else if ( (std::find(threadFunc.begin(), threadFunc.end(), (Function *)F) != threadFunc.end())) // ((Function *)F == threadFunc)
        {
            // other thread functions
            *fout << "\nprocedure {:inline 1} " << static_cast<std::string>((F->getName())) << "(";
            if (!(F->arg_empty()))
            {
                for (auto fi = F->arg_begin(); fi != F->arg_end(); ++fi)
                {
                    *fout << printRegNameInBoogie(fi) << ": bv32";
                    
                    auto fi_comma = fi;
                    fi_comma++;
                    if (fi_comma != F->arg_end())
                        *fout << ", ";
                }
            }
            // only returns the bank address information
            *fout << ") returns (";
            for (unsigned int i = 0; i < bankAddressInstrList2d[count].size(); i++)
            {
                *fout << "read_" << i << ": bool, index_" << i << ": bv32";
                if (i != bankAddressInstrList2d[count].size() - 1)
                    *fout << ", ";
            }
            *fout << ") \n";
        }
        else
        {
            // other thread functions
            *fout << "\nprocedure {:inline 1} " << static_cast<std::string>((F->getName())) << "(";
            if (!(F->arg_empty()))
            {
                for (auto fi = F->arg_begin(); fi != F->arg_end(); ++fi)
                    *fout << printRegNameInBoogie(fi) << ": bv32, ";
                
                // note: loop index can be more than one
                *fout << "store_input: bv32";
            }
            // only returns the bank address information
            *fout << ") returns (array_data: bv32) \n";
        }
        
        // add modifies - listed array name
        for (auto it = arrayInfo.begin(); it != arrayInfo.end(); ++it)
        {
            arrayNode *AN = *it;
            std::string arrayName;
            arrayName = AN->name;
            *fout << "modifies @" << arrayName << ";\n";
        }
        
        *fout << "{\n";
    } // end of printFunctionPrototype




    // This section is to collect all the variables have been used and prepare for declaration in Boogie
    void EASY::printVarDeclarationsInMain(Function *F, std::fstream *fout, std::vector<int>& threadNum )
    {

        int k = -1;

        for (unsigned int i = 0; i < threadNum.size(); i++){
            for (int m = 0; m < threadNum[i]; m++){
                k++;
                for (unsigned int j = 0; j < bankAddressInstrList2d[i].size(); j++)
                    *fout << "\tvar t" << k << "_read_" << j << ": bool" <<";\n\tvar t" << k << "_index_" << j << " :bv32;\n";

            }
            
        }
    }   //  end of printVarDeclarationsInMain
    
    // This section is to collect all the variables have been used and prepare for declaration in Boogie
    void EASY::printVarDeclarations(Function *F, std::fstream *fout)
    {
        for (auto BB = F->begin(); BB != F->end(); ++BB) {                      // Basic block level
            for (auto I = BB->begin(); I != BB->end(); ++I) {                   // Instruction level
                switch (I->getOpcode()) {
                    case 1:     // ret
                        // no need for var declarations
                    break;

                    case 2:     // br
                    if (I->getNumOperands() == 3)
                    {
                        if (!varFoundInList((Value *)(I->getOperand(0)), F))
                            varDeclaration((Value *)(I->getOperand(0)), fout, 0);
                    }
                    break;

                    case 8:     // add
                    if (!varFoundInList(I, F))
                        varDeclaration(I, fout, 0);
                    if (!varFoundInList((Value *)(I->getOperand(0)), F))
                        varDeclaration((Value *)(I->getOperand(0)), fout, 0);
                    if (!varFoundInList((Value *)(I->getOperand(1)), F))
                        varDeclaration((Value *)(I->getOperand(1)), fout, 0);
                    break;

                    case 10:     // sub
                    if (!varFoundInList(I, F))
                        varDeclaration(I, fout, 0);
                    if (!varFoundInList((Value *)(I->getOperand(0)), F))
                        varDeclaration((Value *)(I->getOperand(0)), fout, 0);
                    if (!varFoundInList((Value *)(I->getOperand(1)), F))
                        varDeclaration((Value *)(I->getOperand(1)), fout, 0);
                    break;
                    case 12:     // mul
                    if (!varFoundInList(I, F))
                        varDeclaration(I, fout, 0);
                    if (!varFoundInList((Value *)(I->getOperand(0)), F))
                        varDeclaration((Value *)(I->getOperand(0)), fout, 0);
                    if (!varFoundInList((Value *)(I->getOperand(1)), F))
                        varDeclaration((Value *)(I->getOperand(1)), fout, 0);
                    break;
                    case 20:     // shl
                    if (!varFoundInList(I, F))
                        varDeclaration(I, fout, 0);
                    if (!varFoundInList((Value *)(I->getOperand(0)), F))
                        varDeclaration((Value *)(I->getOperand(0)), fout, 0);
                    if (!varFoundInList((Value *)(I->getOperand(1)), F))
                        varDeclaration((Value *)(I->getOperand(1)), fout, 0);
                    break;
                    case 21:     // lshr
                    if (!varFoundInList(I, F))
                        varDeclaration(I, fout, 0);
                    if (!varFoundInList((Value *)(I->getOperand(0)), F))
                        varDeclaration((Value *)(I->getOperand(0)), fout, 0);
                    if (!varFoundInList((Value *)(I->getOperand(1)), F))
                        varDeclaration((Value *)(I->getOperand(1)), fout, 0);
                    break;
                    case 22:     // ashr
                    partition_flag = 1;
                    if (!varFoundInList(I, F))
                        varDeclaration(I, fout, 0);
                    if (!varFoundInList((Value *)(I->getOperand(0)), F))
                        varDeclaration((Value *)(I->getOperand(0)), fout, 0);
                    if (!varFoundInList((Value *)(I->getOperand(1)), F))
                        varDeclaration((Value *)(I->getOperand(1)), fout, 0);
                    break;
                    case 23:     // and
                    if (!varFoundInList(I, F))
                        varDeclaration(I, fout, 0);
                    if (!varFoundInList((Value *)(I->getOperand(0)), F))
                        varDeclaration((Value *)(I->getOperand(0)), fout, 0);
                    if (!varFoundInList((Value *)(I->getOperand(1)), F))
                        varDeclaration((Value *)(I->getOperand(1)), fout, 0);
                    break;
                    case 24:     // or
                    if (!varFoundInList(I, F))
                        varDeclaration(I, fout, 0);
                    if (!varFoundInList((Value *)(I->getOperand(0)), F))
                        varDeclaration((Value *)(I->getOperand(0)), fout, 0);
                    if (!varFoundInList((Value *)(I->getOperand(1)), F))
                        varDeclaration((Value *)(I->getOperand(1)), fout, 0);
                    break;
                    
                    case 27:     // load
                    if (!varFoundInList(I, F))
                        varDeclaration(I, fout, 0);
                    if (!varFoundInList((Value *)(I->getOperand(0)), F))
                        varDeclaration((Value *)(I->getOperand(0)), fout, 0);
                    break;

                    case 28:     // store
                    if (!varFoundInList((Value *)(I->getOperand(0)), F))
                        varDeclaration((Value *)(I->getOperand(0)), fout, 0);
                    if (!varFoundInList((Value *)(I->getOperand(1)), F))
                        varDeclaration((Value *)(I->getOperand(1)), fout, 0);
                    break;
                    
                    case 29:     // getelementptr                               // this is what we want to modify
                    if (!varFoundInList(I, F))
                            varDeclaration(I, fout, 0);     // leave output random
                        break;
                        
                    case 33:     // load
                    if (!varFoundInList(I, F))
                        varDeclaration(I, fout, 0);
                    if (!varFoundInList((Value *)(I->getOperand(0)), F))
                        varDeclaration((Value *)(I->getOperand(0)), fout, 0);
                    break;

                    case 34:     // zext
                    if (!varFoundInList(I, F))
                        varDeclaration(I, fout, 0);
                    if (!varFoundInList((Value *)(I->getOperand(0)), F))
                        varDeclaration((Value *)(I->getOperand(0)), fout, 0);
                    break;

                    case 46:    // icmp
                    if (!varFoundInList(I, F))
                        varDeclaration(I, fout, 1);
                    if (!varFoundInList((Value *)(I->getOperand(0)), F))
                        varDeclaration((Value *)(I->getOperand(0)), fout, 0);
                    if (!varFoundInList((Value *)(I->getOperand(1)), F))
                        varDeclaration((Value *)(I->getOperand(1)), fout, 0);
                    break;
                    case 48:     // phi
                    if (!varFoundInList(I, F))
                        varDeclaration(I, fout, 0);
                    if (!varFoundInList((Value *)(I->getOperand(0)), F))
                        varDeclaration((Value *)(I->getOperand(0)), fout, 0);
                    if (!varFoundInList((Value *)(I->getOperand(1)), F))
                        varDeclaration((Value *)(I->getOperand(1)), fout, 0);
                    break;
                    case 49:     // call
                    if (I->getNumOperands() > 1)
                    {
                        for (unsigned int i = 0; i != (I->getNumOperands()- 1); ++i)
                        {
                            if (!varFoundInList((Value *)(I->getOperand(i)), F))
                                varDeclaration((Value *)(I->getOperand(i)), fout, 0);
                        }
                    }
                    break;
                    case 50:     // select
                    partition_flag = 1;
                    if (!varFoundInList(I, F))
                        varDeclaration(I, fout, 1);
                    if (!varFoundInList((Value *)(I->getOperand(0)), F))
                        varDeclaration((Value *)(I->getOperand(0)), fout, 0);
                    if (!varFoundInList((Value *)(I->getOperand(1)), F))
                        varDeclaration((Value *)(I->getOperand(1)), fout, 0);
                    if (!varFoundInList((Value *)(I->getOperand(2)), F))
                        varDeclaration((Value *)(I->getOperand(2)), fout, 0);
                    break;

                    default:
                    ;
                }
                
            }                                                               // End of instruction level analysis
        }                                                                   // End of basic block level analysis
    }
    
    // To split phi instruction to several load in corresponding basic blocks
    void EASY::anlyzePhiInstr(Module &M)
    {
        for (auto F = M.begin(); F != M.end(); ++F) {   // Function level
            for (auto BB = F->begin(); BB != F->end(); ++BB) {  // Basic block level
                for (auto I = BB->begin(); I != BB->end(); ++I) {   // Instruction level
                    if (I->getOpcode() == 48)       // phi instruction
                    {
                        if (llvm::PHINode *phiInst = dyn_cast<llvm::PHINode>(&*I)) {
                            phiNode *phiTfInst = new phiNode [phiInst->getNumIncomingValues()];
                            
                            for (unsigned int it = 0; it < phiInst->getNumIncomingValues(); ++it)
                            {
                                phiTfInst[it].res = printRegNameInBoogie(I);
                                phiTfInst[it].bb = phiInst->getIncomingBlock(it);
                                phiTfInst[it].ip = phiInst->getIncomingValue(it);
                                phiTfInst[it].instr = phiInst;
                                
                                phiList.push_back(&phiTfInst[it]);
                            } // end for
                        }  // end if
                    } // end if
                }  // End of instruction level
            }   // End of basic block level
        }   // End of function level
    }   // end of anlyzePhiInstr
    
    // Call function printing in main functions
    void EASY::instrDecodingInMain(Function *F, std::fstream *fout, std::vector<int>& threadNum)
    {
        errs() << "instr decoding in main ...\n";
        int t_i = 0;
        int func_i = -1;
        // print call instructions
        for (auto threadFunc_it = threadFunc.begin(); threadFunc_it != threadFunc.end(); ++threadFunc_it)
        {

            Function *threadFunc_temp = *threadFunc_it;
            func_i++;

        for (auto BB = F->begin(); BB != F->end(); ++BB) {                      // Basic block level
            for (auto I = BB->begin(); I != BB->end(); ++I) {                   // Instruction level
                if (I->getOpcode() == 49) {
                    if (I->getOperand(I->getNumOperands()-1) == threadFunc_temp)
                    {
                        *fout << "\t// thread function call " << t_i << ": \n";
                        *fout << "\tcall ";
                      
                            for (unsigned int idxit = 0; idxit < bankAddressInstrList2d[func_i].size(); idxit++)
                            {
                                *fout << "t" << t_i << "_read_" << idxit <<", t" << t_i << "_index_" << idxit;
                                if (idxit != bankAddressInstrList2d[func_i].size() - 1)
                                    *fout << ", ";
                            }
                            *fout <<" := " << static_cast<std::string>((I->getOperand(I->getNumOperands()-1))->getName()) << "(";
                            if (I->getNumOperands() > 1)
                            {
                                *fout << printRegNameInBoogie((Value *)I->getOperand(0));
                                for (unsigned int i = 1; i != (I->getNumOperands()- 1); ++i)
                                    *fout << "," << printRegNameInBoogie((Value *)I->getOperand(i));
                            }
                            *fout << ");\n";
                        
                        
                        t_i++;
                    }   // end if
                }   // end if
            }      // End of instruction level
        }// End of basic block level
    } // End of thread func loop
 
    int k = -1;

    for (unsigned int i = 0; i < threadNum.size(); i++){
        for (int n = 0; n < threadNum[i]; n++){
            k++;
            for (unsigned int m = 0; m < bankAddressInstrList2d[i].size(); m++)
            {
                for (int j = 0; j < partitionNum; j++) 
                    // k: threadnum; m: bankAddress of a threadfunc; j: partitionNum
                    *fout << "\tassert !t" << k << "_read_" << m << " || t" << k << "_index_" << m << " != " << j << "bv32;\n";
            }
        }
    }

    *fout << "\n\treturn;\n";
    }   // end of instrDecodingInMain
    
    
    // Translate all IR instruction to Boogie instruction
    void EASY::instrDecoding(Function *F, std::fstream *fout, int &count)
    {
        // get all loop information
        llvm::DominatorTree* DT = new llvm::DominatorTree();
        DT->recalculate(*F);
        //generate the LoopInfoBase for the current function
        llvm::LoopInfoBase<llvm::BasicBlock, llvm::Loop>* KLoop = new llvm::LoopInfoBase<llvm::BasicBlock, llvm::Loop>();
        KLoop->releaseMemory();
        KLoop->Analyze(*DT);
        for (unsigned int i = 0; i < bankAddressInstrList2d[count].size(); i++)
        {
            *fout << "\tread_" << i << " := false;\n";
        }
        
        int indexCounter = 0;
        for (auto BB = F->begin(); BB != F->end(); ++BB) {                      // Basic block level
            // *fout << "// For basic block: " << BB->getName() << "\n";
            *fout << "\n\t// For basic block: bb_" << getBlockLabel(BB) << "\n";
            *fout << "\tbb_" << getBlockLabel(BB) << ":\n";
            // errs() << "debug: " << *BB << "\n";
            // Here add assertion of loop invariant conditions at start of the loop
            if(KLoop->isLoopHeader(BB))
            {
                // phiNode *phiInst;
                // unsigned int idxNameAddr;
                if (BasicBlock *BBExit = KLoop->getLoopFor(BB)->getExitingBlock())
                {
                    Instruction *exitCond;
                    Instruction *startCond;
                    Instruction *indVarBehaviour;
                    std::string endBound;
                    std::string startBound;
                    std::string indvarName;
                    std::string endSign;
                    std::string startSign;
                    std::string loopInverseCheck;   // check if exitcondition inversed
                    bool equalSign = 0;

                    auto instrexit = BBExit->end();
                    --instrexit;
                    
                    if (printRegNameInBoogie((Value *)instrexit->getOperand(0)) != "undef")
                    {
                        // instruction contains end loop index
                        for (auto exit_i = BBExit->begin(); exit_i != BBExit->end(); ++exit_i)
                        {
                            if (printRegNameInBoogie(exit_i) == printRegNameInBoogie(instrexit->getOperand(0)))
                                exitCond = exit_i;
                        }
                        endBound = printRegNameInBoogie(exitCond->getOperand(1));
                        
                        // check exit equality
                        loopInverseCheck = printRegNameInBoogie(exitCond);
                        loopInverseCheck.resize(9);
                        if (loopInverseCheck == "%exitcond")
                        {
                            if (CmpInst *cmpInst = dyn_cast<CmpInst>(&*exitCond)) {
                                if (cmpInst->getPredicate() == CmpInst::ICMP_NE)
                                    equalSign = 1;
                                else if (cmpInst->getPredicate() == CmpInst::ICMP_UGT)
                                    equalSign = 1;
                                else if (cmpInst->getPredicate() == CmpInst::ICMP_ULT)
                                    equalSign = 1;
                                else if (cmpInst->getPredicate() == CmpInst::ICMP_SGT)
                                    equalSign = 1;
                                else if (cmpInst->getPredicate() == CmpInst::ICMP_SLT)
                                    equalSign = 1;
                            }
                        }
                        else
                        {
                            if (CmpInst *cmpInst = dyn_cast<CmpInst>(&*exitCond)) {
                                if (cmpInst->getPredicate() == CmpInst::ICMP_EQ)
                                    equalSign = 1;
                                else if (cmpInst->getPredicate() == CmpInst::ICMP_UGE)
                                    equalSign = 1;
                                else if (cmpInst->getPredicate() == CmpInst::ICMP_ULE)
                                    equalSign = 1;
                                else if (cmpInst->getPredicate() == CmpInst::ICMP_SGE)
                                    equalSign = 1;
                                else if (cmpInst->getPredicate() == CmpInst::ICMP_SLE)
                                    equalSign = 1;
                            }
                        }
                        
                        // instruction contains start loop index
                        // this will not hold for matrix transfer... where exit condition not compatible...
                        
                        for (auto invarI = BBExit->begin(); invarI != BBExit->end(); ++invarI)
                        {
                            if (invarI->getOpcode() != 2)   // label name and variable name can be same causing bugs...
                            {
                                for (auto invarIt = phiList.begin(); invarIt != phiList.end(); ++invarIt)
                                {
                                    phiNode *phiTfInst = *invarIt;
                                    for (unsigned int it = 0; it < invarI->getNumOperands(); ++it)
                                    {
                                        if (phiTfInst->res == printRegNameInBoogie((Value *)invarI->getOperand(it)))
                                        {
                                            if (invarI->getOpcode() == 8 || invarI->getOpcode() == 10)
                                            {
                                                indVarBehaviour = invarI;
                                                startCond = dyn_cast<Instruction>(invarI->getOperand(it));
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        
                        if (indVarBehaviour->getOpcode() == 8)    // add
                        {
                            startSign = "bv32sge(";
                            if (equalSign)
                                endSign = "bv32sle(";
                            else
                                endSign = "bv32slt(";
                        }
                        else if (indVarBehaviour->getOpcode() == 10)  // sub
                        {
                            startSign = "bv32sle(";
                            if (equalSign)
                                endSign = "bv32sge(";
                            else
                                endSign = "bv32sgt(";
                        }
                        else
                            errs() <<"Undefined index behaviour: "<< *indVarBehaviour << "\n";
                        
                        
                        indvarName = printRegNameInBoogie(startCond);
                        
                        // start and exit sign
                        if (llvm::PHINode *phiInst = dyn_cast<llvm::PHINode>(&*startCond)) {
                            for (unsigned int it = 0; it < phiInst->getNumIncomingValues(); ++it)
                            {
                                if (ConstantInt *constVar = dyn_cast<ConstantInt>(phiInst->getIncomingValue(it)))
                                    startBound = printRegNameInBoogie(constVar);
                            } // end for
                        }  // end if
                        
                        // construct loop invariant string
                        invariance *currInvar = new invariance;
                        currInvar->loop = KLoop->getLoopFor(BB);
                        currInvar->indVarChange = indVarBehaviour;
                        currInvar->invar = startSign + indvarName + "," + startBound + ") && " + endSign + indvarName + "," + endBound + ")";
                        // debug
                        errs() << "Found loop invariant condition: (" << currInvar->invar << ")\n\n";
                        invarList.push_back(currInvar);
                        
                        // print
                        *fout << "\tassert ( " << currInvar->invar << ");\n";
                        *fout << "\thavoc " << indvarName << ";\n";
                        *fout << "\tassume ( " << currInvar->invar << ");\n";
                    }   // end of check undef
                }
                else
                {
                    // mark - this is the case of while(1)
                    // Due to unfinished automated loop invariants generation, this section will be filled in the future updates
                    errs() << "Found infintite loop entry: " << *BB << "\n";
                    errs() << "Found infintite loop: " << *(KLoop->getLoopFor(BB)) << "\n";
                    
                }
                
            }   // end of insert loop invariants in the beginning
            
            // Here add assertion of loop invariant conditions right before loop index change
            BasicBlock *currBB = BB;
            Instruction *endLoopCheckPoint;
            std::string endLoopInvar;
            int dim = 0;
            int search;
            if(KLoop->getLoopFor(BB))
            {
                if (currBB == KLoop->getLoopFor(BB)->getExitingBlock())
                {
                    auto instrexit = currBB->end();
                    --instrexit;
                    Instruction *instrExit = instrexit;
                    if (printRegNameInBoogie((Value *)instrExit->getOperand(0)) != "undef")
                    {
                        // errs() << "loop exit: " << *BB << "\n";
                        search = 0;
                        for (auto it = invarList.begin(); it != invarList.end(); ++it)
                        {
                            invariance *currInvar = *it;
                            if (KLoop->getLoopFor(BB) == currInvar->loop)
                            {
                                search++;
                                endLoopCheckPoint = currInvar->indVarChange;
                                endLoopInvar = currInvar->invar;
                            }
                        }
                        if (search != 1)
                            errs() << "Error: Loop invariant errors at end of the loop - " << search << "conditions matched.\n";
                    }
                }
            }
            
            
            // Start instruction printing
            for (auto I = BB->begin(); I != BB->end(); ++I) {                   // Instruction level
                // start printing end loop assertion here
                if ((Instruction *)I == endLoopCheckPoint)
                {
                    errs() << "Matched loop invariant condition " << search << ": (" << endLoopInvar << ")\n\n";
                    *fout << "\tassert ( " << endLoopInvar << ");\n";
                    // skip current basic block
                    I = BB->end();
                    --I;
                    // *fout << "\treturn;\n\tassume false;\n";
                }
                
                switch (I->getOpcode()) {
                    case 1:     // ret
                        *fout << "\treturn;\n";  //default return - memory not accessed.
                        break;
                        
                    case 2:     // br
                        // do phi resolution here
                    for (auto it = phiList.begin(); it != phiList.end(); ++it)
                    {
                        phiNode *phiTfInst = *it;
                        if (phiTfInst->bb == BB)
                            *fout << "\t" << phiTfInst->res << " := " << printRegNameInBoogie(phiTfInst->ip) << ";\n";
                    }

                        // add branch instruction here
                    if (endLoopCheckPoint->getParent() == I->getParent()) {
                            // typically for loop jumping back to iteration
                            // show original in comments
                            if (I->getNumOperands() == 1) // if (inst->isConditional())?
                                errs() << "Error: Mistaken a simple br as a loop exit condition check: " << *I << "\n";
                            else if (I->getNumOperands() == 3)
                                *fout << "//\tif(" << printRegNameInBoogie((Value *)I->getOperand(0)) << " == 1bv32) {goto bb_"<< getBlockLabel((BasicBlock *)(I->getOperand(2))) << ";} else {goto bb_" << getBlockLabel((BasicBlock *)(I->getOperand(1))) << ";}\n";
                            else
                                errs() << "Error: Instruction decoding error at br instruction: " << *I << "\n";
                            
                            *fout << "\tgoto bb_"<< getBlockLabel((BasicBlock *)(I->getOperand(2))) << ";\n";
                        }
                        else
                        {   // normal cases
                            if (I->getNumOperands() == 1) // if (inst->isConditional())?
                                *fout << "\tgoto bb_"<< getBlockLabel((BasicBlock *)(I->getOperand(0))) << ";\n";
                            else if (I->getNumOperands() == 3)
                                *fout << "\tif(" << printRegNameInBoogie((Value *)I->getOperand(0)) << " == 1bv32) {goto bb_"<< getBlockLabel((BasicBlock *)(I->getOperand(2))) << ";} else {goto bb_" << getBlockLabel((BasicBlock *)(I->getOperand(1))) << ";}\n";
                            else
                                errs() << "Error: Instruction decoding error at br instruction: " << *I << "\n";
                        }
                        break;
                        
                    case 8:     // add
                    if (OverflowingBinaryOperator *op = dyn_cast<OverflowingBinaryOperator>(I)) {
                        if ((op->hasNoUnsignedWrap()) && (op->hasNoSignedWrap()))
                                // has both nuw and nsw
                                *fout << "\t" << printRegNameInBoogie(I) << " := bv32add("<< printRegNameInBoogie((Value *)I->getOperand(0)) << ", " << printRegNameInBoogie((Value *)I->getOperand(1)) << ");\n";       // ---might has problems...
                            else if (op->hasNoUnsignedWrap())
                                // only nuw
                                *fout << "\t" << printRegNameInBoogie(I) << " := bv32add("<< printRegNameInBoogie((Value *)I->getOperand(0)) << ", " << printRegNameInBoogie((Value *)I->getOperand(1)) << ");\n";       // ---might has problems...
                            else if (op->hasNoSignedWrap())
                                // only nsw
                                *fout << "\t" << printRegNameInBoogie(I) << " := bv32add("<< printRegNameInBoogie((Value *)I->getOperand(0)) << ", " << printRegNameInBoogie((Value *)I->getOperand(1)) << ");\n";       // ---might has problems...
                            else
                                // normal add
                                *fout << "\t" << printRegNameInBoogie(I) << " := bv32add("<< printRegNameInBoogie((Value *)I->getOperand(0)) << ", " << printRegNameInBoogie((Value *)I->getOperand(1)) << ");\n";
                        }
                        else
                            errs() << "Error: Instruction decoding error at add instruction: " << *I << "\n";
                        break;
                        
                    case 10:     // sub
                    if (OverflowingBinaryOperator *op = dyn_cast<OverflowingBinaryOperator>(I)) {
                        if ((op->hasNoUnsignedWrap()) && (op->hasNoSignedWrap()))
                                // has both nuw and nsw
                                *fout << "\t" << printRegNameInBoogie(I) << " := bv32sub("<< printRegNameInBoogie((Value *)I->getOperand(0)) << ", " << printRegNameInBoogie((Value *)I->getOperand(1)) << ");\n";       // ---might has problems...
                            else if (op->hasNoUnsignedWrap())
                                // only nuw
                                *fout << "\t" << printRegNameInBoogie(I) << " := bv32sub("<< printRegNameInBoogie((Value *)I->getOperand(0)) << ", " << printRegNameInBoogie((Value *)I->getOperand(1)) << ");\n";       // ---might has problems...
                            else if (op->hasNoSignedWrap())
                                // only nsw
                                *fout << "\t" << printRegNameInBoogie(I) << " := bv32sub("<< printRegNameInBoogie((Value *)I->getOperand(0)) << ", " << printRegNameInBoogie((Value *)I->getOperand(1)) << ");\n";       // ---might has problems...
                            else
                                // normal add
                                *fout << "\t" << printRegNameInBoogie(I) << " := bv32sub("<< printRegNameInBoogie((Value *)I->getOperand(0)) << ", " << printRegNameInBoogie((Value *)I->getOperand(1)) << ");\n";
                        }
                        else
                            errs() << "Error: Instruction decoding error at sub instruction: " << *I << "\n";
                        break;
                        
                    case 12:     // mul
                    *fout << "\t" << printRegNameInBoogie(I) << " := bv32mul("<< printRegNameInBoogie((Value *)I->getOperand(0)) << ", " << printRegNameInBoogie((Value *)I->getOperand(1)) << ");\n";
                    break;
                    case 20:     // shl
                    *fout << "\t" << printRegNameInBoogie(I) << " := bv32shl("<< printRegNameInBoogie((Value *)I->getOperand(0)) << ", " << printRegNameInBoogie((Value *)I->getOperand(1)) << ");\n";
                    break;
                    case 21:     // lshr
                    *fout << "\t" << printRegNameInBoogie(I) << " := bv32lshr("<< printRegNameInBoogie((Value *)I->getOperand(0)) << ", " << printRegNameInBoogie((Value *)I->getOperand(1)) << ");\n";
                    break;

                    case 22:     // ashr
                    *fout << "\t" << printRegNameInBoogie(I) << " := bv32ashr("<< printRegNameInBoogie((Value *)I->getOperand(0)) << ", " << printRegNameInBoogie((Value *)I->getOperand(1)) << ");\n";
                    break;
                    case 23:     // and
                    *fout << "\t" << printRegNameInBoogie(I) << " := bv32and("<< printRegNameInBoogie((Value *)I->getOperand(0)) << ", " << printRegNameInBoogie((Value *)I->getOperand(1)) << ");\n";
                    break;
                    case 24:     // or
                    *fout << "\t" << printRegNameInBoogie(I) << " := bv32or("<< printRegNameInBoogie((Value *)I->getOperand(0)) << ", " << printRegNameInBoogie((Value *)I->getOperand(1)) << ");\n";
                    break;
                    case 27:     // load

                    if (isa<GetElementPtrInst>((Instruction *)I->getOperand(0)))
                    {
                        for (auto ait = arrayInfo.begin(); ait != arrayInfo.end(); ++ait)
                            {   // get dimension
                                arrayNode *AN = *ait;
                                std::vector<uint64_t> size = AN->size;
                                GlobalVariable *addr = AN->addr;
                                
                                if (((Instruction *)I->getOperand(0))->getOperand(0) == addr)
                                {
                                    dim = size.size();
                                    break;
                                }
                            }
                            
                            if (dim == 0)
                                errs() << "Error: unknown array stored - cannot find size: " << *I << "\n";
                            else if (dim == 1)
                                *fout << "\t" << printRegNameInBoogie((Value *)I) << " := " << printRegNameInBoogie((Value *)((Instruction *)I->getOperand(0))->getOperand(0)) << "[" << printRegNameInBoogie((Value *)((Instruction *)I->getOperand(0))->getOperand(2)) << "];\n";
                            else if (dim == 2)
                                *fout << "\t" << printRegNameInBoogie((Value *)I) << " := " << printRegNameInBoogie((Value *)((Instruction *)I->getOperand(0))->getOperand(0)) << "[" << printRegNameInBoogie((Value *)((Instruction *)I->getOperand(0))->getOperand(1)) << "][" << printRegNameInBoogie((Value *)((Instruction *)I->getOperand(0))->getOperand(2)) << "];\n";
                            else
                                errs() << "Error: 2+ dimension array accessed - not compatible yet: " << *I << "\n";
                        }
                        else
                            *fout << "\t" << printRegNameInBoogie(I) << " := "<< printRegNameInBoogie((Value *)I->getOperand(0)) << ";\n";
                        
                        break;
                        
                    case 28:    // store
                    if (isa<GetElementPtrInst>((Instruction *)I->getOperand(1)))
                    {
                        for (auto ait = arrayInfo.begin(); ait != arrayInfo.end(); ++ait)
                            {   // get dimension
                                arrayNode *AN = *ait;
                                std::vector<uint64_t> size = AN->size;
                                GlobalVariable *addr = AN->addr;
                                
                                if (((Instruction *)I->getOperand(1))->getOperand(0) == addr)
                                {
                                    dim = size.size();
                                    break;
                                }
                            }
                            
                            if (dim == 0)
                                errs() << "Error: unknown array stored - cannot find size: " << *I << "\n";
                            else if (dim == 1)
                                *fout << "\t" << printRegNameInBoogie((Value *)((Instruction *)I->getOperand(1))->getOperand(0)) << "[" << printRegNameInBoogie((Value *)((Instruction *)I->getOperand(1))->getOperand(2)) << "] := " << printRegNameInBoogie((Value *)I->getOperand(0)) << ";\n";
                            else if (dim == 2)
                                *fout << "\t" << printRegNameInBoogie((Value *)((Instruction *)I->getOperand(1))->getOperand(0)) << "[" << printRegNameInBoogie((Value *)((Instruction *)I->getOperand(1))->getOperand(1)) << "][" << printRegNameInBoogie((Value *)((Instruction *)I->getOperand(1))->getOperand(2)) << "] := " << printRegNameInBoogie((Value *)I->getOperand(0)) << ";\n";
                            else
                                errs() << "Error: 2+ dimension array accessed - not compatible yet: " << *I << "\n";
                        }
                        else
                            *fout << "\t" << printRegNameInBoogie((Value *)I->getOperand(1)) << " := "<< printRegNameInBoogie((Value *)I->getOperand(0)) << ";\n";
                        break;
                    case 29:     // getelementptr                               // this can be ignored
                        /*for (auto ait = arrayInfo.begin(); ait != arrayInfo.end(); ++ait)
                        {   // get dimension
                            arrayNode *AN = *ait;
                            std::vector<uint64_t> size = AN->size;
                            GlobalVariable *addr = AN->addr;
                            
                            if (I->getOperand(0) == addr)
                            {
                                dim = size.size();
                                break;
                            }
                        }
                        
                        if (dim == 0)
                            errs() << "Error: unknown array accessed - cannot find size: " << *I << "\n";
                        else if (dim == 1)
                            *fout << "\t" << printRegNameInBoogie((Value *)I) << " := " << printRegNameInBoogie((Value *)I->getOperand(0)) << "[" << printRegNameInBoogie((Value *)I->getOperand(2)) << "];\n";
                        else if (dim == 2)
                            *fout << "\t" << printRegNameInBoogie((Value *)I) << " := " << printRegNameInBoogie((Value *)I->getOperand(0)) << "[" << printRegNameInBoogie((Value *)I->getOperand(1)) << "][" << printRegNameInBoogie((Value *)I->getOperand(2)) << "];\n";
                        else
                            errs() << "Error: 2+ dimension array accessed - not compatible yet: " << *I << "\n";*/
                    *fout << "\t" << printRegNameInBoogie(I) << " := 0bv32;\n";
                    break;

                    case 33:     // trunc
                        // ignore trunc instructions
                    *fout << "\t" << printRegNameInBoogie(I) << " := "<< printRegNameInBoogie((Value *)I->getOperand(0)) << ";\n";
                    break;

                    case 34:     // zext   - may be modified later
                        // possible types are i1, i8. i16, i24, i32, i40, i48, i56, i64, i88, i96, i128, ref, float
                        if (I->getType()->getTypeID() == 10) // check if it is integer type
                        {
                            IntegerType *resType = dyn_cast<IntegerType>(I->getType());
                            IntegerType *oprType = dyn_cast<IntegerType>(I->getOperand(0)->getType());
                            
                            if (!resType)
                                errs() << "Error found in getting result type of zext instruction: " << *I << "\n";
                            if (!oprType)
                                errs() << "Error found in getting operand type of zext instruction: " << *I << "\n";
                            
                            if (resType->getBitWidth() > oprType->getBitWidth())
                                *fout << "\t" << printRegNameInBoogie(I) << " := " << printRegNameInBoogie((Value *)I->getOperand(0)) << ";\n";
                            else
                            {
                                // do mod
                                // case errs() << printRegNameInBoogie(I) << ":=" << printRegNameInBoogie((Value *)I->getOperand(0)) << " % " << ??? << ";\n";
                            }
                        }
                        else
                            errs() << "Error: Undefined type in zext instruction: " << *I << "\n";
                        break;
                        
                    case 46:    // icmp
                    if (CmpInst *cmpInst = dyn_cast<CmpInst>(&*I)) {
                        if (cmpInst->getPredicate() == CmpInst::ICMP_EQ) {
                            *fout << "\tif (" << printRegNameInBoogie((Value *)I->getOperand(0)) << " == " << printRegNameInBoogie((Value *)I->getOperand(1)) << ") { " << printRegNameInBoogie(I) << " := 1bv32; } else { " << printRegNameInBoogie(I) << " := 0bv32; }\n";
                        }
                        else if (cmpInst->getPredicate() == CmpInst::ICMP_NE) {
                            *fout << "\tif (" << printRegNameInBoogie((Value *)I->getOperand(0)) << " != " << printRegNameInBoogie((Value *)I->getOperand(1)) << ") { " << printRegNameInBoogie(I) << " := 1bv32; } else { " << printRegNameInBoogie(I) << " := 0bv32; }\n";
                        }
                        else if (cmpInst->getPredicate() == CmpInst::ICMP_UGT) {
                                *fout << "\tif (bv32ugt(" << printRegNameInBoogie((Value *)I->getOperand(0)) << ", " << printRegNameInBoogie((Value *)I->getOperand(1)) << ") == true) { " << printRegNameInBoogie(I) << " := 1bv32; } else { " << printRegNameInBoogie(I) << " := 0bv32; }\n"; // ---might has problems...
                            }
                            else if (cmpInst->getPredicate() == CmpInst::ICMP_UGE) {
                                *fout << "\tif (bv32uge(" << printRegNameInBoogie((Value *)I->getOperand(0)) << ", " << printRegNameInBoogie((Value *)I->getOperand(1)) << ") == true) { " << printRegNameInBoogie(I) << " := 1bv32; } else { " << printRegNameInBoogie(I) << " := 0bv32; }\n"; // ---might has problems...
                            }
                            else if (cmpInst->getPredicate() == CmpInst::ICMP_ULT) {
                                *fout << "\tif (bv32ult(" << printRegNameInBoogie((Value *)I->getOperand(0)) << ", " << printRegNameInBoogie((Value *)I->getOperand(1)) << ") == true) { " << printRegNameInBoogie(I) << " := 1bv32; } else { " << printRegNameInBoogie(I) << " := 0bv32; }\n"; // ---might has problems...
                            }
                            else if (cmpInst->getPredicate() == CmpInst::ICMP_ULE) {
                                *fout << "\tif (bv32ule(" << printRegNameInBoogie((Value *)I->getOperand(0)) << ", " << printRegNameInBoogie((Value *)I->getOperand(1)) << ") == true) { " << printRegNameInBoogie(I) << " := 1bv32; } else { " << printRegNameInBoogie(I) << " := 0bv32; }\n"; // ---might has problems...
                            }
                            else if (cmpInst->getPredicate() == CmpInst::ICMP_SGT) {
                                *fout << "\tif (bv32sgt(" << printRegNameInBoogie((Value *)I->getOperand(0)) << ", " << printRegNameInBoogie((Value *)I->getOperand(1)) << ") == true) { " << printRegNameInBoogie(I) << " := 1bv32; } else { " << printRegNameInBoogie(I) << " := 0bv32; }\n";
                            }
                            else if (cmpInst->getPredicate() == CmpInst::ICMP_SGE) {
                                *fout << "\tif (bv32sge(" << printRegNameInBoogie((Value *)I->getOperand(0)) << ", " << printRegNameInBoogie((Value *)I->getOperand(1)) << ") == true) { " << printRegNameInBoogie(I) << " := 1bv32; } else { " << printRegNameInBoogie(I) << " := 0bv32; }\n";
                            }
                            else if (cmpInst->getPredicate() == CmpInst::ICMP_SLT) {
                                *fout << "\tif (bv32slt(" << printRegNameInBoogie((Value *)I->getOperand(0)) << ", " << printRegNameInBoogie((Value *)I->getOperand(1)) << ") == true) { " << printRegNameInBoogie(I) << " := 1bv32; } else { " << printRegNameInBoogie(I) << " := 0bv32; }\n";
                            }
                            else if (cmpInst->getPredicate() == CmpInst::ICMP_SLE) {
                                *fout << "\tif (bv32sle(" << printRegNameInBoogie((Value *)I->getOperand(0)) << ", " << printRegNameInBoogie((Value *)I->getOperand(1)) << ") == true) { " << printRegNameInBoogie(I) << " := 1bv32; } else { " << printRegNameInBoogie(I) << " := 0bv32; }\n";
                            }
                            else
                                errs() << "Error: Instruction decoding error at icmp instruction: " << *I << "\n";
                        }
                        break;
                    case 48:     // phi
                        // Has been done in previous section
                    break;
                    case 49:     // call
                    errs() << "Error: Call functions found in thread functions: " << *I << "\n";
                    break;
                    case 50:     // select
                    *fout << "\tif (" << printRegNameInBoogie((Value *)I->getOperand(0)) << " == 1bv32) { " << printRegNameInBoogie(I) << " := " << printRegNameInBoogie((Value *)I->getOperand(1)) <<  "; } else { " << printRegNameInBoogie(I) << " := " << printRegNameInBoogie((Value *)I->getOperand(2)) << "; }\n";
                    break;
                    default:
                    errs() << "Error: <Invalid operator>" << I->getOpcodeName() << "\t" << I->getOpcode() << "\n ";
                    I->print(errs());
                    errs() << "\n ";
                }   // end of switch
                
                for (auto bankAddrCheck = bankAddressInstrList2d[count].begin(); bankAddrCheck != bankAddressInstrList2d[count].end(); ++bankAddrCheck)
                {
                    if (*bankAddrCheck == I)
                    {
                        *fout << "\tif(*){\n\t\tindex_" << indexCounter << " := " << printRegNameInBoogie(I) << ";\n\t\tread_" << indexCounter << " := true;\n\t}\n\treturn;\n\tassume false;\n";
                        indexCounter++;
                    }
                }
            }                                                               // End of instruction level analysis
            
        }                                     // End of basic block level analysis
    }   // end of instrDecoding


    // This function is to print variable declaration in Boogie
    void EASY::varDeclaration(Value *instrVar, std::fstream *fout, bool isbool){
        *fout << "\tvar " << printRegNameInBoogie(instrVar) << ":bv32;\n";
    }   // end of varDeclaration
    
    // This function is to check if the variable has been declared
    bool EASY::varFoundInList(Value *instrVar, Function *F){
        bool funcFound = false;
        bool varFound = false;
        varList *list;
        
        // check if it is a constant
        if (dyn_cast<llvm::ConstantInt>(instrVar))
            return true;
        
        // search function in variable list
        for (auto it = codeVarList.begin(); it != codeVarList.end(); ++it)
        {
            varList *temp = *it;
            // errs() << "debug check function :" << F->getName() << "\n";// << " VS "<< temp->func->getName() << "\n";
            if (temp->func == F)
            {
                funcFound = true;
                // errs() << "Function " << F->getName() << " found.\n";
                list = *it;
                // Function found => search for var
                if (!isFuncArg(instrVar, F))
                {
                    for (auto itVar = list->vars.begin(); itVar != list->vars.end(); ++itVar)
                    {
                        Value *itvar = *itVar;
                        // errs() << "debug check var: " << printRegNameInBoogie(itvar) << "\n";
                        if (itvar == instrVar)
                            varFound = true;
                    }
                    if (!varFound)
                        list->vars.push_back(instrVar);
                }
                else
                    varFound = true;
            }
        }
        
        if (!funcFound)
        {
            // Function not found => start new
            list = new varList;
            if (!isFuncArg(instrVar, F))
            {
                list->func = F;
                list->vars.push_back(instrVar);
            }
            // errs() << "Function " << F->getName() << " not found and added.\n";
            codeVarList.push_back(list);
        }
        
        return varFound;
    }   // end of varFoundInList
    
    // This is function is determine if it is a function argument
    bool EASY::isFuncArg(Value *instrVar, Function *F){
        for (auto fi = F->arg_begin(); fi != F->arg_end(); ++fi)
        {
            Value *arg = fi;
            if (instrVar == arg)
                return true;
        }
        return false;
    }   // end of isFuncArg
    
    // This is a function to print hidden basic block label
    std::string EASY::getBlockLabel(BasicBlock *BB){
        std::string block_address;
        raw_string_ostream string_stream(block_address);
        BB->printAsOperand(string_stream, false);
        
        std::string temp = string_stream.str();
        
        for (unsigned int i = 0; i<temp.length(); ++i)
        {
            if (temp[i] == '-')
                temp.replace(i, 1, "_");
        }
        
        return temp;
    }   // end of getBlockLabel
    
    // This is a function to print IR variable name
    std::string EASY::printRegNameInBoogie(Value *I){
        std::string instrResName;
        raw_string_ostream string_stream(instrResName);
        I->printAsOperand(string_stream, false);
        if (ConstantInt *constVar = dyn_cast<ConstantInt>(I))
        {

            // errs() << "This is a constant: " << *I << "----";
            // if (constVar->getType()->isIntegerTy())
            if (constVar->isNegative())
            {
                string_stream.str().replace(0,1," ");
                // errs() << "Output test: bv32sub(0bv32, " + string_stream.str()+"bv32)\n";
                return ("bv32neg(" + string_stream.str()+"bv32)"); //*(constVar->getType())
            }
            else
                return(string_stream.str()+"bv32");
        }
        return string_stream.str();
    }       // end of printRegNameInBoogie


    char EASY::ID = 0;
    static RegisterPass<EASY> X("EASY", "Global Memory Partitioning Verification Program",
        false /* Only looks at CFG */,
        false /* Analysis Pass */);

}  // end of legup namespace
