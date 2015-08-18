// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <llvm/Support/raw_ostream.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>

#include "Core/HW/MMIO.h"

#include "Core/PowerPC/JitLLVM/JitLLVM_ConstantLoadStorePass.h"

using namespace llvm;

namespace FuncInfo
{
struct FunctionInfo
{
	bool m_store;
	u8 m_size;
};

static bool inited = false;
static std::map<void*, FunctionInfo> m_loadstore_info;
static void Init()
{
	if (inited)
		return;
	m_loadstore_info[(void*)PowerPC::Read_U8]   = {false, 8};
	m_loadstore_info[(void*)PowerPC::Read_U16]  = {false, 16};
	m_loadstore_info[(void*)PowerPC::Read_U32]  = {false, 32};
	m_loadstore_info[(void*)PowerPC::Read_U64]  = {false, 64};
	m_loadstore_info[(void*)PowerPC::Write_U8]  = {true, 8};
	m_loadstore_info[(void*)PowerPC::Write_U16] = {true, 16};
	m_loadstore_info[(void*)PowerPC::Write_U32] = {true, 32};
	m_loadstore_info[(void*)PowerPC::Write_U64] = {true, 64};
	inited = true;
}
static bool Handled(void* ptr, bool* store, u8* size)
{
	if (!inited)
		return false;
	auto info = m_loadstore_info.find(ptr);
	if (info == m_loadstore_info.end())
		return false;
	*store = info->second.m_store;
	*size = info->second.m_size;
	return true;
}
}

// Visitor that generates code to write to an MMIO location
template <typename T>
class LLVMMMIOWriteCodeGenerator : public MMIO::WriteHandlingMethodVisitor<T>
{
public:
	LLVMMMIOWriteCodeGenerator(IRBuilder<>* builder, Value* src, u32 address)
		: m_builder(builder), m_src(src), m_address(address), m_has_res(false), res_inst(nullptr)
	{
	}

	virtual void VisitNop()
	{
		// Do nothing
		m_has_res = true;
	}
	virtual void VisitDirect(T* addr, u32 mask)
	{
		Constant* const_addr = ConstantInt::getIntegerValue(m_builder->getIntNTy(sizeof(T)), APInt(64, (uint64_t)addr));
		res_inst = m_builder->CreateAnd(m_src, m_builder->getInt32(mask));
		res_inst = m_builder->CreateStore(res_inst, const_addr);
		m_has_res = true;
	}
	virtual void VisitComplex(const std::function<void(u32, T)>* lambda)
	{
		// XXX: Not supported yet
	}

	bool HasResult() { return m_has_res; }
	Value* GetResultInst() { return res_inst; }

private:
	IRBuilder<>* m_builder;
	Value* m_src;
	u32 m_address;
	bool m_has_res;

	Value* res_inst;
};

// Visitor that generates code to read a MMIO value.
template <typename T>
class LLVMMMIOReadCodeGenerator : public MMIO::ReadHandlingMethodVisitor<T>
{
public:
	LLVMMMIOReadCodeGenerator(IRBuilder<>* builder, u32 address)
		: m_builder(builder), m_address(address), m_has_res(false)
	{
	}

	virtual void VisitConstant(T value)
	{
		res_inst = m_builder->getIntN(sizeof(T), value);
		m_has_res = true;
	}

	virtual void VisitDirect(const T* addr, u32 mask)
	{
		Constant* const_addr = ConstantInt::getIntegerValue(m_builder->getIntNTy(sizeof(T)), APInt(64, (uint64_t)addr));
		res_inst = m_builder->CreateLoad(const_addr);
		res_inst = m_builder->CreateAnd(res_inst, m_builder->getInt32(mask));
		m_has_res = true;
	}

	virtual void VisitComplex(const std::function<T(u32)>* lambda)
	{
		// XXX: Not supported yet
	}

	bool HasResult() { return m_has_res; }
	Value* GetResultInst() { return res_inst; }

private:
	IRBuilder<>* m_builder;
	u32 m_address;
	bool m_has_res;

	Value* res_inst;
};

bool ConstantLoadStorePass::runOnBasicBlock(BasicBlock& BB)
{
	FuncInfo::Init();
	LLVMBinding* binder = m_mod->GetBinding();
	LLVMFunction* func = m_mod->GetFunction();
	BasicBlock::iterator start = BB.begin();
	BasicBlock::iterator end = BB.end();

	bool made_filthy = false;
	std::vector<Instruction*> to_delete;
	do
	{
		if (CallInst* inst = dyn_cast<CallInst>(&*start))
		{
			Function* call_func = inst->getCalledFunction();
			void* func_ptr = binder->GetBinding(call_func->getName());
			bool store;
			u8 size;
			if (FuncInfo::Handled(func_ptr, &store, &size))
			{
				// For stores we only need the second argument to be a constant
				// For loads we only need the first(only) argument to be constant
				Value* address_arg = inst->getArgOperand(store ? 1 : 0);
				if (Constant* const_addr = dyn_cast<Constant>(address_arg))
				{
					IRBuilder<> builder(inst);
					Type* llvm_type = builder.getIntNTy(size)->getPointerTo();
					uint32_t const_val = (uint32_t)const_addr->getUniqueInteger().getLimitedValue(0xFFFFFFFF);
					if (PowerPC::IsOptimizableRAMAddress(const_val))
					{
						LoadInst* mem_base = func->GetGlobalMemory();
						Value* mem_offset = builder.CreateGEP(mem_base, {const_addr});
						mem_offset = builder.CreatePointerCast(mem_offset, llvm_type);
						Value* inst_to_rep;
						if (store)
						{
							Value* store_val = inst->getArgOperand(0);
							inst_to_rep = builder.CreateStore(store_val, mem_offset);
						}
						else
						{
							inst_to_rep = builder.CreateLoad(mem_offset);
						}

						made_filthy = true;
						inst->replaceAllUsesWith(inst_to_rep);
						to_delete.push_back(inst);
					}
					else if (PowerPC::IsOptimizableMMIOAccess(const_val, size))
					{
						bool has_res = false;
						Value* inst_to_rep;
						if (store)
						{
							Value* store_val = inst->getArgOperand(0);
							switch (size)
							{
							case 8:
							{
								LLVMMMIOWriteCodeGenerator<u8> gen(&builder, store_val, const_val);
								Memory::mmio_mapping->GetHandlerForWrite<u8>(const_val).Visit(gen);
								inst_to_rep = gen.GetResultInst();
								has_res = gen.HasResult();
							}
							break;
							case 16:
							{
								LLVMMMIOWriteCodeGenerator<u16> gen(&builder, store_val, const_val);
								Memory::mmio_mapping->GetHandlerForWrite<u16>(const_val).Visit(gen);
								inst_to_rep = gen.GetResultInst();
								has_res = gen.HasResult();
							}
							break;
							case 32:
							{
								LLVMMMIOWriteCodeGenerator<u32> gen(&builder, store_val, const_val);
								Memory::mmio_mapping->GetHandlerForWrite<u32>(const_val).Visit(gen);
								inst_to_rep = gen.GetResultInst();
								has_res = gen.HasResult();
							}
							break;
							}
						}
						else
						{
							switch (size)
							{
							case 8:
							{
								LLVMMMIOReadCodeGenerator<u8> gen(&builder, const_val);
								Memory::mmio_mapping->GetHandlerForRead<u8>(const_val).Visit(gen);
								inst_to_rep = gen.GetResultInst();
								has_res = gen.HasResult();
							}
							break;
							case 16:
							{
								LLVMMMIOReadCodeGenerator<u16> gen(&builder, const_val);
								Memory::mmio_mapping->GetHandlerForRead<u16>(const_val).Visit(gen);
								inst_to_rep = gen.GetResultInst();
								has_res = gen.HasResult();
							}
							break;
							case 32:
							{
								LLVMMMIOReadCodeGenerator<u32> gen(&builder, const_val);
								Memory::mmio_mapping->GetHandlerForRead<u32>(const_val).Visit(gen);
								inst_to_rep = gen.GetResultInst();
								has_res = gen.HasResult();
							}
							break;
							}
						}

						if (has_res)
						{
							made_filthy = true;
							if (inst_to_rep)
								inst->replaceAllUsesWith(inst_to_rep);
							to_delete.push_back(inst);
						}
					}
					else if (false && store && PowerPC::IsOptimizableGatherPipeWrite(const_val))
					{
						// XXX: Doesn't yet work
						Value* store_val = inst->getArgOperand(0);
						Value* gatherpipe = func->GetGatherPipe();
						Value* gatherpipe_count = func->LoadGatherPipeCount();
						gatherpipe = builder.CreateGEP(gatherpipe, {builder.CreateZExt(gatherpipe_count, builder.getInt64Ty())});
						gatherpipe_count = builder.CreateAdd(gatherpipe_count, builder.getInt32(size >> 3));
						gatherpipe = builder.CreatePointerCast(gatherpipe, llvm_type);
						StoreInst* inst_to_rep = builder.CreateStore(store_val, gatherpipe);
						func->StoreGatherPipeCount(gatherpipe_count);

						made_filthy = true;
						inst->replaceAllUsesWith(inst_to_rep);
						to_delete.push_back(inst);
					}
				}
			}
		}
	} while (++start != end);

	// Erase all the instructions that we need to
	for (auto it : to_delete)
		it->eraseFromParent();

	return made_filthy;
}

void ConstantLoadStorePass::getAnalysisUsage(AnalysisUsage &Info) const
{
	Info.setPreservesCFG();
}

