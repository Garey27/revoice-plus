cmake_minimum_required(VERSION 3.3)

if(POLICY CMP0069)
    cmake_policy(SET CMP0069 NEW)
endif()
cmake_policy(SET CMP0063 NEW)
if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
endif()

project(Opus C)

add_library(${PROJECT_NAME} STATIC
    config.h

    celt/_kiss_fft_guts.h
    celt/arch.h
    celt/bands.c celt/bands.h
    celt/celt.c celt/celt.h
    celt/celt_decoder.c
    celt/celt_encoder.c
    celt/celt_lpc.c celt/celt_lpc.h
    celt/cpu_support.h
    celt/cwrs.c celt/cwrs.h
    celt/ecintrin.h
    celt/entcode.c celt/entcode.h
    celt/entdec.c celt/entdec.h
    celt/entenc.c celt/entenc.h
    celt/fixed_debug.h
    celt/fixed_generic.h
    celt/float_cast.h
    celt/kiss_fft.c celt/kiss_fft.h
    celt/laplace.c celt/laplace.h
    celt/mathops.c celt/mathops.h
    celt/mdct.c celt/mdct.h
    celt/mfrngcod.h
    celt/modes.c celt/modes.h
    celt/os_support.h
    celt/pitch.c celt/pitch.h
    celt/quant_bands.c celt/quant_bands.h
    celt/rate.c celt/rate.h
    celt/stack_alloc.h
    celt/static_modes_fixed.h
    celt/static_modes_fixed_arm_ne10.h
    celt/static_modes_float.h
    celt/static_modes_float_arm_ne10.h
    celt/vq.c celt/vq.h

    celt/x86/celt_lpc_sse.c celt/x86/celt_lpc_sse.h
    celt/x86/pitch_sse.c celt/x86/pitch_sse.h
    celt/x86/pitch_sse2.c
    celt/x86/pitch_sse4_1.c
    celt/x86/vq_sse.h
    celt/x86/vq_sse2.c
    celt/x86/x86cpu.c celt/x86/x86cpu.h
    celt/x86/x86_celt_map.c

    silk/A2NLSF.c
    silk/ana_filt_bank_1.c
    silk/API.h
    silk/biquad_alt.c
    silk/bwexpander.c
    silk/bwexpander_32.c
    silk/check_control_input.c
    silk/CNG.c
    silk/code_signs.c
    silk/control.h
    silk/control_audio_bandwidth.c
    silk/control_codec.c
    silk/control_SNR.c
    silk/debug.c silk/debug.h
    silk/decoder_set_fs.c
    silk/decode_core.c
    silk/decode_frame.c
    silk/decode_indices.c
    silk/decode_parameters.c
    silk/decode_pitch.c
    silk/decode_pulses.c
    silk/dec_API.c
    silk/define.h
    silk/encode_indices.c
    silk/encode_pulses.c
    silk/enc_API.c
    silk/errors.h
    silk/gain_quant.c
    silk/HP_variable_cutoff.c
    silk/init_decoder.c
    silk/init_encoder.c
    silk/Inlines.h
    silk/inner_prod_aligned.c
    silk/interpolate.c
    silk/lin2log.c
    silk/log2lin.c
    silk/LPC_analysis_filter.c
    silk/LPC_fit.c
    silk/LPC_inv_pred_gain.c
    silk/LP_variable_cutoff.c
    silk/MacroCount.h
    silk/MacroDebug.h
    silk/macros.h
    silk/main.h
    silk/NLSF2A.c
    silk/NLSF_decode.c
    silk/NLSF_del_dec_quant.c
    silk/NLSF_encode.c
    silk/NLSF_stabilize.c
    silk/NLSF_unpack.c
    silk/NLSF_VQ.c
    silk/NLSF_VQ_weights_laroia.c
    silk/NSQ.c silk/NSQ.h
    silk/NSQ_del_dec.c
    silk/pitch_est_defines.h
    silk/pitch_est_tables.c
    silk/PLC.c silk/PLC.h
    silk/process_NLSFs.c
    silk/quant_LTP_gains.c
    silk/resampler.c
    silk/resampler_down2.c
    silk/resampler_down2_3.c
    silk/resampler_private.h
    silk/resampler_private_AR2.c
    silk/resampler_private_down_FIR.c
    silk/resampler_private_IIR_FIR.c
    silk/resampler_private_up2_HQ.c
    silk/resampler_rom.c silk/resampler_rom.h
    silk/resampler_structs.h
    silk/shell_coder.c
    silk/sigm_Q15.c
    silk/SigProc_FIX.h
    silk/sort.c
    silk/stereo_decode_pred.c
    silk/stereo_encode_pred.c
    silk/stereo_find_predictor.c
    silk/stereo_LR_to_MS.c
    silk/stereo_MS_to_LR.c
    silk/stereo_quant_pred.c
    silk/structs.h
    silk/sum_sqr_shift.c
    silk/tables.h
    silk/tables_gain.c
    silk/tables_LTP.c
    silk/tables_NLSF_CB_NB_MB.c
    silk/tables_NLSF_CB_WB.c
    silk/tables_other.c
    silk/tables_pitch_lag.c
    silk/tables_pulses_per_block.c
    silk/table_LSF_cos.c
    silk/tuning_parameters.h
    silk/typedef.h
    silk/VAD.c
    silk/VQ_WMat_EC.c

    silk/float/apply_sine_window_FLP.c
    silk/float/autocorrelation_FLP.c
    silk/float/burg_modified_FLP.c
    silk/float/bwexpander_FLP.c
    silk/float/corrMatrix_FLP.c
    silk/float/encode_frame_FLP.c
    silk/float/energy_FLP.c
    silk/float/find_LPC_FLP.c
    silk/float/find_LTP_FLP.c
    silk/float/find_pitch_lags_FLP.c
    silk/float/find_pred_coefs_FLP.c
    silk/float/inner_product_FLP.c
    silk/float/k2a_FLP.c
    silk/float/LPC_analysis_filter_FLP.c
    silk/float/LPC_inv_pred_gain_FLP.c
    silk/float/LTP_analysis_filter_FLP.c
    silk/float/LTP_scale_ctrl_FLP.c
    silk/float/main_FLP.h
    silk/float/noise_shape_analysis_FLP.c
    silk/float/pitch_analysis_core_FLP.c
    silk/float/process_gains_FLP.c
    silk/float/regularize_correlations_FLP.c
    silk/float/residual_energy_FLP.c
    silk/float/scale_copy_vector_FLP.c
    silk/float/scale_vector_FLP.c
    silk/float/schur_FLP.c
    silk/float/SigProc_FLP.h
    silk/float/sort_FLP.c
    silk/float/structs_FLP.h
    silk/float/warped_autocorrelation_FLP.c
    silk/float/wrappers_FLP.c
    silk/x86/main_sse.h
    silk/x86/NSQ_del_dec_sse.c
    silk/x86/NSQ_sse.c
    silk/x86/SigProc_FIX_sse.h
    silk/x86/VAD_sse.c
    silk/x86/VQ_WMat_EC_sse.c
    silk/x86/x86_silk_map.c

    src/analysis.c src/analysis.h
    src/mlp.c
    src/mlp.h
    src/mlp_data.c
    src/opus.c
    src/opus_decoder.c
    src/opus_encoder.c
    src/opus_multistream.c
    src/opus_multistream_decoder.c
    src/opus_multistream_encoder.c
    src/opus_private.h
    src/repacketizer.c
    src/tansig_table.h
)

if(NOT MSVC)
    if(CMAKE_VERSION VERSION_LESS "3.11")
        set(compile_options_name COMPILE_FLAGS)
    else()
        set(compile_options_name COMPILE_OPTIONS)
    endif()

    set_source_files_properties(celt/x86/celt_lpc_sse.c
        PROPERTIES ${compile_options_name} -msse4.1)

    set_source_files_properties(celt/x86/pitch_sse.c
        PROPERTIES ${compile_options_name} -msse)

    set_source_files_properties(celt/x86/pitch_sse2.c
        PROPERTIES ${compile_options_name} -msse2)

    set_source_files_properties(celt/x86/pitch_sse4_1.c
        PROPERTIES ${compile_options_name} -msse4.1)

    set_source_files_properties(celt/x86/vq_sse2.c
        PROPERTIES ${compile_options_name} -msse2)

    set_source_files_properties(silk/x86/NSQ_del_dec_sse.c
        PROPERTIES ${compile_options_name} -msse4.1)

    set_source_files_properties(silk/x86/NSQ_sse.c
        PROPERTIES ${compile_options_name} -msse4.1)

    set_source_files_properties(silk/x86/VAD_sse.c
        PROPERTIES ${compile_options_name} -msse4.1)

    set_source_files_properties(silk/x86/VQ_WMat_EC_sse.c
        PROPERTIES ${compile_options_name} -msse4.1)
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_CONFIG_H=)

target_compile_definitions(${PROJECT_NAME} PRIVATE OPUS_EXPORT=)

target_include_directories(${PROJECT_NAME} PRIVATE . celt silk silk/float PUBLIC include)