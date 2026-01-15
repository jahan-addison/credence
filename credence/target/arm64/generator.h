/*****************************************************************************
 * Copyright (c) Jahan Addison
 *
 * This software is dual-licensed under the Apache License, Version 2.0 or
 * the GNU General Public License, Version 3.0 or later.
 *
 * You may use this work, in part or in whole, under the terms of either
 * license.
 *
 * See the LICENSE.Apache-v2 and LICENSE.GPL-v3 files in the project root
 * for the full text of these licenses.
 ****************************************************************************/

#pragma once

#include "assembly.h"                     // for Mnemonic, arm_mn, Directives
#include "memory.h"                       // for Memory_Access, Mnemonic
#include "stack.h"                        // for Stack
#include <credence/ir/ita.h>              // for Instructions
#include <credence/ir/object.h>           // for Label, Object, RValue
#include <credence/target/common/flags.h> // for flags
#include <credence/util.h>                // for AST_Node, CREDENCE_PRIVATE...
#include <cstddef>                        // for size_t
#include <deque>                          // for deque
#include <ostream>                        // for ostream
#include <string>                         // for basic_string, string
#include <utility>                        // for move
#include <variant>                        // for variant

/****************************************************************************
 *
 * ARM64 Assembly Code Generator and Emitter Types
 *
 * Generates ARM64/AArch64 assembly for Linux and Darwin (macOS). Compliant
 * with ARM64 Procedure Call Standard (PCS). Translates ITA intermediate
 * representation into machine code.
 *
 * Example - simple function:
 *
 *   B code:
 *     add(x, y) {
 *       return(x + y);
 *     }
 *
 *   Generated ARM64:
 *     add:
 *         stp x29, x30, [sp, #-16]!
 *         mov x29, sp
 *         add x0, x0, x1      ; x in x0, y in x1
 *         ldp x29, x30, [sp], #16
 *         ret
 *
 * Example - globals and strings:
 *
 *   B code:
 *     greeting "Hello, World!\n";
 *     counter 0;
 *
 *   Generated:
 *     .data
 *     ._L_str1__:
 *         .asciz "Hello, World!\n"
 *     greeting:
 *         .quad ._L_str1__
 *     counter:
 *         .quad 0
 *
 *****************************************************************************/

namespace credence::target::arm64 {

namespace flag = common::flag;

namespace {
using Instructions = assembly::Instructions;
using Instruction = assembly::Instruction;
using Operand_Size = assembly::Operand_Size;
using Instruction_Pair = assembly::Instruction_Pair;
using Mnemonic = assembly::Mnemonic;
using Storage = assembly::Storage;
using Immediate = assembly::Immediate;
using Operand_Stack = std::deque<Storage>;
}

void emit(std::ostream& os, util::AST_Node& symbols, util::AST_Node const& ast);

constexpr std::string emit_immediate_storage(Immediate const& immediate);

constexpr std::string emit_stack_storage(assembly::Stack::Offset offset,
    common::flag::flags flags);

constexpr std::string emit_register_storage(assembly::Register device,
    common::flag::flags flags);

void emit_arm64_assembly_prologue(std::ostream& os);

class Assembly_Emitter;

/**
 * @brief Storage Emitter for destination and source storage devices
 */
class Storage_Emitter
{
  public:
    explicit Storage_Emitter(memory::Memory_Access& accessor,
        std::size_t index,
        std::size_t* pointers_index)
        : accessor_(accessor)
        , instruction_index_(index)
        , address_pointer_index(pointers_index)
    {
    }

    enum class Source
    {
        s_0,
        s_1,
        s_2,
        s_3,
    };

    constexpr bool is_alignment_mnemonic(Mnemonic mnemonic)
    {
        return mnemonic == arm_mn(sub) or mnemonic == arm_mn(add) or
               mnemonic == arm_mn(stp) or mnemonic == arm_mn(ldp) or
               mnemonic == arm_mn(ldr) or mnemonic == arm_mn(str);
    }

    std::string get_storage_device_as_string(Storage const& storage);

    void emit(std::ostream& os,
        Storage const& storage,
        Mnemonic mnemonic,
        Source source);

  private:
    void apply_stack_alignment(Storage& operand,
        Mnemonic mnemonic,
        Source source,
        common::flag::flags flags);
    void emit_mnemonic_operand(std::ostream& os,
        Storage const& operand,
        Mnemonic mnemonic,
        Source source,
        common::flag::flags flags);

  private:
    memory::Memory_Access accessor_;
    std::size_t instruction_index_;

  private:
    std::size_t* address_pointer_index{ 0 };
};

/**
 * @brief Text Emitter for the text section in a arm64 application
 */
class Text_Emitter
{
  public:
    explicit Text_Emitter(memory::Memory_Access accessor)
        : accessor_(accessor)
    {
        instructions_ = accessor->instruction_accessor;
    }
    friend class Assembly_Emitter;

    void emit_stdlib_externs(std::ostream& os);
    void emit_text_directives(std::ostream& os);
    void emit_text_section(std::ostream& os);

    bool emit_callee_saved_registers_stp(Mnemonic mnemonic,
        std::ostream& os,
        std::size_t index);

  private:
    void emit_assembly_instruction(std::ostream& os,
        std::size_t index,
        Instruction const& s);
    void emit_assembly_label(std::ostream& os,
        Label const& s,
        bool set_label = true);
    void emit_text_instruction(std::ostream& os,
        std::variant<Label, Instruction> const& instruction,
        std::size_t index,
        bool set_label = true);
    void emit_function_epilogue(std::ostream& os);
    void emit_epilogue_jump(std::ostream& os);

    // clang-format off
  CREDENCE_PRIVATE_UNLESS_TESTED:
    bool test_no_stdlib{ false };
    // clang-format on

  private:
    memory::Memory_Access accessor_;

  private:
    std::size_t address_pointer_index{ 0 };

  private:
    memory::Instruction_Pointer instructions_;
    Instructions return_instructions_;
    std::size_t label_size_{ 0 };
    Label frame_{};
    Label branch_{};
};

/**
 * @brief Data Emitter for the data section in a arm64 application
 */

class Data_Emitter
{
  public:
    explicit Data_Emitter(memory::Memory_Access accessor)
        : accessor_(accessor)
    {
    }
    friend class Assembly_Emitter;

  public:
    void emit_data_section(std::ostream& os);

  private:
    void set_data_globals();
    void set_data_strings();
    void set_data_floats();
    void set_data_doubles();

    inline void set_data_section()
    {
        set_data_globals();
        set_data_strings();
        set_data_floats();
        set_data_doubles();
    }

  private:
    assembly::Directives get_instructions_from_directive_type(
        assembly::Directive directive,
        RValue const& rvalue);

  private:
    memory::Memory_Access accessor_;

  private:
    assembly::Directives instructions_;
};

/**
 * @brief Assembly Emitter that emits the data and text section of an arm64
 * application
 */
class Assembly_Emitter
{
  public:
    explicit Assembly_Emitter(memory::Memory_Access accessor)
        : accessor_(std::move(accessor))
    {
        ir_instructions_ =
            *accessor_->table_accessor.table_->get_ir_instructions();
    }

  public:
    void emit(std::ostream& os);

  private:
    memory::Memory_Access accessor_;

  private:
    ir::Instructions ir_instructions_;

    // clang-format off
  CREDENCE_PRIVATE_UNLESS_TESTED:
    Data_Emitter data_{ accessor_ };
    Text_Emitter text_{ accessor_ };
    // clang-format on
};

#ifdef CREDENCE_TEST
void emit(std::ostream& os,
    util::AST_Node& symbols,
    util::AST_Node const& ast,
    bool no_stdlib = true);
#endif
} // namespace credence::target::arm64
