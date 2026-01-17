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

#include "assembly.h"                     // for Operand_Size, Storage, Dir...
#include "memory.h"                       // for Memory_Access, Operand_Size
#include "stack.h"                        // for Stack
#include <credence/ir/ita.h>              // for Instructions
#include <credence/ir/object.h>           // for Label, Object, RValue
#include <credence/target/common/flags.h> // for flags
#include <credence/util.h>                // for AST_Node, CREDENCE_PRIVATE...
#include <cstddef>                        // for size_t
#include <ostream>                        // for ostream
#include <string>                         // for basic_string, string
#include <utility>                        // for move
#include <variant>                        // for variant

/****************************************************************************
 *
 * x86-64 Assembly Code Generator and Emitter Types
 *
 * Generates Intel-syntax x86-64 assembly for Linux and Darwin (macOS).
 * Compliant with System V ABI calling conventions. Translates ITA
 * intermediate representation into optimized x86-64 machine code.
 *
 * Example - simple function:
 *
 *   B code:
 *     add(x, y) {
 *       return(x + y);
 *     }
 *
 *   x86-64 (Intel syntax):
 *     add:
 *         push rbp
 *         mov rbp, rsp
 *         mov eax, edi        ; x in edi (1st arg)
 *         add eax, esi        ; y in esi (2nd arg)
 *         pop rbp
 *         ret
 *
 * Example - globals and strings:
 *
 *   B code:
 *     greeting "Hello, World!\n";
 *     counter 0;
 *
 *   x86-64 (Intel syntax):
 *     .data
 *     ._L_str1__:
 *         .asciz "Hello, World!\n"
 *     greeting:
 *         .quad ._L_str1__
 *     counter:
 *         .quad 0
 *
 *****************************************************************************/

namespace credence::target::x86_64 {

using Instruction_Pair = assembly::Instruction_Pair;

void emit(std::ostream& os, util::AST_Node& symbols, util::AST_Node const& ast);

constexpr std::string emit_immediate_storage(
    assembly::Immediate const& immediate);

constexpr std::string emit_stack_storage(assembly::Stack::Offset offset,
    assembly::Operand_Size size,
    common::flag::flags flags);

constexpr std::string emit_register_storage(assembly::Register device,
    assembly::Operand_Size size,
    common::flag::flags flags);

void emit_x86_64_assembly_intel_prologue(std::ostream& os);

class Assembly_Emitter;

/**
 * @brief Storage Emitter for destination and source storage devices
 */
class Storage_Emitter
{
  public:
    explicit Storage_Emitter(memory::Memory_Access& accessor,
        std::size_t index,
        Storage& source_storage)
        : accessor_(accessor)
        , instruction_index_(index)
        , source_storage_(source_storage)
    {
    }

    constexpr void set_address_size(Operand_Size address)
    {
        address_size = address;
    }

    constexpr void reset_address_size() { address_size = Operand_Size::Empty; }

    std::string get_storage_device_as_string(assembly::Storage const& storage,
        Operand_Size size);

    void emit(std::ostream& os,
        assembly::Storage const& storage,
        assembly::Mnemonic mnemonic,
        memory::Operand_Type type_);

  private:
    memory::Memory_Access accessor_;
    std::size_t instruction_index_;

  private:
    Operand_Size address_size = Operand_Size::Empty;
    Storage& source_storage_;
};

/**
 * @brief Text Emitter for the text section in a x86_64 application
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

  private:
    void emit_assembly_instruction(std::ostream& os,
        std::size_t index,
        assembly::Instruction const& s);
    void emit_assembly_label(std::ostream& os,
        Label const& s,
        bool set_label = true);
    void emit_text_instruction(std::ostream& os,
        std::variant<Label, assembly::Instruction> const& instruction,
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
    memory::Instruction_Pointer instructions_;
    assembly::Instructions return_instructions_;
    std::size_t label_size_{ 0 };
    Label frame_{};
    Label branch_{};
};

/**
 * @brief Data Emitter for the data section in a x86_64 application
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
 * @brief Assembly Emitter that emits the data and text section of an x64
 * application
 */
class Assembly_Emitter
{
  public:
    explicit Assembly_Emitter(memory::Memory_Access accessor)
        : accessor_(std::move(accessor))
    {
        ir_instructions_ =
            *accessor_->table_accessor.get_table()->get_ir_instructions();
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
} // namespace x86_64
