// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <llvm/InitializePasses.h>
#include <llvm/LinkAllPasses.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/IR/IRPrintingPasses.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Transforms/IPO/PassManagerBuilder.h>

#include "Common/StringUtil.h"

#include "VideoCommon/VertexLoaderManager.h"
#include "VideoCommon/VertexLoaderLLVM/VertexLoaderLLVM.h"

using namespace llvm;

static const float scale_factors[] =
{
	1.0 / (1ULL <<  0), 1.0 / (1ULL <<  1), 1.0 / (1ULL <<  2), 1.0 / (1ULL <<  3),
	1.0 / (1ULL <<  4), 1.0 / (1ULL <<  5), 1.0 / (1ULL <<  6), 1.0 / (1ULL <<  7),
	1.0 / (1ULL <<  8), 1.0 / (1ULL <<  9), 1.0 / (1ULL << 10), 1.0 / (1ULL << 11),
	1.0 / (1ULL << 12), 1.0 / (1ULL << 13), 1.0 / (1ULL << 14), 1.0 / (1ULL << 15),
	1.0 / (1ULL << 16), 1.0 / (1ULL << 17), 1.0 / (1ULL << 18), 1.0 / (1ULL << 19),
	1.0 / (1ULL << 20), 1.0 / (1ULL << 21), 1.0 / (1ULL << 22), 1.0 / (1ULL << 23),
	1.0 / (1ULL << 24), 1.0 / (1ULL << 25), 1.0 / (1ULL << 26), 1.0 / (1ULL << 27),
	1.0 / (1ULL << 28), 1.0 / (1ULL << 29), 1.0 / (1ULL << 30), 1.0 / (1ULL << 31),
};

template<typename... Args>
FunctionType* GenerateFunctionType(Type* ret_type, Args... args)
{
	std::vector<Type*> types = {args...};
	return FunctionType::get(ret_type, types, false);
}

// Utilities
LoadInst* VertexLoaderLLVM::Read_U8(Value* offset)
{
	Value* offset_val = m_builder->CreateGEP(m_base_src, {offset});
	return m_builder->CreateAlignedLoad(offset_val, 1);
}

LoadInst* VertexLoaderLLVM::Read_U16(Value* offset)
{
	Type* llvm_type = m_builder->getInt16Ty()->getPointerTo();
	Value* mem_offset = m_builder->CreateGEP(m_base_src, {offset});
	mem_offset = m_builder->CreatePointerCast(mem_offset, llvm_type);
	return m_builder->CreateAlignedLoad(mem_offset, 1);
}

LoadInst* VertexLoaderLLVM::Read_U32(Value* offset)
{
	Type* llvm_type = m_builder->getInt32Ty()->getPointerTo();
	Value* mem_offset = m_builder->CreateGEP(m_base_src, {offset});
	mem_offset = m_builder->CreatePointerCast(mem_offset, llvm_type);
	return m_builder->CreateAlignedLoad(mem_offset, 1);
}

Value* VertexLoaderLLVM::Read_Vector(Type* type, int num_elem, Value* offset)
{
	Type* vector_type = VectorType::get(type, num_elem);
	Value* loc_val = m_builder->CreateGEP(m_base_src, {offset});
	loc_val = m_builder->CreatePointerCast(loc_val, vector_type->getPointerTo());
	return m_builder->CreateAlignedLoad(loc_val, 1);
}

LoadInst* VertexLoaderLLVM::Read_Stride(Value* offset)
{
	GlobalVariable* array_stride_global = m_main_mod->getNamedGlobal("array_stride");
	LoadInst* array_stride = m_builder->CreateLoad(array_stride_global, m_builder->getInt32(0), "array_stride");
	Value* mem_offset = m_builder->CreateGEP(array_stride, {m_builder->getInt32(0), m_builder->getInt32(0), offset});
	return m_builder->CreateAlignedLoad(mem_offset, 1);
}

LoadInst* VertexLoaderLLVM::Read_ArrayBase(Value* offset)
{
	GlobalVariable* array_bases_global = m_main_mod->getNamedGlobal("cached_array_bases");
	LoadInst* array_stride = m_builder->CreateLoad(array_bases_global, m_builder->getInt32(0), "cached_array_bases");
	Value* mem_offset = m_builder->CreateGEP(array_stride, {m_builder->getInt32(0), m_builder->getInt32(0), offset});
	return m_builder->CreateAlignedLoad(mem_offset, 1);
}

LoadInst* VertexLoaderLLVM::Read_F32(Value* offset)
{
	Type* llvm_type = m_builder->getFloatTy()->getPointerTo();
	Value* mem_offset = m_builder->CreateGEP(m_base_src, {offset});
	mem_offset = m_builder->CreatePointerCast(mem_offset, llvm_type);
	return m_builder->CreateAlignedLoad(mem_offset, 1);
}

void VertexLoaderLLVM::Write_U8(Value* val, Value* offset)
{
	Value* offset_val = m_builder->CreateGEP(m_base_dest, {offset});
	m_builder->CreateAlignedStore(val, offset_val, 1);
}

void VertexLoaderLLVM::Write_U16(Value* val, Value* offset)
{
	Type* llvm_type = m_builder->getIntNTy(16)->getPointerTo();
	Value* mem_offset = m_builder->CreateGEP(m_base_dest, {offset});
	mem_offset = m_builder->CreatePointerCast(mem_offset, llvm_type);
	m_builder->CreateAlignedStore(val, mem_offset, 1);
}

void VertexLoaderLLVM::Write_U32(Value* val, Value* offset)
{
	Type* llvm_type = m_builder->getIntNTy(32)->getPointerTo();
	Value* mem_offset = m_builder->CreateGEP(m_base_dest, {offset});
	mem_offset = m_builder->CreatePointerCast(mem_offset, llvm_type);
	m_builder->CreateAlignedStore(val, mem_offset, 1);
}

void VertexLoaderLLVM::Write_F32(Value* val, Value* offset)
{
	Type* llvm_type = m_builder->getFloatTy()->getPointerTo();
	Value* mem_offset = m_builder->CreateGEP(m_base_dest, {offset});
	mem_offset = m_builder->CreatePointerCast(mem_offset, llvm_type);
	m_builder->CreateAlignedStore(val, mem_offset, 1);
}

void VertexLoaderLLVM::Write_Vector(Value* val, Value* offset)
{
	Value* mem_offset = m_builder->CreateGEP(m_base_dest, {offset});
	mem_offset = m_builder->CreatePointerCast(mem_offset, val->getType()->getPointerTo());
	m_builder->CreateAlignedStore(val, mem_offset, 1);
}

void VertexLoaderLLVM::Write_ArrayBase(Value* val, Value* off1, Value* off2)
{
	GlobalVariable* position_cache_global = m_main_mod->getNamedGlobal("position_cache");
	Value* mem_offset = m_builder->CreateGEP(position_cache_global, {off1});
	mem_offset = m_builder->CreatePointerCast(mem_offset, val->getType()->getPointerTo());
	m_builder->CreateAlignedStore(val, mem_offset, 1);
}

void VertexLoaderLLVM::Write_PositionIndex(Value* val, Value* offset)
{
	GlobalVariable* position_idx_global = m_main_mod->getNamedGlobal("position_mtx_idx");

	Value* mem_offset = m_builder->CreateGEP(position_idx_global, {offset});
	mem_offset = m_builder->CreatePointerCast(mem_offset, m_builder->getInt8Ty()->getPointerTo());
	m_builder->CreateAlignedStore(val, mem_offset, 1);
}

VertexLoaderLLVM::VertexLoaderLLVM(const TVtxDesc& vtx_desc, const VAT& vtx_att)
	: VertexLoaderBase(vtx_desc, vtx_att)
{
	if (!IsInitialized())
		return;

	InitializeAllTargets();
	InitializeAllAsmPrinters();
	InitializeAllTargetMCs();
	InitializeNativeTarget();
	InitializeNativeTargetAsmPrinter();
	LLVMContext& con = getGlobalContext();

	m_main_mod = new Module("Dolphin LLVM Vertex Loader", con);
	Module* dummy_mod = new Module("Dummy", con);

	// Generate Engine
	m_engine_builder = new EngineBuilder(std::unique_ptr<Module>(dummy_mod));
	m_engine_builder->setEngineKind(EngineKind::JIT);

	SmallVector<std::string, 0> attrs;
	std::string arch = "";
//#define AArch64
#ifdef AArch64
	std::string cpu = "";
#else
	std::string cpu = "haswell";
#endif

	TargetMachine* default_target = m_engine_builder->selectTarget();
	Triple test("aarch64", "None", "Unknown");

	TargetMachine* target = m_engine_builder->selectTarget(
#ifdef AArch64
		test,
#else
		default_target->getTargetTriple(),
#endif
		arch, cpu, attrs);

	m_engine = m_engine_builder->create(target);
	if (!m_engine)
		m_engine = m_engine_builder->create();

	m_builder = new IRBuilder<>(m_main_mod->getContext());

	// Generate Function
	FunctionType* main_type = GenerateFunctionType(Type::getInt32Ty(con),
		Type::getInt8Ty(con)->getPointerTo(),
		Type::getInt8Ty(con)->getPointerTo(),
		Type::getInt32Ty(con));

	std::string func_name = StringFromFormat("VertexLoader");

	// Generate the function
	m_func = Function::Create(
		main_type,
		Function::ExternalLinkage,
		func_name,
		m_main_mod);
	m_func->setCallingConv(CallingConv::C);

	m_entry = BasicBlock::Create(con, "entry", m_func);
	m_builder->SetInsertPoint(m_entry);

	if (m_VtxDesc.Position & MASK_INDEXED)
		m_skip_vertex = BasicBlock::Create(con, "skip_vertex", m_func);

	m_debug_enabled = false;
	GenerateVertexLoader();
}

VertexLoaderLLVM::~VertexLoaderLLVM()
{
	delete m_engine;
}

Value* VertexLoaderLLVM::GetVertexAddr(int array, u64 attribute, u32 offset)
{
	LLVMContext& con = getGlobalContext();

	Value* ret_val;
	if (attribute & MASK_INDEXED)
	{
		Value* index_val;
		if (attribute == INDEX8)
		{
			index_val = Read_U8(m_builder->getInt32(m_src_ofs));
			m_src_ofs += 1;
		}
		else
		{
			index_val = Read_U16(m_builder->getInt32(m_src_ofs));
			m_src_ofs += 2;
			std::vector<Type *> arg_type;
			arg_type.push_back(m_builder->getInt16Ty());

			std::vector<Value *> args;
			args.push_back(index_val);

			Function* bswap_fun = Intrinsic::getDeclaration(m_main_mod, Intrinsic::bswap, arg_type);
			index_val = m_builder->CreateCall(bswap_fun, args);
		}

		if (array == ARRAY_POSITION)
		{
			Value* cmp_val;
			if (attribute == INDEX8)
				cmp_val = m_builder->getInt8(0xFF);
			else
				cmp_val = m_builder->getInt16(0xFFFF);
			Value* cmp_result = m_builder->CreateICmpEQ(index_val, cmp_val);
			BasicBlock* new_main = BasicBlock::Create(con, "main", m_func);
			m_builder->CreateCondBr(cmp_result, m_skip_vertex, new_main);
			m_builder->SetInsertPoint(new_main);
		}

		Value* stride = Read_Stride(m_builder->getInt32(array));
		index_val = m_builder->CreateMul(m_builder->CreateZExt(index_val, m_builder->getInt32Ty()), stride);
		Value* array_base = Read_ArrayBase(m_builder->getInt32(array));
		ret_val = m_builder->CreateAdd(m_builder->CreatePtrToInt(array_base, m_builder->getInt64Ty()), m_builder->CreateZExt(index_val, m_builder->getInt64Ty()));
		ret_val = m_builder->CreateAdd(ret_val, m_builder->getInt64(offset));
	}
	else
	{
		ret_val = m_builder->getInt64(m_src_ofs + offset);
	}
	return ret_val;
}

int VertexLoaderLLVM::ReadVertex(u64 attribute, int format, int count_in, int count_out, bool dequantize, u8 scaling_exponent, AttributeFormat* native_format, Value* offset)
{
	LLVMContext& con = getGlobalContext();

	int elem_size = 1 << (format / 2);
	int load_bytes = elem_size * count_in;
	int load_size = load_bytes == 1 ? 1 : load_bytes <= 2 ? 2 : load_bytes <= 4 ? 4 : load_bytes <= 8 ? 8 : 16;
	load_size <<= 3;
	elem_size <<= 3;
	Type* const type_mapping[] =
	{
		m_builder->getInt8Ty(),
		m_builder->getInt8Ty(),
		m_builder->getInt16Ty(),
		m_builder->getInt16Ty(),
		m_builder->getInt32Ty(),
	};
	int read_count_in = 4; //count_in == 3 ? 4 : count_in;
	int write_count_out = 4; //count_out == 3 ? 4 : count_out;
	Type* read_type = type_mapping[format];
	Type* read_vector_type = VectorType::get(read_type, read_count_in);
	Type* write_type = VectorType::get(m_builder->getFloatTy(), read_count_in);

	Value* input_val = Read_Vector(read_type, read_count_in, offset);

	std::vector<Type *> arg_type;
	arg_type.push_back(read_vector_type);

	std::vector<Value *> args;
	args.push_back(input_val);

	Function* bswap_fun = Intrinsic::getDeclaration(m_main_mod, Intrinsic::bswap, arg_type);

	switch (format)
	{
	case FORMAT_FLOAT:
		input_val = m_builder->CreateCall(bswap_fun, args);
	break;
	case FORMAT_UBYTE:
		input_val = m_builder->CreateUIToFP(input_val, write_type);
	break;
	case FORMAT_BYTE:
		input_val = m_builder->CreateSIToFP(input_val, write_type);
	break;
	case FORMAT_USHORT:
		input_val = m_builder->CreateCall(bswap_fun, args);
		input_val = m_builder->CreateUIToFP(input_val, write_type);
	break;
	case FORMAT_SHORT:
		input_val = m_builder->CreateCall(bswap_fun, args);
		input_val = m_builder->CreateSIToFP(input_val, write_type);
	break;
	};

	if (format != FORMAT_FLOAT)
	{
		if (dequantize && scaling_exponent)
		{
			Value* scale_factor = ConstantFP::get(Type::getFloatTy(con), scale_factors[scaling_exponent]);
			Value* scale_vector = m_builder->CreateVectorSplat(write_count_out, scale_factor);
			input_val = m_builder->CreateFMul(input_val, scale_vector);
		}
	}

	const u32 write_size = count_out == 3 ? 128 : count_out * 32;
	const u32 mask = count_out == 3 ? 0xF : count_out == 2 ? 0x7 : 0x3;

	Write_Vector(input_val, m_builder->getInt32(m_dst_ofs));

	// Z-Freeze
	if (native_format == &m_native_vtx_decl.position)
	{
		BasicBlock* store_block = BasicBlock::Create(con, "z_freeze_store_position", m_func);
		BasicBlock* new_main = BasicBlock::Create(con, "main", m_func);
		Value* cmp_res = m_builder->CreateICmpSGT(m_loops_remaining, m_builder->getInt32(3));
		m_builder->CreateCondBr(cmp_res, new_main, store_block);
		m_builder->SetInsertPoint(store_block);
			Value* cache_offset = m_builder->CreateShl(m_loops_remaining, m_builder->getInt32(4));
			cache_offset = m_builder->CreateSub(cache_offset, m_builder->getInt32(-16));
			Write_ArrayBase(input_val, cache_offset, m_builder->getInt32(0));
		m_builder->CreateBr(new_main);
		m_builder->SetInsertPoint(new_main);
	}

	native_format->components = count_out;
	native_format->enable = true;
	native_format->offset = m_dst_ofs;
	native_format->type = VAR_FLOAT;
	native_format->integer = false;
	m_dst_ofs += sizeof(float) * count_out;

	if (attribute == DIRECT)
		m_src_ofs += load_bytes;

	return load_bytes;
}

void VertexLoaderLLVM::ReadColor(u64 attribute, int format, Value* offset)
{
	int load_bytes = 0;
	switch (format)
	{
	case FORMAT_24B_888:
	case FORMAT_32B_888x:
	case FORMAT_32B_8888:
	{
		Value* color = Read_U32(offset);
		if (format != FORMAT_32B_8888)
			m_builder->CreateOr(color, m_builder->getInt32(0xFF000000));
		Write_U32(color, m_builder->getInt32(m_dst_ofs));
		load_bytes = 3 + (format != FORMAT_24B_888);
	}
	break;
	case FORMAT_16B_565:
	{
		//                   RRRRRGGG GGGBBBBB
		// AAAAAAAA BBBBBBBB GGGGGGGG RRRRRRRR

		Value* color = Read_U16(offset);
		std::vector<Type *> arg_type;
		arg_type.push_back(m_builder->getInt16Ty());

		std::vector<Value *> args;
		args.push_back(color);

		Function* bswap_fun = Intrinsic::getDeclaration(m_main_mod, Intrinsic::bswap, arg_type);
		color = m_builder->CreateCall(bswap_fun, args);

		color = m_builder->CreateZExt(color, m_builder->getInt32Ty());

		Value* out_color, *tmp;
		tmp = m_builder->CreateLShr(color, m_builder->getInt32(8));
		out_color = m_builder->CreateAnd(tmp, m_builder->getInt32(0xF8));

		tmp = m_builder->CreateShl(color, m_builder->getInt32(5));
		tmp = m_builder->CreateAnd(tmp, m_builder->getInt32(0xFC00));
		out_color = m_builder->CreateOr(out_color, tmp);

		tmp = m_builder->CreateShl(color, m_builder->getInt32(19));
		tmp = m_builder->CreateAnd(tmp, m_builder->getInt32(0xF80000));
		out_color = m_builder->CreateOr(out_color, tmp);

		tmp = m_builder->CreateLShr(color, m_builder->getInt32(5));
		tmp = m_builder->CreateAnd(tmp, m_builder->getInt32(0x070007));
		out_color = m_builder->CreateOr(out_color, tmp);

		tmp = m_builder->CreateLShr(color, m_builder->getInt32(6));
		tmp = m_builder->CreateAnd(tmp, m_builder->getInt32(0x300));
		out_color = m_builder->CreateOr(out_color, tmp);

		out_color = m_builder->CreateOr(color, m_builder->getInt32(0xFF000000));

		Write_U32(out_color, m_builder->getInt32(m_dst_ofs));
		load_bytes = 2;
	}
	break;
	case FORMAT_16B_4444:
	{
		//                   BBBBAAAA RRRRGGGG
		// AAAAAAAA BBBBBBBB GGGGGGGG RRRRRRRR

		Value* color = Read_U16(offset);
		color = m_builder->CreateZExt(color, m_builder->getInt32Ty());

		Value* out_color, *tmp;

		out_color = m_builder->CreateAnd(color, m_builder->getInt32(0xF0));

		tmp = m_builder->CreateAnd(color, m_builder->getInt32(0xF));
		tmp = m_builder->CreateShl(tmp, m_builder->getInt32(12));
		out_color = m_builder->CreateOr(out_color, tmp);

		tmp = m_builder->CreateAnd(color, m_builder->getInt32(0xF00));
		tmp = m_builder->CreateShl(tmp, m_builder->getInt32(8));
		out_color = m_builder->CreateOr(out_color, tmp);

		tmp = m_builder->CreateAnd(color, m_builder->getInt32(0x0F00));
		tmp = m_builder->CreateShl(tmp, m_builder->getInt32(20));
		out_color = m_builder->CreateOr(out_color, tmp);

		out_color = m_builder->CreateOr(out_color, m_builder->CreateLShr(out_color, m_builder->getInt32(4)));

		Write_U32(out_color, m_builder->getInt32(m_dst_ofs));
		load_bytes = 2;
	}
	break;
	case FORMAT_24B_6666:
	{
		//          RRRRRRGG GGGGBBBB BBAAAAAA
		// AAAAAAAA BBBBBBBB GGGGGGGG RRRRRRRR
		// u32 col = (val >> 16) & 0x000000FC;
		// col |=    (val >>  2) & 0x0000FC00;
		// col |=    (val << 12) & 0x00FC0000;
		// col |=    (val << 26) & 0xFC000000;
		// col |=    (col >>  6) & 0x03030303;

		Value* color = Read_U32(m_builder->CreateSub(offset, m_builder->getInt32(1)));
		std::vector<Type *> arg_type;
		arg_type.push_back(m_builder->getInt32Ty());

		std::vector<Value *> args;
		args.push_back(color);

		Function* bswap_fun = Intrinsic::getDeclaration(m_main_mod, Intrinsic::bswap, arg_type);
		color = m_builder->CreateCall(bswap_fun, args);

		Value* out_color, *tmp;

		tmp = m_builder->CreateLShr(color, m_builder->getInt32(16));
		out_color = m_builder->CreateAnd(tmp, m_builder->getInt32(0xFC));

		tmp = m_builder->CreateLShr(color, m_builder->getInt32(2));
		tmp = m_builder->CreateAnd(tmp, m_builder->getInt32(0xFC00));
		out_color = m_builder->CreateOr(out_color, tmp);

		tmp = m_builder->CreateShl(color, m_builder->getInt32(12));
		tmp = m_builder->CreateAnd(tmp, m_builder->getInt32(0xFC0000));
		out_color = m_builder->CreateOr(out_color, tmp);

		tmp = m_builder->CreateShl(color, m_builder->getInt32(26));
		tmp = m_builder->CreateAnd(tmp, m_builder->getInt32(0xFC000000));
		out_color = m_builder->CreateOr(out_color, tmp);


		tmp = m_builder->CreateLShr(out_color, m_builder->getInt32(6));
		tmp = m_builder->CreateAnd(tmp, m_builder->getInt32(0x03030303));
		out_color = m_builder->CreateOr(out_color, tmp);

		Write_U32(out_color, m_builder->getInt32(m_dst_ofs));

		load_bytes = 3;
	}
	break;
	}
	if (attribute == DIRECT)
		m_src_ofs += load_bytes;
}

void VertexLoaderLLVM::GenerateVertexLoader()
{
	LLVMContext& con = getGlobalContext();

	std::vector<Argument*> args;
	for (Argument& arg : m_func->args())
		args.push_back(&arg);

	Value* m_arg_src = args[0];
	Value* m_arg_dest = args[1];
	Value* m_arg_count = args[2];

	m_src_ofs = 0;
	m_dst_ofs = 0;

	/* Constants */
	// Cached array bases
	std::vector<Type*> struct_values =
	{
		ArrayType::get(m_builder->getInt8Ty()->getPointerTo(), 12),
	};

	Type* cached_arraybases_type = StructType::create(struct_values, "cached_array_bases", false);
	Constant* const_arraybases_val = ConstantInt::getIntegerValue(cached_arraybases_type->getPointerTo(), APInt(64, (uint64_t)&VertexLoaderManager::cached_arraybases));
	m_main_mod->getOrInsertGlobal("cached_array_bases", cached_arraybases_type->getPointerTo());
	GlobalVariable* arraybases_global = m_main_mod->getNamedGlobal("cached_array_bases");
	arraybases_global->setInitializer(const_arraybases_val);

	// position cache
	std::vector<Type*> position_cache_values =
	{
		ArrayType::get(VectorType::get(m_builder->getFloatTy(), 4), 3),
	};
	Type* position_cache_type = StructType::create(position_cache_values, "position_cache", false);
	Constant* const_position_val = ConstantInt::getIntegerValue(position_cache_type->getPointerTo(), APInt(64, (uint64_t)&VertexLoaderManager::position_cache));
	m_main_mod->getOrInsertGlobal("position_cache", position_cache_type->getPointerTo());
	GlobalVariable* position_cache_global = m_main_mod->getNamedGlobal("position_cache");
	position_cache_global->setInitializer(const_position_val);

	// position matrix index
	std::vector<Type*> position_mtx_values =
	{
		ArrayType::get(m_builder->getFloatTy(), 3),
	};
	Type* position_mtx_type = StructType::create(position_cache_values, "position_mtx_idx", false);
	Constant* const_position_idx_val = ConstantInt::getIntegerValue(position_mtx_type->getPointerTo(), APInt(64, (uint64_t)&VertexLoaderManager::position_matrix_index));
	m_main_mod->getOrInsertGlobal("position_mtx_idx", position_mtx_type->getPointerTo());
	GlobalVariable* position_idx_global = m_main_mod->getNamedGlobal("position_mtx_idx");
	position_idx_global->setInitializer(const_position_idx_val);

	// Array strides
	std::vector<Type*> array_stride_struct =
	{
		ArrayType::get(m_builder->getInt32Ty(), 32),
	};

	Type* array_strides_type = StructType::create(array_stride_struct, "array_stride", false);
	Constant* const_array_strides_val = ConstantInt::getIntegerValue(array_strides_type->getPointerTo(), APInt(64, (uint64_t)&g_main_cp_state.array_strides));
	m_main_mod->getOrInsertGlobal("array_stride", array_strides_type->getPointerTo());
	GlobalVariable* array_stride_global = m_main_mod->getNamedGlobal("array_stride");
	array_stride_global->setInitializer(const_array_strides_val);

	m_main = BasicBlock::Create(con, "main", m_func);
	// Entry does nothing, just lets us know our arguments
	m_builder->CreateBr(m_main);
	m_builder->SetInsertPoint(m_main);

	// Let's set up our phi values

	// Current source base
	m_base_src = m_builder->CreatePHI(m_builder->getInt8Ty()->getPointerTo(), 11, "base_src");
	m_base_src->addIncoming(m_arg_src, m_entry);

	// Current base destination
	m_base_dest = m_builder->CreatePHI(m_builder->getInt8Ty()->getPointerTo(), 11, "base_dest");
	m_base_dest->addIncoming(m_arg_dest, m_entry);

	// Current loops remaining
	m_loops_remaining = m_builder->CreatePHI(m_builder->getInt32Ty(), 11, "loops_remaining");
	m_loops_remaining->addIncoming(m_arg_count, m_entry);

	// Number of vertices skipped
	if (m_VtxDesc.Position & MASK_INDEXED)
	{
		m_skipped_verts = m_builder->CreatePHI(m_builder->getInt32Ty(), 11, "skipped_verts");
		m_skipped_verts->addIncoming(m_builder->getInt32(0), m_entry);
	}

	if (m_VtxDesc.PosMatIdx)
	{
		Value* pos = Read_U8(m_builder->getInt32(m_dst_ofs));
		pos = m_builder->CreateAnd(pos, m_builder->getInt8(0x3F));
		Write_U8(pos, m_builder->getInt32(m_dst_ofs));

		BasicBlock* store_block = BasicBlock::Create(con, "z_freeze_store_position_idx", m_func);
		BasicBlock* new_main = BasicBlock::Create(con, "main", m_func);
		Value* cmp_res = m_builder->CreateICmpSGT(m_loops_remaining, m_builder->getInt32(3));
		m_builder->CreateCondBr(cmp_res, new_main, store_block);
		m_builder->SetInsertPoint(store_block);
			Write_PositionIndex(pos, m_loops_remaining);
		m_builder->CreateBr(new_main);
		m_builder->SetInsertPoint(new_main);

		m_native_components |= VB_HAS_POSMTXIDX;
		m_native_vtx_decl.posmtx.components = 4;
		m_native_vtx_decl.posmtx.enable = true;
		m_native_vtx_decl.posmtx.offset = m_dst_ofs;
		m_native_vtx_decl.posmtx.type = VAR_UNSIGNED_BYTE;
		m_native_vtx_decl.posmtx.integer = true;

		m_src_ofs += sizeof(u8);
		m_dst_ofs += sizeof(u32);
	}

	u32 texmatidx_ofs[8];
	const u64 tm[8] = {
		m_VtxDesc.Tex0MatIdx, m_VtxDesc.Tex1MatIdx, m_VtxDesc.Tex2MatIdx, m_VtxDesc.Tex3MatIdx,
		m_VtxDesc.Tex4MatIdx, m_VtxDesc.Tex5MatIdx, m_VtxDesc.Tex6MatIdx, m_VtxDesc.Tex7MatIdx,
	};
	for (int i = 0; i < 8; i++)
	{
		if (tm[i])
			texmatidx_ofs[i] = m_src_ofs++;
	}

	// Position
	{
		int elem_size = 1 << (m_VtxAttr.PosFormat / 2);
		int load_bytes = elem_size * (m_VtxAttr.PosElements + 2);
		int load_size = load_bytes == 1 ? 1 : load_bytes <= 2 ? 2 : load_bytes <= 4 ? 4 : load_bytes <= 8 ? 8 : 16;
		int pos_elements = m_VtxAttr.PosElements + 2;
		load_size <<= 3;

		Value* offset = GetVertexAddr(ARRAY_POSITION, m_VtxDesc.Position);
		ReadVertex(m_VtxDesc.Position, m_VtxAttr.PosFormat, pos_elements, pos_elements,
		           m_VtxAttr.ByteDequant, m_VtxAttr.PosFrac, &m_native_vtx_decl.position, offset);
	}

	if (m_VtxDesc.Normal)
	{
		static const u8 map[8] = {7, 6, 15, 14};
		u8 scaling_exponent = map[m_VtxAttr.NormalFormat];

		u32 int_off = 0;
		for (int i = 0; i < (m_VtxAttr.NormalElements ? 3 : 1); i++)
		{
			if (!i || m_VtxAttr.NormalIndex3)
			{
				int elem_size = 1 << (m_VtxAttr.NormalFormat / 2);

				int load_bytes = elem_size * 3;
				int load_size = load_bytes == 1 ? 1 : load_bytes <= 2 ? 2 : load_bytes <= 4 ? 4 : load_bytes <= 8 ? 8 : 16;

				Value* offset = GetVertexAddr(ARRAY_NORMAL, m_VtxDesc.Normal, int_off);

				int_off += i * elem_size * 3;

				int bytes_read = ReadVertex(m_VtxDesc.Normal, m_VtxAttr.NormalFormat, 3, 3,
				                            true, scaling_exponent, &m_native_vtx_decl.normals[i], offset);

				int_off += bytes_read;
			}
		}

		m_native_components |= VB_HAS_NRM0;
		if (m_VtxAttr.NormalElements)
			m_native_components |= VB_HAS_NRM1 | VB_HAS_NRM2;
	}

	const u64 col[2] = {m_VtxDesc.Color0, m_VtxDesc.Color1};
	for (int i = 0; i < 2; i++)
	{
		m_native_vtx_decl.colors[i].components = 4;
		m_native_vtx_decl.colors[i].type = VAR_UNSIGNED_BYTE;
		m_native_vtx_decl.colors[i].integer = false;

		if (col[i])
		{
			Value* offset = GetVertexAddr(ARRAY_COLOR + i, col[i]);

			ReadColor(col[i], m_VtxAttr.color[i].Comp, offset);
			m_native_components |= VB_HAS_COL0 << i;
			m_native_vtx_decl.colors[i].components = 4;
			m_native_vtx_decl.colors[i].enable = true;
			m_native_vtx_decl.colors[i].offset = m_dst_ofs;
			m_native_vtx_decl.colors[i].type = VAR_UNSIGNED_BYTE;
			m_native_vtx_decl.colors[i].integer = false;
			m_dst_ofs += 4;
		}
	}

	const u64 tc[8] = {
		m_VtxDesc.Tex0Coord, m_VtxDesc.Tex1Coord, m_VtxDesc.Tex2Coord, m_VtxDesc.Tex3Coord,
		m_VtxDesc.Tex4Coord, m_VtxDesc.Tex5Coord, m_VtxDesc.Tex6Coord, m_VtxDesc.Tex7Coord,
	};

	for (int i = 0; i < 8; i++)
	{
		m_native_vtx_decl.texcoords[i].offset = m_dst_ofs;
		m_native_vtx_decl.texcoords[i].type = VAR_FLOAT;
		m_native_vtx_decl.texcoords[i].integer = false;

		int elements = m_VtxAttr.texCoord[i].Elements + 1;
		if (tc[i])
		{
			m_native_components |= VB_HAS_UV0 << i;

			int elem_size = 1 << (m_VtxAttr.texCoord[i].Format / 2);
			int load_bytes = elem_size * (elements + 2);
			int load_size = load_bytes == 1 ? 1 : load_bytes <= 2 ? 2 : load_bytes <= 4 ? 4 : load_bytes <= 8 ? 8 : 16;
			load_size <<= 3;

			u8 scaling_exponent = m_VtxAttr.texCoord[i].Frac;

			Value* offset = GetVertexAddr(ARRAY_TEXCOORD0 + i, tc[i]);

			ReadVertex(tc[i], m_VtxAttr.texCoord[i].Format, elements, tm[i] ? 2 : elements,
			           m_VtxAttr.ByteDequant, scaling_exponent, &m_native_vtx_decl.texcoords[i], offset);
		}
		if (tm[i])
		{
			m_native_components |= VB_HAS_TEXMTXIDX0 << i;
			m_native_vtx_decl.texcoords[i].components = 3;
			m_native_vtx_decl.texcoords[i].enable = true;
			m_native_vtx_decl.texcoords[i].type = VAR_FLOAT;
			m_native_vtx_decl.texcoords[i].integer = false;

			Value* texmtxid = Read_U8(m_builder->getInt32(texmatidx_ofs[i]));
			texmtxid = m_builder->CreateUIToFP(texmtxid, m_builder->getFloatTy());

			if (tc[i])
			{
				Write_U32(texmtxid, m_builder->getInt32(m_dst_ofs));
				m_dst_ofs += sizeof(float);
			}
			else
			{
				m_native_vtx_decl.texcoords[i].offset = m_dst_ofs;

				Write_U32(m_builder->getInt32(0), m_builder->getInt32(m_dst_ofs));
				Write_U32(m_builder->getInt32(0), m_builder->getInt32(m_dst_ofs + 4));
				Write_U32(texmtxid, m_builder->getInt32(m_dst_ofs + 8));

				m_dst_ofs += sizeof(float) * 3;
			}
		}
	}

	// Prepare for the next vertex.
	Value* new_base_dest = m_builder->CreateAdd(m_builder->CreatePtrToInt(m_base_dest, m_builder->getInt64Ty()), m_builder->getInt64(m_dst_ofs));
	new_base_dest = m_builder->CreateIntToPtr(new_base_dest, m_builder->getInt8Ty()->getPointerTo());
	BasicBlock* footer = BasicBlock::Create(con, "footer", m_func);
	BasicBlock* footer2 = BasicBlock::Create(con, "footer2", m_func);
	m_builder->CreateBr(footer);

	BasicBlock* prev_block = m_builder->GetInsertBlock();
	// Footer
	m_builder->SetInsertPoint(footer);
	PHINode* skipped_verts;
	PHINode* our_base_dest = m_builder->CreatePHI(m_builder->getInt8Ty()->getPointerTo(), 5);
	if (m_VtxDesc.Position & MASK_INDEXED)
	{
		skipped_verts = m_builder->CreatePHI(m_builder->getInt32Ty(), 11, "new_mask_skip");
		skipped_verts->addIncoming(m_skipped_verts, prev_block);
		m_skipped_verts->addIncoming(skipped_verts, footer);

		our_base_dest->addIncoming(m_base_dest, m_skip_vertex);
	}
	our_base_dest->addIncoming(new_base_dest, prev_block);

	// Main->skip_vertex->footer->Main
	// Main->footer->main
	Value* new_base_src = m_builder->CreateAdd(m_builder->CreatePtrToInt(m_base_src, m_builder->getInt64Ty()), m_builder->getInt64(m_src_ofs));
	new_base_src = m_builder->CreateIntToPtr(new_base_src, m_builder->getInt8Ty()->getPointerTo());
	m_base_src->addIncoming(new_base_src, footer);

	Value* new_remaining = m_builder->CreateSub(m_loops_remaining, m_builder->getInt32(1));
	m_loops_remaining->addIncoming(new_remaining, footer);
	Value* cmp = m_builder->CreateICmpNE(m_loops_remaining, m_builder->getInt32(0));
	m_base_dest->addIncoming(our_base_dest, footer);
	m_builder->CreateCondBr(cmp, m_main, footer2);

	// Footer2
	m_builder->SetInsertPoint(footer2);

	if (m_VtxDesc.Position & MASK_INDEXED)
	{
		Value* ret_val = m_builder->CreateSub(m_arg_count, m_skipped_verts);
		m_builder->CreateRet(ret_val);

		m_builder->SetInsertPoint(m_skip_vertex);
		PHINode* skipped_verts2 = m_builder->CreatePHI(m_builder->getInt32Ty(), 11, "new_mask_skip");
		skipped_verts2->addIncoming(m_skipped_verts, m_main);
		Value* new_skipped_count = m_builder->CreateAdd(skipped_verts2, m_builder->getInt32(1));
		skipped_verts->addIncoming(new_skipped_count, m_skip_vertex);
		m_builder->CreateBr(footer);
	}
	else
	{
		m_builder->CreateRet(m_arg_count);
	}

	m_VertexSize = m_src_ofs;
	m_native_vtx_decl.stride = m_dst_ofs;

	legacy::PassManager PM;
	PassManagerBuilder Builder;
	Builder.OptLevel = 2;
	raw_ostream& out = outs();

	if (m_debug_enabled)
		PM.add(createPrintModulePass(out));

	verifyModule(*m_main_mod, &out);
	Builder.populateModulePassManager(PM);
	PM.run(*m_main_mod);

	m_engine->addModule(std::unique_ptr<Module>(m_main_mod));
	m_engine->finalizeObject();

	code_addr = (void*)m_engine->getFunctionAddress("VertexLoader");
}

int VertexLoaderLLVM::RunVertices(DataReader src, DataReader dst, int count)
{
	m_numLoadedVertices += count;
	return ((int (*)(u8* src, u8* dst, int count))code_addr)(src.GetPointer(), dst.GetPointer(), count);
}
