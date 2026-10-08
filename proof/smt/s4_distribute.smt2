; W3SNL1 section 4, leading term of S: -2.*(SA1(ISP)+SA2(ISP)), as written in w3snl1md.F90
; and in kokkos/src/ww_kokkos/snl1_dia.cpp:195, against rewrites an agent might make.
; `=` is SMT-LIB equality on IEEE values: +0 and -0 differ, all NaNs are one value.
;
; Expected:
;   (a) unsat   commuting the product is exact
;   (b) sat     distributing is not: -2*s1 overflows where -2*(s1+s2) does not
;   (c) sat     even with no overflow possible: s1 = -s2 gives -0 one way, +0 the other
;   (d) unsat   with no overflow and a non-zero result, distributing is exact
; Measured 2026-10-01 on 4 cores, all four queries: bitwuzla 0.9.1 23 s, cvc5 1.4.1 79 s.
; z3 5.1.0 answered (a)-(c) and had not proved (d) after 900 s.
;
; run: z3 s4_distribute.smt2   (`pip install z3-solver` ships the z3 CLI; the bitwuzla and
;      cvc5 timings came from their Python APIs, `pip install bitwuzla cvc5`, which ship none)
; SPDX-License-Identifier: MIT
(set-option :produce-models true)
(set-logic QF_FP)
(declare-const s1 Float32)
(declare-const s2 Float32)
(define-fun m2 () Float32 (fp.neg ((_ to_fp 8 24) RNE 2.0)))
(define-fun big () Float32 ((_ to_fp 8 24) RNE 42535295865117307932921825928971026432.0)) ; 2^125
(define-fun ref () Float32 (fp.mul RNE m2 (fp.add RNE s1 s2)))
(define-fun dist () Float32 (fp.add RNE (fp.mul RNE m2 s1) (fp.mul RNE m2 s2)))
(define-fun small () Bool (and (fp.lt (fp.abs s1) big) (fp.lt (fp.abs s2) big)))
; (a)
(push 1)
(assert (not (= ref (fp.mul RNE (fp.add RNE s1 s2) m2))))
(check-sat)
(pop 1)
; (b)
(push 1)
(assert (not (= ref dist)))
(check-sat)
(get-value (s1 s2 ref dist))
(pop 1)
; (c)
(push 1)
(assert small)
(assert (not (= ref dist)))
(check-sat)
(get-value (s1 s2 ref dist))
(pop 1)
; (d)
(push 1)
(assert small)
(assert (not (fp.isZero ref)))
(assert (not (= ref dist)))
(check-sat)
(pop 1)
