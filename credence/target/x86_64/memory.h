/*
 * Copyright (c) Jahan Addison
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "assembly.h"          // for Register, Operand_Size, get_operand_s...
#include "credence/symbol.h"   // for Symbol_Table
#include "stack.h"             // for Stack
#include <credence/ir/ita.h>   // for Instruction
#include <credence/ir/table.h> // for Table, Stack, Vector
#include <credence/map.h>      // for Ordered_Map
#include <credence/types.h>    // for from_lvalue_offset, is_temporary, Size
#include <credence/util.h>     // for contains, overload
#include <cstddef>             // for size_t
#include <deque>               // for deque
#include <functional>          // for function
#include <map>                 // for map
#include <matchit.h>           // for pattern, PatternHelper, PatternPipable
#include <memory>              // for shared_ptr
#include <string>              // for basic_string, string, char_traits
#include <string_view>         // for basic_string_view, string_view
#include <tuple>               // for get, tuple
#include <utility>             // for move, pair
#include <variant>             // for monostate, visit

namespace credence::target::x86_64::memory {
class Memory_Accessor;
} // lines 96-96
namespace credence::target::x86_64::memory::detail {
struct Table_Accessor;
struct Instruction_Accessor;
} // lines 163-163

namespace credence::target::x86_64 {

namespace flag {

using flags = unsigned int;

enum Instruction_Flag : flags
{
    None = 0,
    Address = 1 << 0,
    Indirect = 1 << 1,
    Indirect_Source = 1 << 2,
    Align = 1 << 3,
    Argument = 1 << 4,
    QWord_Dest = 1 << 5,
    Load = 1 << 6

};

} // namespace flag

namespace {

using Register = assembly::Register;
using Directive = assembly::Directive;
using Mnemonic = assembly::Mnemonic;
using Label = assembly::Stack::Label;
using LValue = assembly::Stack::LValue;
using RValue = assembly::Stack::RValue;
using Storage = assembly::Storage;
using Storage_Operands = std::pair<assembly::Storage, assembly::Storage>;
using Operand_Size = assembly::Operand_Size;
using Operator_Symbol = std::string;
using Immediate = assembly::Immediate;
using Directives = assembly::Directives;

}

/**
 * Short-form helpers for matchit predicate pattern matching
 */
constexpr bool is_immediate(RValue const& rvalue)
{
    return type::is_rvalue_data_type(rvalue);
};
inline bool is_temporary(RValue const& rvalue)
{
    return type::is_temporary(rvalue);
};
constexpr bool is_parameter(RValue const& rvalue)
{
    return rvalue.starts_with("_p");
};
inline bool is_vector_offset(RValue const& rvalue)
{
    return util::contains(rvalue, "[") and util::contains(rvalue, "]");
};

namespace memory {

enum class Operand_Type
{
    Destination,
    Source
};

namespace registers {

using general_purpose = std::deque<Register>;

const general_purpose available_qword_register = { Register::rdi,
    Register::r8,
    Register::r9,
    Register::rsi,
    Register::rdx,
    Register::rcx };
const general_purpose available_dword_register = { Register::edi,
    Register::r8d,
    Register::r9d,
    Register::esi,
    Register::edx,
    Register::ecx };

} // namespace registers

using Memory_Access = std::shared_ptr<Memory_Accessor>;
using Instruction_Pointer = std::shared_ptr<detail::Instruction_Accessor>;
using Stack_Pointer = std::shared_ptr<assembly::Stack>;
using Table_Pointer = std::shared_ptr<ir::object::Object>;

/**
 * @brief Get the intel-format prefix for storage device sizes
 */
constexpr std::string storage_prefix_from_operand_size(Operand_Size size)
{
    namespace m = matchit;
    return m::match(size)(
        m::pattern | Operand_Size::Qword = [&] { return "qword ptr"; },
        m::pattern | Operand_Size::Dword = [&] { return "dword ptr"; },
        m::pattern | Operand_Size::Word = [&] { return "word ptr"; },
        m::pattern | Operand_Size::Byte = [&] { return "byte ptr"; },
        m::pattern | m::_ = [&] { return "dword ptr"; });
}

/**
 * @brief Stack frame object that keeps a stack of function calls and arguments
 */
struct Stack_Frame
{
    explicit Stack_Frame(std::shared_ptr<Memory_Accessor> accessor)
        : accessor_(accessor)
    {
    }

    using IR_Function = ir::object::Function_PTR;
    using IR_Stack = ir::Stack;

    void set_stack_frame(Label const& name);

    IR_Function get_stack_frame(Label const& name) const;
    IR_Function get_stack_frame() const;

    IR_Stack argument_stack{};
    IR_Stack call_stack{ "main" };
    Label symbol{ "main" };
    Label tail{};
    std::size_t size{};

  private:
    std::shared_ptr<Memory_Accessor> accessor_;
};

namespace detail {

/**
 *  Forward declarations for the Memory_Accessor dependent accessors
 */

struct Accumulator_Accessor;
struct Instruction_Accessor;
struct Table_Accessor;
struct Flag_Accessor;
struct Vector_Accessor;
class Address_Accessor;
struct Register_Accessor;

using Operand_Lambda = std::function<bool(RValue)>;

/**
 * @brief Flag accessor for bit flags set on instruction indices for emission
 */
struct Flag_Accessor
{
    void set_instruction_flag(flag::Instruction_Flag set_flag,
        unsigned int index);
    void unset_instruction_flag(flag::Instruction_Flag set_flag,
        unsigned int index);
    void set_load_address_from_previous_instruction(
        assembly::Instructions const& instructions);
    void set_instruction_flag(flag::flags flags, unsigned int index);
    constexpr bool index_contains_flag(unsigned int index,
        flag::Instruction_Flag flag)
    {
        if (!instruction_flag.contains(index))
            return false;
        return instruction_flag.at(index) & flag;
    }
    flag::flags get_instruction_flags_at_index(unsigned int index)
    {
        if (!instruction_flag.contains(index))
            return 0;
        return instruction_flag.at(index);
    }

  private:
    Ordered_Map<unsigned int, flag::flags> instruction_flag{};
};

/**
 * @brief Accumulator accessor that grabs rax, eax, or ax by storage size
 *
 *   Note that if signal_register is set, it returns that register instead.
 *   If all registers are unavailable, it grabs an address on the stack
 */
struct Accumulator_Accessor
{
    explicit Accumulator_Accessor(Register* signal_register)
        : signal_register_(signal_register)
    {
    }
    /**
     * @brief Get the accumulator register from storage
     */
    constexpr Register get_accumulator_register_from_storage(
        Storage const& storage,
        Stack_Pointer& stack)
    {
        Register accumulator{};
        std::visit(
            util::overload{
                [&](std::monostate) { accumulator = Register::eax; },
                [&](assembly::Stack::Offset const& offset) {
                    auto size = stack->get_operand_size_from_offset(offset);
                    accumulator = get_accumulator_register_from_size(size);
                },
                [&](Register const& device) { accumulator = device; },
                [&](Immediate const& immediate) {
                    auto size = assembly::get_operand_size_from_rvalue_datatype(
                        immediate);
                    accumulator = get_accumulator_register_from_size(size);
                } },
            storage);
        return accumulator;
    }
    /**
     * @brief Get the accumulator register from Operand_Size
     */
    constexpr Register get_accumulator_register_from_size(
        Operand_Size size = Operand_Size::Dword)
    {
        namespace m = matchit;
        if (*signal_register_ != Register::eax) {
            auto designated = *signal_register_;
            *signal_register_ = Register::eax;
            return designated;
        }
        return m::match(size)(
            m::pattern | Operand_Size::Qword = [&] { return Register::rax; },
            m::pattern | Operand_Size::Word = [&] { return Register::ax; },
            m::pattern | Operand_Size::Byte = [&] { return Register::al; },
            m::pattern | m::_ = [&] { return Register::eax; });
    }

  private:
    Register* signal_register_;
};

/**
 * @brief Vector (array) accessor for vectors in memory
 */
struct Vector_Accessor
{
    explicit Vector_Accessor(Table_Pointer& table)
        : table_(table)
    {
    }
    using Vector_Entry_Pair =
        std::pair<ir::object::Vector::Address, assembly::Operand_Size>;
    Vector_Entry_Pair get_rip_offset_address(LValue const& lvalue,
        RValue const& offset);

  private:
    Table_Pointer& table_;
};

/**
 * @brief Instruction accessor and iterator for instructions in memory
 */
struct Instruction_Accessor
{
    using Instructions = assembly::Instructions;

    Instructions& get_instructions() { return instructions_; }

    auto begin() { return instructions_.begin(); }
    auto end() { return instructions_.end(); }
    auto begin() const { return instructions_.begin(); }
    auto end() const { return instructions_.end(); }

    std::size_t size() { return instructions_.size(); }

  private:
    Instructions instructions_{};
};

/**
 * @brief Buffer accessor that stores addresses of strings and global vectors
 */
class Buffer_Accessor
{
  public:
    explicit Buffer_Accessor(Table_Pointer& table)
        : table_(table)
    {
    }
    std::size_t get_lvalue_string_size(LValue const& lvalue,
        Stack_Frame const& stack_frame);
    std::size_t get_string_size_from_storage(Storage const& storage);
    void set_buffer_size_from_syscall(std::string_view routine,
        Stack_Frame::IR_Stack& argument_stack);
    constexpr bool has_bytes() { return read_bytes_cache_ == 0UL; }
    constexpr std::size_t read_bytes()
    {
        auto read_bytes = read_bytes_cache_;
        read_bytes_cache_ = 0;
        return read_bytes;
    }
    Operand_Lambda is_global_vector = [&](RValue const& rvalue) {
        auto rvalue_reference = type::from_lvalue_offset(rvalue);
        return table_->vectors.contains(rvalue_reference) and
               table_->globals.is_pointer(rvalue_reference);
    };

    friend class Address_Accessor;

  public:
    void insert(RValue const& string_key, Label const& asciz_address)
    {
        string_cache_.insert_or_assign(string_key, asciz_address);
    }
    RValue get_string_address_offset(RValue const& string)
    {
        return string_cache_.at(string);
    }
    inline bool is_allocated_string(RValue const& rvalue)
    {
        return string_cache_.contains(rvalue);
    }

  private:
    Table_Pointer& table_;

  public:
    std::size_t constant_size_index{ 0 };

  private:
    std::map<std::string, RValue> string_cache_{};
    std::size_t read_bytes_cache_{ 0 };
};

/**
 * @brief Address accessor that resolves an lvalue or buffer to an address in
 * memory
 */
class Address_Accessor
{
  public:
    explicit Address_Accessor(Table_Pointer& table,
        Stack_Pointer& stack,
        Flag_Accessor& flag_accessor)
        : table_(table)
        , stack_(stack)
        , flag_accessor_(flag_accessor)
        , buffer_accessor(table)
    {
    }

    Storage get_lvalue_address_from_stack(LValue const& lvalue);
    assembly::Instruction_Pair get_lvalue_address_and_instructions(
        LValue const& lvalue,
        std::size_t instruction_index,
        bool use_prefix = true);
    bool is_lvalue_storage_type(LValue const& lvalue,
        std::string_view type_check);

  private:
    Operand_Lambda is_address = [&](RValue const& rvalue) {
        return stack_->is_allocated(rvalue);
    };
    Operand_Lambda is_vector = [&](RValue const& rvalue) {
        return table_->vectors.contains(type::from_lvalue_offset(rvalue));
    };
    Operand_Lambda is_global_vector = [&](RValue const& rvalue) {
        auto rvalue_reference = type::from_lvalue_offset(rvalue);
        return table_->vectors.contains(rvalue_reference) and
               table_->globals.is_pointer(rvalue_reference);
    };

  public:
    bool address_ir_assignment{ false };

    std::deque<Immediate> immediate_stack{};

  private:
    Table_Pointer& table_;
    Stack_Pointer& stack_;

  private:
    Flag_Accessor& flag_accessor_;

  public:
    Buffer_Accessor buffer_accessor;
};

/**
 * @brief Table accessor for memory access to the table, type checker, and
 * current index in IR visitor iteration
 */
struct Table_Accessor
{
    Table_Accessor() = delete;
    explicit Table_Accessor(Table_Pointer& table)
        : table_(table)
    {
    }
    constexpr void set_ir_iterator_index(unsigned int index_)
    {
        index = index_;
    }
    /**
     * @brief Check if the current ir instruction
     * is a temporary lvalue assignment
     */
    bool is_ir_instruction_temporary()
    {
        return type::is_temporary(
            std::get<1>(table_->ir_instructions->at(index)));
    }
    /**
     * @brief Get the lvalue of the current ir instruction
     */
    std::string get_ir_instruction_lvalue()
    {
        return std::get<1>(table_->ir_instructions->at(index));
    }
    /**
     * @brief Check if last ir instruction was Instruction::MOV
     *
     * Primarily used to determine if we need a second register for an
     * expression
     */
    bool last_ir_instruction_is_assignment()
    {
        if (index < 1)
            return false;
        auto last = table_->ir_instructions->at(index - 1);
        return std::get<0>(last) == ir::Instruction::MOV and
               not type::is_temporary(std::get<1>(last));
    }
    /**
     * @brief Check if next ir instruction is a temporary assignment
     *
     * Primarily used to determine if we need a second register for an
     * expression
     */
    bool next_ir_instruction_is_temporary()
    {
        if (table_->ir_instructions->size() < index + 1)
            return false;
        auto next = table_->ir_instructions->at(index + 1);
        return std::get<0>(next) == ir::Instruction::MOV and
               type::is_temporary(std::get<1>(next));
    }
    Table_Pointer& table_;
    unsigned int index{ 0 };
};

/**
 * @brief Register accessor that manages available and signal registers
 */
struct Register_Accessor
{

    explicit Register_Accessor(Register* signal_register)
        : signal_register(signal_register)
    {
    }
    registers::general_purpose available_qword_register =
        registers::available_qword_register;
    registers::general_purpose available_dword_register =
        registers::available_dword_register;
    Storage get_register_for_binary_operator(RValue const& rvalue,
        Stack_Pointer& stack);
    Storage get_available_register(Operand_Size size, Stack_Pointer& stack);
    void reset_available_registers()
    {
        available_qword_register = registers::available_qword_register;
        available_dword_register = registers::available_dword_register;
    }
    /**
     * @brief Get a second accumulator register from an Operand Size
     */
    constexpr Register get_second_register_from_size(Operand_Size size)
    {
        namespace m = matchit;
        return m::match(size)(
            m::pattern | Operand_Size::Qword = [&] { return Register::rdi; },
            m::pattern | Operand_Size::Word = [&] { return Register::di; },
            m::pattern | Operand_Size::Byte = [&] { return Register::dil; },
            m::pattern | m::_ = [&] { return Register::edi; });
    }

    Register* signal_register;
};

} // namespace detail

/**
 * @brief The Memory registry and mediator that orchestrates access to memory
 */
class Memory_Accessor
{
  public:
    Memory_Accessor() = delete;

    explicit Memory_Accessor(Table_Pointer table, Stack_Pointer stack_pointer)
        : table_(std::move(table))
        , stack(std::move(stack_pointer))
        , table_accessor(table_)
        , accumulator_accessor{ &signal_register }
        , vector_accessor{ table_ }
        , register_accessor{ &signal_register }
        , address_accessor{ table_, stack, flag_accessor }
    {
        instruction_accessor = std::make_shared<detail::Instruction_Accessor>();
    }
    constexpr void set_signal_register(Register signal_)
    {
        signal_register = signal_;
    }

  private:
    Register signal_register = assembly::Register::eax;

  private:
    Table_Pointer table_;

  public:
    Stack_Pointer stack;

  public:
    detail::Flag_Accessor flag_accessor{};
    detail::Table_Accessor table_accessor{ table_ };
    detail::Accumulator_Accessor accumulator_accessor{ &signal_register };
    detail::Vector_Accessor vector_accessor{ table_ };
    detail::Register_Accessor register_accessor{ &signal_register };
    detail::Address_Accessor address_accessor{ table_, stack, flag_accessor };
    Instruction_Pointer instruction_accessor{};
};

} // namespace memory

} // namespace x86_64