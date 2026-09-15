# kokkos/cmake/CompilerWarnings.cmake
# One warning policy for every target in the tree. Lab code is teaching material:
# it is expected to compile clean, so the flags are deliberately strict and are
# applied PRIVATE (a consumer of ww_kokkos inherits the headers, not the flags).
#
# SPDX-License-Identifier: MIT

# Apply the lab's warning set to <tgt>, per language and per compiler.
function(ww_apply_warnings tgt)
  target_compile_options(${tgt} PRIVATE
    # -Wconversion is the important one here: the kernel state is float32 and a
    # silent double->float narrowing is exactly the bug class we must not ship.
    $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>:-Wall;-Wextra;-Wpedantic;-Wshadow;-Wconversion;-Wno-unused-parameter>
    # nvcc_wrapper forwards -Wall to the host compiler; the rest confuse nvcc.
    $<$<COMPILE_LANG_AND_ID:CXX,NVIDIA>:-Wall>
    $<$<COMPILE_LANGUAGE:Fortran>:-Wall;-Wextra;-fimplicit-none>)
endfunction()
