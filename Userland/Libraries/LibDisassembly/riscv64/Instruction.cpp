/*
 * Copyright (c) 2023, kleines Filmröllchen <filmroellchen@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "Instruction.h"
#include "Encoding.h"
#include "IM.h"
#include "Zicsr.h"
#include <AK/Assertions.h>
#include <AK/NonnullOwnPtr.h>
#include <AK/StdLibExtras.h>
#include <AK/String.h>
#include <AK/StringUtils.h>
#include <AK/TypeCasts.h>
#include <LibDisassembly/InstructionStream.h>

namespace Disassembly::RISCV64 {

MemoryAccessMode MemoryAccessMode::from_funct3(u8 funct3)
{
    auto width = static_cast<DataWidth>(funct3 & 0b11);
    bool is_signed = (funct3 & 0b100) == 0;
    return { width, is_signed ? Signedness::Signed : Signedness::Unsigned };
}

static NonnullOwnPtr<InstructionImpl> parse_full_impl(MajorOpcode opcode, u32 instruction)
{
    TODO();
}

static NonnullOwnPtr<InstructionImpl> parse_compressed_impl(CompressedOpcode opcode, u16 instruction)
{
    TODO();
}

NonnullOwnPtr<Instruction> Instruction::parse_full(u32 instruction)
{
    auto opcode = static_cast<MajorOpcode>(instruction & 0b11111'11);
    auto instruction_data = parse_full_impl(opcode, instruction);
    return adopt_own(*new (nothrow) Instruction(move(instruction_data), instruction));
}

NonnullOwnPtr<Instruction> Instruction::parse_compressed(u16 instruction)
{
    auto opcode = extract_compressed_opcode(instruction);
    auto instruction_data = parse_compressed_impl(opcode, instruction);
    return adopt_own(*new (nothrow) Instruction(move(instruction_data), instruction, CompressedTag {}));
}

NonnullOwnPtr<Instruction> Instruction::from_stream(InstructionStream& stream)
{
    u16 first_halfword = AK::convert_between_host_and_little_endian(stream.read16());
    if (is_compressed_instruction(first_halfword))
        return Instruction::parse_compressed(first_halfword);

    u16 second_halfword = AK::convert_between_host_and_little_endian(stream.read16());
    return Instruction::parse_full(first_halfword | (second_halfword << 16));
}

DeprecatedString Instruction::to_deprecated_string(u32 origin, Optional<SymbolProvider const&> symbol_provider) const
{
    return m_data->to_string(m_display_style, origin, symbol_provider).to_deprecated_string();
}

DeprecatedString Instruction::mnemonic() const
{
    return m_data->mnemonic().to_deprecated_string();
}

template<typename InstructionType>
bool simple_instruction_equals(InstructionType const& self, InstructionImpl const& instruction)
{
    if (is<InstructionType>(instruction))
        return self == static_cast<InstructionType const&>(instruction);
    return false;
}

#define MAKE_INSTRUCTION_EQUALS(InstructionType) \
    bool InstructionType::instruction_equals(InstructionImpl const& instruction) const { return simple_instruction_equals<InstructionType>(*this, instruction); }

#define ENUMERATE_INSTRUCTION_IMPLS(M)      \
    M(JumpAndLink)                          \
    M(JumpAndLinkRegister)                  \
    M(LoadUnsignedImmediate)                \
    M(AddUnsignedImmediateToProgramCounter) \
    M(ArithmeticImmediateInstruction)       \
    M(ArithmeticInstruction)                \
    M(MemoryLoad)                           \
    M(MemoryStore)                          \
    M(Branch)                               \
    M(EnvironmentCall)                      \
    M(EnvironmentBreak)                     \
    M(CSRInstruction)                       \
    M(CSRRegisterInstruction)               \
    M(CSRImmediateInstruction)              \
    M(Fence)                                \
    M(InstructionFence)

ENUMERATE_INSTRUCTION_IMPLS(MAKE_INSTRUCTION_EQUALS)

}
