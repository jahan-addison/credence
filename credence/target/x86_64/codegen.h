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

#include "instructions.h"
#include <credence/ir/ita.h>        // for ITA
#include <credence/ir/table.h>      // for Table
#include <credence/target/target.h> // for Backend
#include <credence/util.h>          // for CREDENCE_PRIVATE_UNLESS_TESTED
#include <ostream>                  // for ostream
#include <stack>                    // for stack
#include <string>                   // for basic_string, string
#include <variant>                  // for variant
#include <vector>                   // for vector

// https://cs.brown.edu/courses/cs033/docs/guides/x64_cheatsheet.pdf

namespace credence::target::x86_64 {

class Code_Generator final : public target::Backend_Target_Platform
{
  public:
    Code_Generator() = delete;
    explicit Code_Generator(ir::Table::Table_PTR table)
        : Backend_Target_Platform(std::move(table))
    {
    }

    using Operation_Pair = detail::Operation_Pair;

  private:
    void setup_table();
    void setup_constants();

  public:
    // clang-format off
  CREDENCE_PRIVATE_UNLESS_TESTED:
    Operation_Pair from_ita_expression(ir::Table::RValue const& expr);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    Operation_Pair from_ita_unary_expression(ir::ITA::Quadruple const& inst);
    Operation_Pair from_ita_bitwise_expression(ir::ITA::Quadruple const& inst);
    Operation_Pair from_ita_binary_expression(ir::ITA::Quadruple const& inst);

  CREDENCE_PRIVATE_UNLESS_TESTED:
    // void from_func_start_ita() override;
    // void from_func_end_ita() override;
    // void from_label_ita() override;
    // void from_goto_ita() override;
    // void from_locl_ita() override;
    // void from_globl_ita() override;
    // void from_if_ita() override;
    // void from_jmp_e_ita() override;
    // void from_push_ita() override;
    // void from_pop_ita() override;
    // void from_call_ita() override;
    // void from_cmp_ita() override;
    // void from_mov_ita() override;
    // void from_return_ita() override;
    // void from_leave_ita() override;
    void from_noop_ita() override {
    }
    // clang-format on

  private:
    unsigned int constant_index{ 0 };

  private:
    std::string current_frame{ "main" };
    detail::Instructions instructions_;
    detail::Instructions data_;
};

} // namespace x86_64
