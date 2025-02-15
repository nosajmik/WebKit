//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SeparateStructFromFunctionDeclarations: Separate struct declarations from
// function declaration return type. If necessary gives nameless structs
// internal names.
//
// For example:
//   struct Foo { int a; } foo();
// becomes:
//   struct Foo { int a; };
//   Foo foo();
//

#ifndef COMPILER_TRANSLATOR_TREEOPS_SEPARATESTRUCTFROMFUNCTIONDECLARATIONS_H_
#define COMPILER_TRANSLATOR_TREEOPS_SEPARATESTRUCTFROMFUNCTIONDECLARATIONS_H_

#include "common/angleutils.h"

namespace sh
{
class TCompiler;
class TIntermBlock;
class TSymbolTable;

[[nodiscard]] bool SeparateStructFromFunctionDeclarations(TCompiler *compiler,
                                                          TIntermBlock *root,
                                                          TSymbolTable *symbolTable);
}  // namespace sh

#endif  // COMPILER_TRANSLATOR_TREEOPS_SEPARATESTRUCTFROMFUNCTIONDECLARATIONS_H_
