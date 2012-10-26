/*! \file ocelot.cpp
	\date Friday July 24, 2009
	\author Gregory Diamos <gregory.diamos@gatech.edu>
	\brief The header file for the Ocelot API functions.
*/

#ifndef OCELOT_CPP_INCLUDED
#define OCELOT_CPP_INCLUDED

#include <ocelot/api/interface/ocelot.h>
#include <ocelot/cuda/interface/CudaRuntimeInterface.h>
#include <ocelot/cuda/interface/CudaDriverInterface.h>
#include <ocelot/opencl/interface/OpenCLRuntimeInterface.h>
#include <hydrazine/interface/Exception.h>

namespace ocelot
{
	static ocelot::OcelotInterface* get() {
		if(api::OcelotConfiguration::get().cuda.implementation == "CudaRuntime")
			return cuda::CudaRuntimeInterface::get();
		else if(api::OcelotConfiguration::get().cuda.implementation == "CudaDriver")
			return cuda::CudaDriverInterface::get();
		else if(api::OcelotConfiguration::get().opencl.implementation == "OpenCLRuntime")
			return opencl::OpenCLRuntimeInterface::get();
		else
			return NULL;
	}

	void addTraceGenerator( trace::TraceGenerator& gen, 
		bool persistent )
	{
		get()->addTraceGenerator(gen, persistent );
	}
				
	void clearTraceGenerators( )
	{
		get()->clearTraceGenerators( );
	}

	void addPTXPass(transforms::Pass &pass) 
	{
		get()->addPTXPass(pass);
	}

	void removePTXPass(transforms::Pass &pass) 
	{
		get()->removePTXPass(pass);
	}

	void clearPTXPasses() 
	{
		get()->clearPTXPasses();
	}
	
	void limitWorkerThreads( unsigned int limit )
	{
		get()->limitWorkerThreads( limit );
	}

	void registerPTXModule(std::istream& stream, const std::string& name)
	{
		get()->registerPTXModule( stream, name );
	}

	void registerTexture(const void* texref,
		const std::string& moduleName,
		const std::string& textureName, bool normalize)
	{
		get()->registerTexture(texref, moduleName, textureName, normalize);
	}
	
	void clearErrors()
	{
		get()->clearErrors();
	}

	void reset()
	{
		get()->reset();
	}
	
	PointerMap contextSwitch( unsigned int destinationDevice, 
		unsigned int sourceDevice )
	{
		return std::move(get()->contextSwitch( 
			destinationDevice, sourceDevice ));
	}
	
	void unregisterModule( const std::string& name )
	{
		get()->unregisterModule( name );
	}
	
	void launch(const std::string& module, const std::string& name)
	{
		get()->launch(module, name);
	}

	void registerExternalFunction(const std::string& name, void* function)
	{
		get()->registerExternalFunction(name, function);
	}

	void removeExternalFunction(const std::string& name)
	{
		get()->removeExternalFunction(name);
	}
	
	/*! \brief gets the Ocelot device properties of either the current device 
		(deviceIndex = -1) or the indexed device */
	void getDeviceProperties(executive::DeviceProperties &properties, int deviceIndex) {
		get()->getDeviceProperties(properties, deviceIndex);
	}
}

#endif

