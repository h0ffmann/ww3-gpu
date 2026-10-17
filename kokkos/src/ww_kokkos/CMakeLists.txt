# kokkos/src/ww_kokkos/CMakeLists.txt
# Sources of the portable kernel library: the DIA port (snl1_*), the ST4 port
# (st4_*: W3SDS4, W3SIN4, W3SPR4, CALC_USTAR, INSIN4, TABU_*), the W3SRCE unit (srce_*), the
# propagation unit (prop_*: W3XYP3, W3KTP3, W3QCK1-3, WAVNU1; W3XYP1, W3KTP1, W3XYP2, W3KTP2,
# W3XYPUG), W3WAVE's support routines
# (wave_*, updt_*, disp_*: W3GATH, W3SCAT, W3DZXY, W3UWND, W3UTRN, DISTAB, WAVNU2; prop_map*:
# W3MAP3, W3MAPT), NOAA's operational switches (srce_ref1, srce_ic4; pdlib_*: the unstructured
# PDLIB solver), the other source-term packages (srce_st0..st6, srce_flx1..5,
# srce_swl*: ST0, ST1, ST2, ST3, ST6, FLX1-5, SWL), the easy batch of routines of 100 lines or less
# (docs/EASY_BATCH_KOKKOS_PLAN_202610.md: profs_, tide_, uost_, fld1_, fld2_, gig1_, ic3_, is2_,
# bt8_, bt9_, snl4_, ic5_, snls_, gke_, snl5_, smco_, psmc_, iogo_, srce_, snl1_) and the binary
# fixture reader its L1 tests read the Fortran reference answers with.
#
# SPDX-License-Identifier: MIT
target_sources(ww_kokkos PRIVATE
    real.hpp
    spectrum_fixtures.hpp
    snl1_config.hpp
    snl1_tables.hpp
    snl1_tables.cpp
    snl1_dia.hpp
    snl1_dia.cpp
    snl1_team.hpp
    fixture_io.hpp
    fixture_io.cpp
    st4_params.hpp
    st4_arrays.hpp
    st4_libmvec.hpp
    st4_calc_ustar.hpp
    st4_spr4.hpp
    st4_sin4.hpp
    st4_sds4.hpp
    st4_tables_build.hpp
    st4_insin4.cpp
    st4_tabu_stress.cpp
    st4_tabu_tauhf.cpp
    st4_tabu_tauhf2.cpp
    srce_params.hpp
    prop_params.hpp
    prop_qck1.hpp
    prop_qck2.hpp
    prop_qck3.hpp
    prop_wavnu1.hpp
    prop_ktp3.hpp
    prop_xyp3.hpp
    prop_params1.hpp
    prop_params2.hpp
    prop_xyp1.hpp
    prop_ktp1.hpp
    prop_xyp2.hpp
    prop_ktp2.hpp
    prop_xypug.hpp
    prop.cpp
    wave_gath_scat.hpp
    updt_dzxy.hpp
    updt_uwnd.hpp
    updt_utrn.hpp
    disp_distab.hpp
    disp_wavnu3.hpp
    prop_map3.hpp
    prop_mapt.hpp
    srce_ref1.hpp
    srce_ic4.hpp
    srce_ic1.hpp
    srce_is1.hpp
    srce_tr1.hpp
    srce_bt4.hpp
    snl3_tables.hpp
    snl3_gmd.hpp
    part_ptsort.hpp
    part_ptnghb.hpp
    part_fld.hpp
    part_ptmean.hpp
    part_w3part.hpp
    pdlib_params.hpp
    pdlib_fsn2.hpp
    pdlib_fspsi2.hpp
    pdlib_fsfct2.hpp
    pdlib_w3xypug.hpp
    pdlib_boundary_va.hpp
    pdlib_jacobi_spectral.hpp
    pdlib_jacobi_vec.hpp
    pdlib_jgs_block.hpp
    pdlib_explicit_block.hpp
    pdlib_boundary.hpp
    prop_parall.hpp
    prop_freq_shift.hpp
    wave_support.cpp
    srce_kernel.hpp
    srce_ln1.hpp
    srce_bt1.hpp
    srce_db1.hpp
    srce_st0_spr0.hpp
    srce_st1_spr1.hpp
    srce_st1_sin1.hpp
    srce_st1_sds1.hpp
    srce_st2_inptab.hpp
    srce_st2_spr2.hpp
    srce_st2_sin2.hpp
    srce_st2_sds2.hpp
    srce_st3_tabu_stress.hpp
    srce_st3_tabu_tauhf.hpp
    srce_st3_calc_ustar.hpp
    srce_st3_insin3.hpp
    srce_st3_spr3.hpp
    srce_st3_sin3.hpp
    srce_st3_sds3.hpp
    srce_st6_util.hpp
    srce_st6_spr6.hpp
    srce_st6_sds6.hpp
    srce_st6_lfactor.hpp
    srce_st6_tau_wave_atmos.hpp
    srce_st6_sin6.hpp
    srce_swl_util.hpp
    srce_swl4.hpp
    srce_swl6.hpp
    srce_flx1.hpp
    srce_flx2.hpp
    srce_flx3.hpp
    srce_flx4.hpp
    srce_flx5.hpp
    bt8_ccosh.hpp
    bt8_csinh.hpp
    bt9_calc_nd.hpp
    fld1_infld.hpp
    fld1_sig2wn.hpp
    fld1_wnd2sat.hpp
    fld2_wnd2sat.hpp
    gig1_df1f2theta.hpp
    gke_asymsmattimvec.hpp
    gke_qfunc.hpp
    gke_ufunc.hpp
    gke_vmfunc.hpp
    gke_vpfunc.hpp
    ic3_cginic3_cheng.hpp
    ic3_delta.hpp
    ic3_func0_zhao.hpp
    ic3_f_zhao_cheng.hpp
    ic3_f_zhao_v1.hpp
    ic3_ic3table_cheng.hpp
    ic3_smooth_k.hpp
    ic5_cmplx_cosh.hpp
    ic5_cmplx_sinh.hpp
    ic5_cmplx_tanh2.hpp
    iogo_vmin_d.hpp
    iogo_vplus_d.hpp
    is2_funcd_fderiv.hpp
    is2_funcd_fval.hpp
    is2_w3rpwnice.hpp
    profs_amuxd.hpp
    profs_amuxe.hpp
    profs_amux.hpp
    profs_amuxj.hpp
    profs_amuxms.hpp
    profs_atmux.hpp
    profs_atmuxr.hpp
    profs_bisinit.hpp
    profs_brkdn.hpp
    profs_daxpy.hpp
    profs_ddot.hpp
    profs_dlassq.hpp
    profs_dnrm2.hpp
    profs_givens.hpp
    profs_ilu0.hpp
    profs_implu.hpp
    profs_ldsolc.hpp
    profs_ldsol.hpp
    profs_ldsoll.hpp
    profs_lsolc.hpp
    profs_lsol.hpp
    profs_lusol.hpp
    profs_lutsol.hpp
    profs_qsplit.hpp
    profs_runrc.hpp
    profs_setdepth.hpp
    profs_stopbis.hpp
    profs_tidycg.hpp
    profs_udsolc.hpp
    profs_udsol.hpp
    profs_uppdir.hpp
    profs_usolc.hpp
    profs_usol.hpp
    profs_vbrmv.hpp
    psmc_smcgtcrfr.hpp
    psmc_smckuno2.hpp
    psmc_smcxuno2r.hpp
    psmc_smcyuno2r.hpp
    smco_calc_interp.hpp
    smco_mapsta_smc.hpp
    smco_w3s2xy_smc.hpp
    smco_w3s2xy_smcnn.hpp
    smco_w3s2xy_smcnn_int.hpp
    smco_w3s2xy_smcrg_dir.hpp
    smco_w3s2xy_smcrg.hpp
    snl1_gauleg.hpp
    snl4_cgfnc.hpp
    snl4_wkfnc.hpp
    snl5_hasnan.hpp
    snls_expand.hpp
    srce_sign_vsd_patankar_ww3.hpp
    srce_sign_vsd_semi_implicit_ww3.hpp
    tide_astr.hpp
    tide_caldatt.hpp
    tide_dpythag.hpp
    tide_dsvbksb.hpp
    tide_tide_predict.hpp
    tide_tide_predict_only.hpp
    tide_tide_set_indices.hpp
    uost_compute_reduction_psi.hpp
    uost_uost_setgrid.hpp
    uost_uost_sourceterm_compute.hpp
    uost_uost_sourceterm_setgrid.hpp
    uost_uost_srctrmcompute.hpp
    bt9_ng.hpp
    fld1_wnd2z0m.hpp
    gke_biinterpwt.hpp
    gke_coocsrind.hpp
    gke_findquartetconfig.hpp
    gke_prepkgrid.hpp
    gke_tfunc.hpp
    ic3_bsdet.hpp
    ic3_cmplx_root_muller_v1.hpp
    ic3_drfun_quad_cheng.hpp
    ic3_func1_zhao.hpp
    ic3_ic3precalc_cheng.hpp
    ic3_wn_cmplx_hf.hpp
    ic5_balancing_matrix.hpp
    ic5_nr_corr.hpp
    ic5_polyroots.hpp
    ic5_w3ic5wncg.hpp
    iogo_w3flgrdflag.hpp
    iogo_w3flgrdupdt.hpp
    is2_w3fsd_dave.hpp
    parall_get_jsea_ibelong.hpp
    parall_init_get_isea.hpp
    parall_init_get_jsea_isproc.hpp
    parall_prop_refraction_pr1.hpp
    parall_set_up_nseal_nsealm.hpp
    profs_mgsro.hpp
    prop_cflxy.hpp
    psmc_smcaverg.hpp
    psmc_smcdcxy.hpp
    psmc_smcdhxy.hpp
    psmc_smcxuno2.hpp
    psmc_smcxuno3.hpp
    psmc_smcxuno3r.hpp
    psmc_smcyuno2.hpp
    psmc_smcyuno3.hpp
    psmc_smcyuno3r.hpp
    psmc_w3smcell.hpp
    psmc_w3smcgmp.hpp
    snl1_couple.hpp
    snl1_f1f1f1.hpp
    snl5_calc_wbtv2.hpp
    snl5_insnl5.hpp
    srce_calc_fpi.hpp
    tide_juldayt.hpp
    tide_setvuf.hpp
    tide_setvuf_fast.hpp
    tide_svd.hpp
    tide_tide_find_indices_analysis.hpp
    tide_tide_find_indices_prediction.hpp
    tide_vuf.hpp
    uost_uost_sourceterm_compute_ld.hpp
    uost_uost_sourceterm_compute_se.hpp
    updt_w3uic1.hpp
    updt_w3uic5.hpp
    updt_w3urho.hpp
    wavset_compute_active_node.hpp
    wavset_differentiate_xydir.hpp
    wavset_differentiate_xydir_mapsta.hpp
    wavset_differentiate_xydir_native.hpp
    wavset_fd_compute_diff.hpp
    wavset_fd_compute_lh_stress.hpp
    wavset_fd_set_meanvalue_to_zero.hpp
    wavset_fd_wave_setup_apply_fct.hpp
    wavset_fd_wave_setup_apply_precond.hpp
    wavset_fd_wave_setup_compute_system.hpp
    wavset_fd_wave_setup_scalar_prod.hpp
    wavset_fd_wave_setup_solve_poisson_neumann_dir.hpp
    wavset_preparation_fd_scheme.hpp
    wavset_trig_compute_diff.hpp
    wavset_trig_compute_lh_stress.hpp
    wavset_trig_wave_setup_apply_fct.hpp
    wavset_trig_wave_setup_apply_precond.hpp)

# No floating-point contraction in the ported kernels.
#
# GCC defaults to -ffp-contract=fast, so at -O3 -march=x86-64-v3 it fuses
# `AWG1*UE(..) + AWG2*UE(..)` into an FMA: one rounding where WW3's Fortran does
# two. That is a different computation, and it showed up exactly where it must --
# the openmp-release preset drifted from the committed Fortran fixture by 1.1e-5
# relative while serial-debug was bit-identical. The port's contract is the
# Fortran's arithmetic, so contraction is off and the answer is the same on every
# preset. Removing this line is a physics change, not an optimisation.
target_compile_options(ww_kokkos PRIVATE
    $<$<COMPILE_LANG_AND_ID:CXX,GNU,Clang>:-ffp-contract=off>)
# nvcc_wrapper reports itself to CMake as GNU, so the flag above lands on the
# *host* half of the compile only; the device half needs nvcc's own switch.
if(Kokkos_ENABLE_CUDA)
  target_compile_options(ww_kokkos PRIVATE $<$<COMPILE_LANGUAGE:CXX>:--fmad=false>)
endif()

# ST4 calls glibc's libmvec where gfortran -O3 does (st4_libmvec.hpp), so the
# host port computes the Fortran's bits at those five loops; ST3 and ST6 reuse
# it, and W3SIN6 adds _ZGVbN4v_tanhf (srce_st6_sin6.hpp).
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
  target_link_libraries(ww_kokkos PUBLIC mvec)
endif()
