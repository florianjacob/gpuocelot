/*! \file PTXKernel.cpp
	\author Gregory Diamos <gregory.diamos@gatech>
	\date Thursday September 17, 2009
	\brief The header file for the PTXKernel class
*/

#ifndef PTX_KERNEL_H_INCLUDED
#define PTX_KERNEL_H_INCLUDED

#include <ocelot/ir/interface/PTXKernel.h>
#include <ocelot/ir/interface/ControlFlowGraph.h>

#include <hydrazine/interface/Version.h>
#include <hydrazine/implementation/debug.h>

#ifdef REPORT_BASE
#undef REPORT_BASE
#endif

#define REPORT_BASE 0

namespace ir
{
	PTXKernel::PTXKernel( PTXStatementVector::const_iterator start,
		PTXStatementVector::const_iterator end, bool function) : 
		Kernel( Instruction::PTX, function )
	{
		// get parameters/locals, extract kernel name
		for( PTXStatementVector::const_iterator it = start; it != end; ++it ) 
		{
			if( (*it).directive == PTXStatement::Param )
			{
				parameters.push_back( Parameter( *it ) );
			}
			else if( (*it).directive == PTXStatement::Local
				|| (*it).directive == PTXStatement::Shared )
			{
				locals.insert( std::make_pair( ( *it ).name, Local( *it ) ) );
			}
			else if( (*it).directive == PTXStatement::Entry )
			{
				name = (*it).name;
			}
		}
		_cfg = new ControlFlowGraph;
		constructCFG( *_cfg, start, end );
	}

	PTXKernel::PTXKernel( const PTXKernel& kernel ) : Kernel( kernel )
	{
		
	}

	const PTXKernel& PTXKernel::operator=(const PTXKernel &kernel) 
	{
		if( &kernel == this ) return *this;
		
		Kernel::operator=(kernel);
		_function = kernel.function();

		return *this;	
	}

	PTXKernel::RegisterVector PTXKernel::getReferencedRegisters() const
	{
		report( "Getting list of all referenced registers" );				

		typedef std::unordered_set< analysis::DataflowGraph::RegisterId > 
			RegisterSet;
		assert( !_dfg->ssa() );

		RegisterSet encountered;
		RegisterSet predicates;
			
		RegisterVector regs;
		
		for( analysis::DataflowGraph::const_iterator block = _dfg->begin(); 
			block != _dfg->end(); ++block  )
		{
			report( " For block " << block->label() );
						
			for( analysis::DataflowGraph::InstructionVector::const_iterator 
				instruction = block->instructions().begin(); 
				instruction != block->instructions().end(); ++instruction )
			{
				report( "  For instruction " << instruction->i->toString() );

				typedef analysis::DataflowGraph::RegisterPointerVector
					RegisterPointerVector;

				for( RegisterPointerVector::const_iterator 
					d = instruction->d.begin(); d != instruction->d.end(); ++d )
				{
					if( d->type != ir::PTXOperand::pred )
					{
						if( encountered.insert( *d->pointer ).second )
						{
							report( "   Added %r" << *d->pointer );
							analysis::DataflowGraph::Register live_reg( 
								*d->pointer, d->type );
							regs.push_back( live_reg );
						}
					}
					else
					{
						if( predicates.insert( *d->pointer ).second )
						{
							report( "   Added %p" << *d->pointer );
							analysis::DataflowGraph::Register live_reg( 
								*d->pointer, d->type );
							regs.push_back( live_reg );
						}
					}
				}
			}
		}
		
		return regs;
	}

	analysis::DataflowGraph* PTXKernel::dfg() 
	{
		assertM(_cfg != 0, "Must create cfg before building dfg.");
		if(_dfg) return _dfg;
		assignRegisters( *_cfg );
		_dfg = new analysis::DataflowGraph( *_cfg );
		return _dfg;
	}

	const analysis::DataflowGraph* PTXKernel::dfg() const 
	{
		return Kernel::dfg();
	}

	bool PTXKernel::executable() const {
		return false;
	}

	void PTXKernel::constructCFG( ControlFlowGraph &cfg,
		PTXStatementVector::const_iterator kernelStart,
		PTXStatementVector::const_iterator kernelEnd) {
		typedef std::unordered_map< std::string, 
			ControlFlowGraph::iterator > BlockToLabelMap;
		typedef std::vector< ControlFlowGraph::iterator > BlockPointerVector;
	
		BlockToLabelMap blocksByLabel;
		BlockPointerVector branchBlocks;

		ControlFlowGraph::iterator last_inserted_block = cfg.end();
		ControlFlowGraph::iterator block = cfg.insert_block(
			ControlFlowGraph::BasicBlock("", cfg.newId()));
		ControlFlowGraph::Edge edge(cfg.get_entry_block(), block, 
			ControlFlowGraph::Edge::FallThrough);
	
		unsigned int statementIndex = 0;
		for( ; kernelStart != kernelEnd; ++kernelStart, ++statementIndex ) 
		{
			const PTXStatement &statement = *kernelStart;
		
			if( statement.directive == PTXStatement::Label ) 
			{
				// a label indicates the termination of a previous block
				//
				// This implementation does not store any empty basic blocks.
				if( block->instructions.size() ) {
					//
					// insert old block
					//
					if (edge.type != ControlFlowGraph::Edge::Invalid) {
						cfg.insert_edge(edge);
					}
				
					edge.head = block;
					last_inserted_block = block;
					block = cfg.insert_block(
						ControlFlowGraph::BasicBlock("", cfg.newId()));
					edge.tail = block;
					edge.type = ControlFlowGraph::Edge::FallThrough;
				}
				
				block->label = statement.name;
				assertM( blocksByLabel.count( block->label ) == 0, 
					"Duplicate blocks with label " << block->label )
				blocksByLabel.insert( std::make_pair( block->label, block ) );
			}
			else if( statement.directive == PTXStatement::Instr ) 
			{
				block->instructions.push_back( statement.instruction.clone() );
				
				if (statement.instruction.opcode == PTXInstruction::Bra) 
				{
					last_inserted_block = block;
					// dont't add fall through edges for unconditional branches
					if (edge.type != ControlFlowGraph::Edge::Invalid ) {
						cfg.insert_edge(edge);
					}
					edge.head = block;
					branchBlocks.push_back(block);
					block = cfg.insert_block(
						ControlFlowGraph::BasicBlock("", cfg.newId()));
					if (statement.instruction.pg.condition 
						!= ir::PTXOperand::PT) {
						edge.tail = block;
						edge.type = ControlFlowGraph::Edge::FallThrough;
					}
					else {
						edge.type = ControlFlowGraph::Edge::Invalid;
					}
				}
				else if( statement.instruction.opcode == PTXInstruction::Exit )
				{
					last_inserted_block = block;
					if (edge.type != ControlFlowGraph::Edge::Invalid) {
						cfg.insert_edge(edge);
					}
					edge.head = block;
					edge.tail = cfg.get_exit_block();
					edge.type = ControlFlowGraph::Edge::FallThrough;

					block = cfg.insert_block(
						ControlFlowGraph::BasicBlock("", cfg.newId()));
					edge.type = ControlFlowGraph::Edge::Invalid;
				}
				else if( statement.instruction.opcode == PTXInstruction::Call )
				{
					assertM(false, "Unhandled control flow instruction call");
				}
				else if( statement.instruction.opcode == PTXInstruction::Ret )
				{
					last_inserted_block = block;
					if (edge.type != ControlFlowGraph::Edge::Invalid) {
						cfg.insert_edge(edge);
					}
					edge.head = block;
					edge.tail = cfg.get_exit_block();
					edge.type = ControlFlowGraph::Edge::Branch;

					block = cfg.insert_block(
						ControlFlowGraph::BasicBlock("", cfg.newId()));
					edge.type = ControlFlowGraph::Edge::Invalid;
				}
			}
		}

		if (block->instructions.size()) 
		{
			if (edge.type != ControlFlowGraph::Edge::Invalid) {
				cfg.insert_edge(edge);
			}
			edge.head = block;
			edge.tail = cfg.get_exit_block();
			edge.type = ControlFlowGraph::Edge::FallThrough;
			cfg.insert_edge(edge);
		}
		else 
		{
			if(last_inserted_block!=cfg.end()) 
			{
				// make sure there is a fall through edge from the last 
				// inserted block to the exit node
				ControlFlowGraph::edge_iterator ft_e = cfg.edges_end();
				for( ControlFlowGraph::edge_pointer_iterator 
					it = last_inserted_block->out_edges.begin(); 
					it != last_inserted_block->out_edges.end(); ++it )
				{
					if( (*it)->type == ControlFlowGraph::Edge::FallThrough 
						&& (*it)->tail == cfg.get_exit_block() )
					{
						ft_e = (*it);
						break;
					}
				}
				if( ft_e == cfg.edges_end() )
				{
					cfg.insert_edge(ControlFlowGraph::Edge(last_inserted_block, 
						cfg.get_exit_block()));
				}
			}
			cfg.remove_block(block);
		}

		// go back and add edges for basic blocks terminating in branches
		for( BlockPointerVector::iterator it = branchBlocks.begin();
			it != branchBlocks.end(); ++it ) 
		{
			PTXInstruction& bra = *static_cast<PTXInstruction*>(
				(*it)->instructions.back());
			// skip always false branches
			if( bra.pg.condition == ir::PTXOperand::nPT ) continue;
			
			BlockToLabelMap::iterator labeledBlockIt = 
				blocksByLabel.find( bra.d.identifier );
		
			assertM(labeledBlockIt != blocksByLabel.end(), 
				"undefined label " << bra.d.identifier);
		
			bra.d.identifier = labeledBlockIt->second->label;
			cfg.insert_edge(ControlFlowGraph::Edge(*it, 
				labeledBlockIt->second, ControlFlowGraph::Edge::Branch));
		}
	}

	PTXKernel::RegisterMap PTXKernel::assignRegisters( ControlFlowGraph& cfg ) 
	{
		RegisterMap map;
	
		report( "Allocating registers " );
	
		for (ControlFlowGraph::iterator block = cfg.begin(); 
			block != cfg.end(); ++block) {
			for (ControlFlowGraph::InstructionList::iterator 
				instruction = block->instructions.begin(); 
				instruction != block->instructions.end(); ++instruction) {
				PTXInstruction& instr = *static_cast<PTXInstruction*>(
					*instruction);
				PTXOperand PTXInstruction:: * operands[] = 
				{ &PTXInstruction::a, &PTXInstruction::b, &PTXInstruction::c, 
					&PTXInstruction::d, &PTXInstruction::pg, 
					&PTXInstruction::pq };
		
				report( " For instruction '" << instr.toString() << "'" );
		
				for (int i = 0; i < 6; i++) {
					if ((instr.*operands[i]).addressMode 
						== PTXOperand::Invalid) {
						continue;
					}
					if ((instr.*operands[i]).type == PTXOperand::pred
						&& (instr.*operands[i]).condition == PTXOperand::PT) {
						continue;
					}
					if ((instr.*operands[i]).addressMode == PTXOperand::Register
						|| (instr.*operands[i]).addressMode 
						== PTXOperand::Indirect) {
						if ((instr.*operands[i]).vec != PTXOperand::v1) {
							for (PTXOperand::Array::iterator 
								a_it = (instr.*operands[i]).array.begin(); 
								a_it != (instr.*operands[i]).array.end(); 
								++a_it) {
								RegisterMap::iterator it 
									= map.find(a_it->registerName());

								PTXOperand::RegisterType reg = 0;
								if (it == map.end()) {
									reg = (PTXOperand::RegisterType) map.size();
									map.insert(std::make_pair( 
										a_it->registerName(), reg));
								}
								else {
									reg = it->second;
								}
								a_it->reg = reg;
								report( "  Assigning register " 
									<< a_it->registerName() 
									<< " to " << a_it->reg );
								a_it->identifier.clear();
							}
						}
						else {
							RegisterMap::iterator it 
								= map.find((instr.*operands[i]).registerName());

							PTXOperand::RegisterType reg = 0;
							if (it == map.end()) {
								reg = (PTXOperand::RegisterType) map.size();
								map.insert(std::make_pair( 
									(instr.*operands[i]).registerName(), reg));
							}
							else {
								reg = it->second;
							}
							(instr.*operands[i]).reg = reg;
							report("  Assigning register " 
								<< (instr.*operands[i]).registerName() 
								<< " to " << reg);
							(instr.*operands[i]).identifier.clear();
						}
					}
				}
			}
		}

		return map;
	}

	void PTXKernel::write(std::ostream& stream) const 
	{
		stream << "/*\n* Ocelot Version : " 
			<< hydrazine::Version().toString() << "\n*/\n";
	
		stream << ".entry " << name;
		if (parameters.size()) {
			stream << "(";
			for( ParameterVector::const_iterator parameter = parameters.begin();
				parameter != parameters.end(); ++parameter )
			{
				if( parameter != parameters.begin() )
				{
					stream << ",\n\t\t" << parameter->toString();
				}
				else
				{
					stream << parameter->toString();
				}
			}
			stream << ")\n";
		}
		stream << "{\n";
		
		for (LocalMap::const_iterator local = locals.begin();
			local != locals.end(); ++local) {
			stream << "\t" << local->second.toString() << "\n";
		}
		
		if( _dfg != 0 )
		{
			RegisterVector regs = getReferencedRegisters();
		
			for (RegisterVector::const_iterator reg = regs.begin();
				reg != regs.end(); ++reg) {
				if (reg->type == PTXOperand::pred) {
					stream << "\t.reg .pred %p" << reg->id << ";\n";
				}
				else {
					stream << "\t.reg ." 
						<< PTXOperand::toString( reg->type ) << " " 
						<< "%r" << reg->id << ";\n";
				}
			}
		}
		
		if (_cfg != 0) {
			ControlFlowGraph::BlockPointerVector 
				blocks = _cfg->executable_sequence();
		
			int blockIndex = 1;
			for (ControlFlowGraph::BlockPointerVector::iterator 
				block = blocks.begin(); block != blocks.end(); 
				++block, ++blockIndex) {
				std::string label = (*block)->label;
				std::string comment = (*block)->comment;
				if ((*block)->instructions.size() 
					|| (label != "entry" && label != "exit" && label != "")) {
					if (label == "") {
						std::stringstream ss;
						ss << "$__Block_" << blockIndex;
						label = ss.str();
					}
					stream << "\t" << label << ":";
					if (comment != "") {
						stream << "\t\t\t\t/* " << comment << " */ ";
					}
					stream << "\n";
				}
				
				for( ControlFlowGraph::InstructionList::iterator 
					instruction = (*block)->instructions.begin(); 
					instruction != (*block)->instructions.end();
					++instruction ) {
					stream << "\t\t" << (*instruction)->toString() << ";\n";
				}
			}
		}
		stream << "}\n";
	}

	/*! \brief renames all the blocks with canonical names */
	void PTXKernel::canonicalBlockLabels(int kernelID) {

		// visit every block and map the old label to the new label
		std::map<std::string, std::string> labelMap;
		
		for (ControlFlowGraph::iterator 
			block = cfg()->begin(); 
			block != cfg()->end(); ++block) { 

			if (block == cfg()->get_entry_block()) continue;
			if (block == cfg()->get_exit_block()) continue;
			
			std::stringstream ss;
			ss << "$BB_" << kernelID << "_";
			ss.fill('0');
			ss.width(4);
			ss << block->id;
			ss.width(0);
			labelMap[block->label] = ss.str();
			block->comment = block->label;
			block->label = ss.str();
		}
		
		// visit every branch and rewrite the branch target according to the label map

		for (ControlFlowGraph::iterator block = cfg()->begin(); 
			block != cfg()->end(); ++block) {
			for (ControlFlowGraph::InstructionList::iterator 
				instruction = block->instructions.begin(); 
				instruction != block->instructions.end(); ++instruction) {
				PTXInstruction &instr = *static_cast<PTXInstruction*>(
					*instruction);
				if (instr.opcode == ir::PTXInstruction::Bra) {
					instr.d.identifier = labelMap[instr.d.identifier];
				}
			}
		}
	}
}

#endif
