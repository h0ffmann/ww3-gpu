; W3SNL1 section 3.a, EP1 for one bin (w3snl1md.F90 W3SNL1 section 3.a;
; kokkos/src/ww_kokkos/snl1_dia.cpp:154-157): the Fortran order with a rounding after every
; product and every sum, against the three-FMA chain g++ -O3 -march=x86-64-v3 emits when
; -ffp-contract=off is missing. Inputs restricted to the physical range: interpolation
; weights in [0, 1], energies positive and normal.
;
; Expected: sat, with a counterexample one ULP apart. This is the formal form of the
; 1.1e-5 drift kokkos/PORT_STATUS.md records for the openmp-release preset.
; Measured 2026-10-01 on 4 cores: bitwuzla 0.9.1 0.4-1.6 s, cvc5 1.4.1 0.5 s, z3 5.1.0 31 s.
;
; run: z3 ep1_contraction.smt2   (`pip install z3-solver` ships the z3 CLI; the bitwuzla and
;      cvc5 timings came from their Python APIs, `pip install bitwuzla cvc5`, which ship none)
; SPDX-License-Identifier: MIT
(set-option :produce-models true)
(set-logic QF_FP)
(declare-const a1 Float32)
(declare-const a2 Float32)
(declare-const a3 Float32)
(declare-const a4 Float32)
(declare-const u1 Float32)
(declare-const u2 Float32)
(declare-const u3 Float32)
(declare-const u4 Float32)
(define-fun zero () Float32 ((_ to_fp 8 24) RNE 0.0))
(define-fun one () Float32 ((_ to_fp 8 24) RNE 1.0))
(define-fun w ((x Float32)) Bool (and (fp.leq zero x) (fp.leq x one)))
(define-fun e ((x Float32)) Bool (and (fp.isNormal x) (fp.isPositive x)))
; AWG1*UE(IP11) + AWG2*UE(IP12) + AWG3*UE(IP13) + AWG4*UE(IP14), left to right
(define-fun ref () Float32
  (fp.add RNE (fp.add RNE (fp.add RNE (fp.mul RNE a1 u1) (fp.mul RNE a2 u2))
                          (fp.mul RNE a3 u3))
              (fp.mul RNE a4 u4)))
(define-fun fused () Float32
  (fp.fma RNE a4 u4 (fp.fma RNE a3 u3 (fp.fma RNE a2 u2 (fp.mul RNE a1 u1)))))
(assert (and (w a1) (w a2) (w a3) (w a4) (e u1) (e u2) (e u3) (e u4)))
(assert (not (= ref fused)))
(check-sat)
(get-value (a1 a2 a3 a4 u1 u2 u3 u4 ref fused))
