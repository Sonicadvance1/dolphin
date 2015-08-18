// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <llvm/Pass.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>

#include "Core/PowerPC/JitLLVM/JitBinding.h"

class ConstantLoadStorePass : public llvm::BasicBlockPass
{
private:
	char m_ID;
	LLVMModule* m_mod;

public:
	ConstantLoadStorePass(char ID, LLVMModule* mod)
		: llvm::BasicBlockPass(ID), m_ID(ID), m_mod(mod)
	{}
	bool runOnBasicBlock(llvm::BasicBlock& BB) override;
	void getAnalysisUsage(llvm::AnalysisUsage &Info) const override;
};
