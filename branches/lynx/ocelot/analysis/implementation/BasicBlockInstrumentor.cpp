/*! \file BasicBlockInstrumentor.cpp
	\date Monday November 15, 2010
	\author Naila Farooqui <naila@cc.gatech.edu>
	\brief The source file for the BasicBlockInstrumentor class.
*/

#ifndef BASIC_BLOCK_INSTRUMENTOR_CPP_INCLUDED
#define BASIC_BLOCK_INSTRUMENTOR_CPP_INCLUDED

#include <ocelot/analysis/interface/BasicBlockInstrumentor.h>

#include <ocelot/cuda/interface/cuda_runtime.h>

#include <ocelot/analysis/interface/BasicBlockInstrumentationPass.h>
#include <ocelot/analysis/interface/BasicBlockExecutionCountPass.h>
#include <ocelot/ir/interface/Module.h>

#include <hydrazine/implementation/ArgumentParser.h>
#include <hydrazine/implementation/string.h>
#include <hydrazine/implementation/debug.h>
#include <hydrazine/implementation/Exception.h>

#include <fstream>

using namespace hydrazine;

namespace analysis
{

    void BasicBlockInstrumentor::analyze(ir::Module &module) {
        basicBlocks = 0;
        if(!kernelName.empty()){
            basicBlocks = module.kernels().find(kernelName)->second->dfg()->size() - 2;
        }
        else {
            for (ir::Module::KernelMap::const_iterator kernel = module.kernels().begin(); 
		        kernel != module.kernels().end(); ++kernel) {
                basicBlocks += (kernel->second)->dfg()->size();
            }
        }
    }

    void BasicBlockInstrumentor::initialize() {
        
        counter = 0;
        cudaMalloc((void **) &counter, basicBlocks * threadBlocks * threads * sizeof(size_t));
        cudaMemset( counter, 0, basicBlocks * threadBlocks * threads * sizeof( size_t ));
        
        cudaMemcpyToSymbol(((BasicBlockInstrumentationPass *)pass)->basicBlockCounterBase().c_str(), &counter, sizeof(*counter), 0, cudaMemcpyHostToDevice);
    }

    analysis::Pass *BasicBlockInstrumentor::createPass() {
        return new analysis::BasicBlockExecutionCountPass;           
    }

    size_t* BasicBlockInstrumentor::extractResults(std::ostream *out) {

        size_t *info = new size_t[basicBlocks * threads * threadBlocks];
        if(counter) {
            cudaMemcpy(info, counter, basicBlocks * threads * threadBlocks * sizeof( size_t ), cudaMemcpyDeviceToHost);
            cudaFree(counter);
        }

        *out << "\n\n" << kernelName << ":\n";
        *out << "\n--------------- " << description << " ---------------\n\n";

        _kernelProfile.basicBlockExecutionCountMap.clear();

        size_t i = 0;
        size_t j = 0;
        size_t k = 0;
        
        for(k = 0; k < threadBlocks; k++) {
            for(i = 0; i < basicBlocks; i++) {
                for(j = 0; j < threads; j++) {
                    _kernelProfile.basicBlockExecutionCountMap[i] += info[(i * threads) + (k * basicBlocks * threads) + j];
                }
            }   
        }

        for(j = 0; j < basicBlocks; j++) {
            *out << "Total Execution Count for Basic Block " << j << ": " << _kernelProfile.basicBlockExecutionCountMap[j] << std::endl;
        }
        
        return info;
    }

    BasicBlockInstrumentor::BasicBlockInstrumentor() : description("Basic Block Execution Count Per Thread") {

    }

}

#endif