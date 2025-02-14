/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef AUDIO_PROXY_INTERFACE_H
#define AUDIO_PROXY_INTERFACE_H


/* Volume Type */
enum {
    VOLUME_TYPE_OFFLOAD = 0,
    VOLUME_TYPE_MMAP,
    VOLUME_TYPE_CALL,
    VOLUME_TYPE_CALL_TX_MUTE,
    VOLUME_TYPE_CALL_RX_MUTE,
};

/* Compress Function Type */
enum {
    COMPRESS_TYPE_WAIT = 0,
    COMPRESS_TYPE_NEXTTRACK,
    COMPRESS_TYPE_PARTIALDRAIN,
    COMPRESS_TYPE_DRAIN,
};

/* Special Audio Device Type */
enum {
    BUILTIN_EARPIECE = 0,
    BUILTIN_SPEAKER,
    BUILTIN_MIC,
    PROXIMITY_SENSOR,
};

/* Audio Device Configuration Type */
enum {
    DEVICE_CONFIG_NONE = 0,
    DEVICE_CONFIG_INTERNAL,
    DEVICE_CONFIG_EXTERNAL,
};

enum {
    DEVICE_BLUETOOTH = 0,
    DEVICE_FMRADIO,
};

/* A-Box Interface Type */
enum {
    UAIF0 = 0,
    UAIF1,
    UAIF2,
    UAIF3,
};


// Audio Usage Check Utility Functions
bool is_active_usage_APCall(void *proxy);
bool is_usage_CPCall(audio_usage ausage);
bool is_active_usage_CPCall(void *proxy);
bool is_usage_APCall(audio_usage ausage);
bool is_usb_connected(void);

// Audio Stream Proxy Get/Set Fungtions
uint32_t proxy_get_actual_channel_count(void *proxy_stream);    // Return Actual Channel Count
uint32_t proxy_get_actual_sampling_rate(void *proxy_stream);    // Return Actual Samplung Rate
uint32_t proxy_get_actual_period_size(void *proxy_stream);
uint32_t proxy_get_actual_period_count(void *proxy_stream);
int32_t  proxy_get_actual_format(void *proxy_stream);           // Return Actual Android Audio Format, not PCM Format

// Audio Stream Proxy Offload Functions
void  proxy_offload_set_nonblock(void *proxy_stream);
int   proxy_offload_compress_func(void *proxy_stream, int func_type);
int   proxy_offload_pause(void *proxy_stream);
int   proxy_offload_resume(void *proxy_stream);

// Audio Stream Proxy Playback Stream Functions
void *proxy_create_playback_stream(void *proxy, int type, void *config, char *address);
void  proxy_destroy_playback_stream(void *proxy_stream);
int   proxy_close_playback_stream(void *proxy_stream);
int   proxy_open_playback_stream(void *proxy_stream, int32_t min_size_frames, void *mmap_info);
int   proxy_start_playback_stream(void *proxy_stream);
int   proxy_write_playback_buffer(void *proxy_stream, void *buffer, int bytes);
int   proxy_stop_playback_stream(void *proxy_stream);
int   proxy_reconfig_playback_stream(void *proxy_stream, int type, void *config);
int   proxy_get_render_position(void *proxy_stream, uint32_t *frames);
int   proxy_get_presen_position(void *proxy_stream, uint64_t *frames, struct timespec *timestamp);
int   proxy_getparam_playback_stream(void *proxy_stream, void *query_params, void *reply_params);
int   proxy_setparam_playback_stream(void *proxy_stream, void *parameters);
uint32_t proxy_get_playback_latency(void *proxy_stream);
void  proxy_dump_playback_stream(void *proxy_stream, int fd);

// Audio Stream Proxy Capture Stream Functions
void *proxy_create_capture_stream(void *proxy, int type, int usage, void *config, char *address);
void  proxy_destroy_capture_stream(void *proxy_stream);
int   proxy_close_capture_stream(void *proxy_stream);
int   proxy_open_capture_stream(void *proxy_stream, int32_t min_size_frames, void *mmap_info);
int   proxy_start_capture_stream(void *proxy_stream);
int   proxy_read_capture_buffer(void *proxy_stream, void *buffer, int bytes);
int   proxy_stop_capture_stream(void *proxy_stream);
int   proxy_reconfig_capture_stream(void *proxy_stream, int type, void *config);
int   proxy_reconfig_capture_usage(void *proxy_stream, int type, int usage);
int   proxy_get_capture_pos(void *proxy_stream, int64_t *frames, int64_t *time);
int   proxy_get_active_microphones(void *proxy_stream, void *array, int *count);
int   proxy_getparam_capture_stream(void *proxy_stream, void *query_params, void *reply_params);
int   proxy_setparam_capture_stream(void *proxy_stream, void *parameters);
void  proxy_dump_capture_stream(void *proxy_stream, int fd);
void  proxy_update_capture_usage(void *proxy_stream, int usage);

int   proxy_get_mmap_position(void *proxy_stream, void *pos);

// Audio Device Proxy Path Route Functions
bool  proxy_init_route(void *proxy, char *path);
void  proxy_deinit_route(void *proxy);
bool  proxy_update_route(void *proxy, int ausage, int device);
bool  proxy_set_route(void *proxy, int ausage, int device, int modifier, bool set);

// Audio Device Proxy Functions
void  proxy_stop_voice_call(void *proxy);
void  proxy_start_voice_call(void *proxy);
void  proxy_stop_fm_radio(void *proxy);
void  proxy_start_fm_radio(void *proxy);

int   proxy_get_mixer_value_int(void *proxy, const char *name);
int   proxy_get_mixer_value_array(void *proxy, const char *name, void *value, int count);
void  proxy_set_mixer_value_int(void *proxy, const char *name, int value);
void  proxy_set_mixer_value_string(void *proxy, const char *name, const char *value);
void  proxy_set_mixer_value_array(void *proxy, const char *name, const void *value, int count);
void  proxy_set_audio_interface(void *proxy, unsigned int interface, unsigned int sample_rate,
                                unsigned int bit_width, unsigned int channel);
void  proxy_set_audiomode(void *proxy, int audiomode);
void  proxy_set_volume(void *proxy, int volume_type, float left, float right);
void  proxy_set_upscale(void *proxy, int sampling_rate, int pcm_format);
#ifdef SUPPORT_STHAL_INTERFACE
int   proxy_check_sthalstate(void *proxy);
#endif
void  proxy_call_status(void *proxy, int status);
int   proxy_set_parameters(void *proxy, void *parameters);
int   proxy_get_microphones(void *proxy, void *array, int *count);
void  proxy_set_apcall_txse(void);
void  proxy_clear_apcall_txse(void);
void  proxy_set_call_path_param(uint32_t set, uint32_t param, int32_t value);

#ifdef SUPPORT_BTA2DP_OFFLOAD
bool proxy_is_bt_a2dp_ready(void);
#endif

// Audio Device Proxy Dump Function
int   proxy_fw_dump(int fd);

// Audio Device Proxy Creation/Destruction
bool  proxy_is_initialized(void);
void *proxy_init(void);
void  proxy_deinit(void *proxy);

#endif /* AUDIO_PROXY_INTERFACE_H */
