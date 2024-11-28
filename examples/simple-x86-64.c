/**
 * This file is part of Rellume.
 *
 * (c) 2022, Alexis Engelke <alexis.engelke@googlemail.com>
 *
 * Rellume is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License (LGPL)
 * as published by the Free Software Foundation, either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * Rellume is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Rellume.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdbool.h>
#include <llvm-c/Core.h>

#include <rellume/rellume.h>


int main(void) {
    // Create LLVM module
    LLVMModuleRef mod = LLVMModuleCreateWithName("lifter");

    // static const unsigned char code[] = {
    //     0x48, 0x89, 0xf8, // mov rax,rdi
    //     0x48, 0x39, 0xf7, // cmp rdi,rsi
    //     0x7d, 0x03,       // jge $+3
    //     0x48, 0x89, 0xf0, // mov rax,rsi
    //     0xc3,             // ret
    // };

    static const unsigned char code[] = {
        0x55,                                       // push rbp
        0x48, 0x89, 0xe5,                           // mov rbp,rsp
        0xc7, 0x45, 0xf4, 0x02, 0x00, 0x00, 0x00,   // mov DWORD PTR [rbp-0xc],0x2
        0xc7, 0x45, 0xf8, 0x03, 0x00, 0x00, 0x00,   // mov DWORD PTR [rbp-0x8],0x3
        0x8b, 0x55, 0xf4,                           // mov edx,DWORD PTR [rbp-0xc]
        0x8b, 0x45, 0xf8,                           // mov eax,DWORD PTR [rbp-0x8]
        0x01, 0xd0,                                 // add eax,edx
        0x89, 0x45, 0xfc,                           // mov DWORD PTR [rbp-0x4],eax
        0x8b, 0x45, 0xfc,                           // mov eax,DWORD PTR [rbp-0x4]
        0x5d,                                       // pop rbp
        0xc3,                                       // ret
    };
        

    // Create function for lifting
    LLConfig* cfg = ll_config_new();
    ll_config_set_architecture(cfg, "x86-64");
    ll_config_set_call_ret_clobber_flags(cfg, true);
    LLFunc* fn = ll_func_new(mod, cfg);
    // Lift the whole function by following all direct jumps
    ll_func_decode_cfg(fn, (uintptr_t) code, NULL, NULL);
    LLVMValueRef llvm_fn = ll_func_lift(fn);
    LLVMSetValueName(llvm_fn, "main");
    LLVMDumpValue(llvm_fn);

    return 0;
}
