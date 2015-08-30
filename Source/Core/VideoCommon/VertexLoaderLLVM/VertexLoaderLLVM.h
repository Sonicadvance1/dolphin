// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

#include "VideoCommon/VertexLoaderBase.h"

class VertexLoaderLLVM : public VertexLoaderBase
{
private:
	llvm::ExecutionEngine* m_engine;
	llvm::Module* m_main_mod;
	llvm::Function* m_func;

	bool m_debug_enabled;

	// Arguments
	llvm::PHINode* m_base_src;
	llvm::PHINode* m_base_dest;
	llvm::PHINode* m_loops_remaining;
	llvm::PHINode* m_skipped_verts;

	llvm::EngineBuilder* m_engine_builder;

	// Blocks
	llvm::BasicBlock* m_entry;
	llvm::BasicBlock* m_skip_vertex;
	llvm::BasicBlock* m_main;

	llvm::IRBuilder<>* m_builder;

	u32 m_src_ofs;
	u32 m_dst_ofs;

	// Our final function pointer
	void* code_addr;

public:
	VertexLoaderLLVM(const TVtxDesc& vtx_desc, const VAT& vtx_att);
	~VertexLoaderLLVM();

protected:
	std::string GetName() const override { return "VertexLoaderLLVM"; }
	bool IsInitialized() override { return true; }
	int RunVertices(DataReader src, DataReader dst, int count) override;

private:
	void GenerateVertexLoader();

	// Utilities
	llvm::LoadInst* Read_U8(llvm::Value* offset);
	llvm::LoadInst* Read_U16(llvm::Value* offset);
	llvm::LoadInst* Read_U32(llvm::Value* offset);
	llvm::LoadInst* Read_F32(llvm::Value* offset);
	llvm::Value* Read_Vector(llvm::Type* type, int num_elem, llvm::Value* offset);

	llvm::LoadInst* Read_Stride(llvm::Value* offset);
	llvm::LoadInst* Read_ArrayBase(llvm::Value* offset);

	void Write_U8(llvm::Value* val, llvm::Value* offset);
	void Write_U16(llvm::Value* val, llvm::Value* offset);
	void Write_U32(llvm::Value* val, llvm::Value* offset);
	void Write_F32(llvm::Value* val, llvm::Value* offset);
	void Write_Vector(llvm::Value* val, llvm::Value* offset);
	void Write_ArrayBase(llvm::Value* val, llvm::Value* off1, llvm::Value* off2);
	void Write_PositionIndex(llvm::Value* val, llvm::Value* offset);

	llvm::Value* GetVertexAddr(int array, u64 attribute, u32 offset = 0);
	int ReadVertex(u64 attribute, int format, int count_in, int count_out, bool dequantize, u8 scaling_exponent, AttributeFormat* native_format, llvm::Value* offset);
	void ReadColor(u64 attribute, int format, llvm::Value* offset);

};
