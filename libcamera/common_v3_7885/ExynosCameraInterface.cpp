/*
**
** Copyright 2013, Samsung Electronics Co. LTD
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

/* #define LOG_NDEBUG 0 */
#define LOG_TAG "ExynosCameraInterface"
#include <cutils/log.h>

#include "ExynosCameraInterface.h"
#include "ExynosCameraAutoTimer.h"

#ifdef SAMSUNG_TN_FEATURE
#include "SecCameraVendorTags.h"
#endif

namespace android {

static int HAL_camera_device_open(
        const struct hw_module_t* module,
        const char *id,
        struct hw_device_t** device)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    int cameraId = atoi(id);
    CameraMetadata metadata;
    camera_metadata_entry flashAvailable;
    bool hasFlash = false;
    FILE *fp = NULL;
    char flashFilePath[100] = {'\0',};

#ifdef BOARD_FRONT_CAMERA_ONLY_USE
    cameraId += 1;
#endif

#ifdef BOARD_BACK_CAMERA_USES_EXTERNAL_CAMERA
    if (cameraId == 0) {
        return HAL_ext_camera_device_open_wrapper(module, id, device);
    }
#endif

#ifdef BOARD_FRONT_CAMERA_USES_EXTERNAL_CAMERA
    if (cameraId == 1) {
        return HAL_ext_camera_device_open_wrapper(module, id, device);
    }
#endif

#if (defined BOARD_BACK_CAMERA_USES_EXTERNAL_CAMERA) && (defined BOARD_FRONT_CAMERA_USES_EXTERNAL_CAMERA)
#else
    enum CAMERA_STATE state;

    ALOGI("INFO(%s[%d]):camera(%d) in", __FUNCTION__, __LINE__, cameraId);

#ifdef BOARD_FRONT_CAMERA_ONLY_USE
    if (cameraId < 1 || cameraId > HAL_getNumberOfCameras())
#else
    if (cameraId < 0 || cameraId >= HAL_getNumberOfCameras())
#endif
    {
        ALOGE("ERR(%s):Invalid camera ID %s", __FUNCTION__, id);
        return -EINVAL;
    }

#ifdef FRONT_1_CAMERA_SENSOR_NAME
    /*
     * if it use dual camera.
     * just calibrate it as proper cameraId_1.
     */
    int cameraId_0 = -1;
    int cameraId_1 = -1;

    getDualCameraId(&cameraId_0, &cameraId_1);

    if (0 < cameraId_1) {
        if (CAMERA_ID_BACK_1 <= cameraId) {
            ALOGD("DEBUG(%s[%d]):calibrate camera ID(%d) -> camera ID(%d) for dual camera",
                __FUNCTION__, __LINE__, cameraId, cameraId_1);
            cameraId = cameraId_1;
        }
    }
#endif

    state = CAMERA_OPENED;
    if (check_camera_state(state, cameraId) == false) {
        ALOGE("ERR(%s):camera(%d) state(%d) is INVALID", __FUNCTION__, cameraId, state);
        return -EUSERS;
    }

#ifdef BOARD_FRONT_CAMERA_ONLY_USE
    if ((unsigned int)cameraId <= (sizeof(sCameraInfo) / sizeof(sCameraInfo[0])))
#else
    if ((unsigned int)cameraId < (sizeof(sCameraInfo) / sizeof(sCameraInfo[0])))
#endif
    {
        if (g_cam_device[cameraId]) {
            ALOGE("DEBUG(%s):returning existing camera ID %s", __FUNCTION__, id);
            *device = (hw_device_t *)g_cam_device[cameraId];
            goto done;
        }

        g_cam_device[cameraId] = (camera_device_t *)malloc(sizeof(camera_device_t));
        if (!g_cam_device[cameraId])
            return -ENOMEM;

        g_cam_openLock[cameraId].lock();
        g_cam_device[cameraId]->common.tag     = HARDWARE_DEVICE_TAG;
        g_cam_device[cameraId]->common.version = 1;
        g_cam_device[cameraId]->common.module  = const_cast<hw_module_t *>(module);
        g_cam_device[cameraId]->common.close   = HAL_camera_device_close;

        g_cam_device[cameraId]->ops = &camera_device_ops;

        ALOGD("DEBUG(%s):open camera %s", __FUNCTION__, id);
        g_cam_device[cameraId]->priv = new ExynosCamera(cameraId, g_cam_device[cameraId]);
        *device = (hw_device_t *)g_cam_device[cameraId];
        ALOGI("INFO(%s[%d]):camera(%d) out from new g_cam_device[%d]->priv()",
            __FUNCTION__, __LINE__, cameraId, cameraId);

        g_cam_openLock[cameraId].unlock();
        ALOGI("INFO(%s[%d]):camera(%d) unlocked..", __FUNCTION__, __LINE__, cameraId);
    } else {
        ALOGE("DEBUG(%s):camera(%s) open fail - must front camera open first",
            __FUNCTION__, id);
        return -EINVAL;
    }

done:
    cam_stateLock[cameraId].lock();
    cam_state[cameraId] = state;
    cam_stateLock[cameraId].unlock();

    if (g_cam_info[cameraId]) {
        metadata = g_cam_info[cameraId];
        flashAvailable = metadata.find(ANDROID_FLASH_INFO_AVAILABLE);

        if (flashAvailable.count == 1 && flashAvailable.data.u8[0] == 1) {
            hasFlash = true;
        } else {
            hasFlash = false;
        }
    }

    if(hasFlash && g_cam_torchEnabled[cameraId]) {
        if (cameraId == CAMERA_ID_BACK) {
            snprintf(flashFilePath, sizeof(flashFilePath), TORCH_REAR_FILE_PATH);
        } else {
            snprintf(flashFilePath, sizeof(flashFilePath), TORCH_FRONT_FILE_PATH);
        }

        fp = fopen(flashFilePath, "w+");

        if (fp == NULL) {
            ALOGE("ERR(%s[%d]):torch file open(%s) fail",
                __FUNCTION__, __LINE__, flashFilePath);
        } else {
            fwrite("0", sizeof(char), 1, fp);
            fflush(fp);
            fclose(fp);

            g_cam_torchEnabled[cameraId] = false;
        }
    }

    if (g_callbacks) {
        g_callbacks->torch_mode_status_change(g_callbacks, id, TORCH_MODE_STATUS_NOT_AVAILABLE);
        ALOGI("INFO(%s[%d]):camera(%d) TORCH_MODE_STATUS_NOT_AVAILABLE", __FUNCTION__, __LINE__, cameraId);
    }

    ALOGI("INFO(%s[%d]):camera(%d) out", __FUNCTION__, __LINE__, cameraId);
#endif /* (defined BOARD_BACK_CAMERA_USES_EXTERNAL_CAMERA) && (defined BOARD_FRONT_CAMERA_USES_EXTERNAL_CAMERA) */

    return 0;
}

static int HAL_camera_device_close(struct hw_device_t* device)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    uint32_t cameraId = -1;
    enum CAMERA_STATE state;
    char camid[10];

#if (defined BOARD_BACK_CAMERA_USES_EXTERNAL_CAMERA) && (defined BOARD_FRONT_CAMERA_USES_EXTERNAL_CAMERA)
#else
    ALOGI("INFO(%s[%d]): in", __FUNCTION__, __LINE__);

    if (device) {
        camera_device_t *cam_device = (camera_device_t *)device;
        cameraId = obj(cam_device)->getCameraId();

        ALOGI("INFO(%s[%d]):camera(%d)", __FUNCTION__, __LINE__, cameraId);

        state = CAMERA_CLOSED;
        if (check_camera_state(state, cameraId) == false) {
            ALOGE("ERR(%s):camera(%d) state(%d) is INVALID",
                __FUNCTION__, cameraId, state);
            return -1;
        }

        g_cam_openLock[cameraId].lock();
        ALOGI("INFO(%s[%d]):camera(%d) locked..", __FUNCTION__, __LINE__, cameraId);
        g_cam_device[cameraId] = NULL;
        g_cam_openLock[cameraId].unlock();
        ALOGI("INFO(%s[%d]):camera(%d) unlocked..", __FUNCTION__, __LINE__, cameraId);

        delete static_cast<ExynosCamera *>(cam_device->priv);
        free(cam_device);

        cam_stateLock[cameraId].lock();
        cam_state[cameraId] = state;
        cam_stateLock[cameraId].unlock();
        ALOGI("INFO(%s[%d]):camera(%d)", __FUNCTION__, __LINE__, cameraId);
    } else {
        ALOGE("ERR(%s) invalid device", __FUNCTION__);
        return -1;
    }

    /* Update torch status */
    g_cam_torchEnabled[cameraId] = false;
    snprintf(camid, sizeof(camid), "%d\n", cameraId);
    if (g_callbacks) {
        g_callbacks->torch_mode_status_change(g_callbacks, camid, TORCH_MODE_STATUS_AVAILABLE_OFF);
        ALOGI("INFO(%s[%d]):camera(%d) TORCH_MODE_STATUS_AVAILABLE_OFF", __FUNCTION__, __LINE__, cameraId);
    }
    ALOGI("INFO(%s[%d]): out", __FUNCTION__, __LINE__);
#endif /* (defined BOARD_BACK_CAMERA_USES_EXTERNAL_CAMERA) && (defined BOARD_FRONT_CAMERA_USES_EXTERNAL_CAMERA) */

    return 0;
}

static int HAL_camera_device_set_preview_window(
        struct camera_device *dev,
        struct preview_stream_ops *buf)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    static int ret;
    uint32_t cameraId = obj(dev)->getCameraId();

    ALOGI("INFO(%s[%d]):camera(%d) in", __FUNCTION__, __LINE__, cameraId);
    ret = obj(dev)->setPreviewWindow(buf);
    ALOGI("INFO(%s[%d]):camera(%d) out", __FUNCTION__, __LINE__, cameraId);
    return ret;
}

static void HAL_camera_device_set_callbacks(struct camera_device *dev,
        camera_notify_callback notify_cb,
        camera_data_callback data_cb,
        camera_data_timestamp_callback data_cb_timestamp,
        camera_request_memory get_memory,
        void* user)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    ALOGV("DEBUG(%s):", __FUNCTION__);
    obj(dev)->setCallbacks(notify_cb, data_cb, data_cb_timestamp,
                           get_memory,
                           user);
}

static void HAL_camera_device_enable_msg_type(
        struct camera_device *dev,
        int32_t msg_type)
{
    ALOGV("DEBUG(%s):", __FUNCTION__);
    obj(dev)->enableMsgType(msg_type);
}

static void HAL_camera_device_disable_msg_type(
        struct camera_device *dev,
        int32_t msg_type)
{
    ALOGV("DEBUG(%s):", __FUNCTION__);
    obj(dev)->disableMsgType(msg_type);
}

static int HAL_camera_device_msg_type_enabled(
        struct camera_device *dev,
        int32_t msg_type)
{
    ALOGV("DEBUG(%s):", __FUNCTION__);
    return obj(dev)->msgTypeEnabled(msg_type);
}

static int HAL_camera_device_start_preview(struct camera_device *dev)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    static int ret;
    uint32_t cameraId = obj(dev)->getCameraId();
    enum CAMERA_STATE state;

    ALOGI("INFO(%s[%d]):camera(%d) in", __FUNCTION__, __LINE__, cameraId);

    state = CAMERA_PREVIEW;
    if (check_camera_state(state, cameraId) == false) {
        ALOGE("ERR(%s):camera(%d) state(%d) is INVALID",
            __FUNCTION__, cameraId, state);
        return -1;
    }

    g_cam_previewLock[cameraId].lock();

#ifdef DUAL_CAMERA_SUPPORTED
    if (cameraId != 0 && g_cam_device[0] != NULL
        && cam_state[0] != CAMERA_NONE && cam_state[0] != CAMERA_CLOSED) {
        ret = obj(dev)->setDualMode(true);
        if (ret != NO_ERROR)
            ALOGE("ERR(%s[%d]):camera(%d) set dual mode fail, ret(%d)",
                    __FUNCTION__, __LINE__, cameraId, ret);
        else
            ALOGI("INFO(%s[%d]):camera(%d) set dual mode)",
                    __FUNCTION__, __LINE__, cameraId);
    }
#endif

    ret = obj(dev)->startPreview();
    ALOGI("INFO(%s[%d]):camera(%d) out from startPreview()",
        __FUNCTION__, __LINE__, cameraId);

    g_cam_previewLock[cameraId].unlock();

    ALOGI("INFO(%s[%d]):camera(%d) unlocked..", __FUNCTION__, __LINE__, cameraId);

    if (ret == OK) {
        cam_stateLock[cameraId].lock();
        cam_state[cameraId] = state;
        cam_stateLock[cameraId].unlock();
        ALOGI("INFO(%s[%d]):camera(%d) out (startPreview succeeded)",
            __FUNCTION__, __LINE__, cameraId);
    } else {
        ALOGI("INFO(%s[%d]):camera(%d) out (startPreview FAILED)",
            __FUNCTION__, __LINE__, cameraId);
    }
    return ret;
}

static void HAL_camera_device_stop_preview(struct camera_device *dev)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    uint32_t cameraId = obj(dev)->getCameraId();
    enum CAMERA_STATE state;

    ALOGI("INFO(%s[%d]):camera(%d) in", __FUNCTION__, __LINE__, cameraId);
/* HACK : If camera in recording state, */
/*        CameraService have to call the stop_recording before the stop_preview */
#if 1
    if (cam_state[cameraId] == CAMERA_RECORDING) {
        ALOGE("ERR(%s[%d]):camera(%d) in RECORDING RUNNING state ---- INVALID ----",
            __FUNCTION__, __LINE__, cameraId);
        ALOGE("ERR(%s[%d]):camera(%d) The stop_recording must be called "
            "before the stop_preview  ---- INVALID ----",
            __FUNCTION__, __LINE__,  cameraId);
        HAL_camera_device_stop_recording(dev);
        ALOGE("ERR(%s[%d]):cameraId=%d out from stop_recording  ---- INVALID ----",
            __FUNCTION__, __LINE__,  cameraId);

        for (int i=0; i<30; i++) {
            ALOGE("ERR(%s[%d]):camera(%d) The stop_recording must be called "
                "before the stop_preview  ---- INVALID ----",
                __FUNCTION__, __LINE__,  cameraId);
        }
        ALOGE("ERR(%s[%d]):camera(%d) sleep 500ms for ---- INVALID ---- state",
            __FUNCTION__, __LINE__,  cameraId);
        usleep(500000); /* to notify, sleep 500ms */
    }
#endif
    state = CAMERA_PREVIEWSTOPPED;
    if (check_camera_state(state, cameraId) == false) {
        ALOGE("ERR(%s):camera(%d) state(%d) is INVALID", __FUNCTION__, cameraId, state);
        return;
    }

    g_cam_previewLock[cameraId].lock();

    obj(dev)->stopPreview();
    ALOGI("INFO(%s[%d]):camera(%d) out from stopPreview()",
        __FUNCTION__, __LINE__, cameraId);

    g_cam_previewLock[cameraId].unlock();

    ALOGI("INFO(%s[%d]):camera(%d) unlocked..", __FUNCTION__, __LINE__, cameraId);

    cam_stateLock[cameraId].lock();
    cam_state[cameraId] = state;
    cam_stateLock[cameraId].unlock();
    ALOGI("INFO(%s[%d]):camera(%d) out", __FUNCTION__, __LINE__, cameraId);
}

static int HAL_camera_device_preview_enabled(struct camera_device *dev)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    ALOGV("DEBUG(%s):", __FUNCTION__);
    return obj(dev)->previewEnabled();
}

static int HAL_camera_device_store_meta_data_in_buffers(
        struct camera_device *dev,
        int enable)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    ALOGV("DEBUG(%s):", __FUNCTION__);
    return obj(dev)->storeMetaDataInBuffers(enable);
}

static int HAL_camera_device_start_recording(struct camera_device *dev)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    static int ret;
    uint32_t cameraId = obj(dev)->getCameraId();
    enum CAMERA_STATE state;

    ALOGI("INFO(%s[%d]):camera(%d) in", __FUNCTION__, __LINE__, cameraId);

    state = CAMERA_RECORDING;
    if (check_camera_state(state, cameraId) == false) {
        ALOGE("ERR(%s):camera(%d) state(%d) is INVALID",
            __FUNCTION__, cameraId, state);
        return -1;
    }

    g_cam_recordingLock[cameraId].lock();

    ret = obj(dev)->startRecording();
    ALOGI("INFO(%s[%d]):camera(%d) out from startRecording()",
        __FUNCTION__, __LINE__, cameraId);

    g_cam_recordingLock[cameraId].unlock();

    ALOGI("INFO(%s[%d]):camera(%d) unlocked..", __FUNCTION__, __LINE__, cameraId);

    if (ret == OK) {
        cam_stateLock[cameraId].lock();
        cam_state[cameraId] = state;
        cam_stateLock[cameraId].unlock();
        ALOGI("INFO(%s[%d]):camera(%d) out (startRecording succeeded)",
            __FUNCTION__, __LINE__, cameraId);
    } else {
        ALOGI("INFO(%s[%d]):camera(%d) out (startRecording FAILED)",
            __FUNCTION__, __LINE__, cameraId);
    }
    return ret;
}

static void HAL_camera_device_stop_recording(struct camera_device *dev)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    uint32_t cameraId = obj(dev)->getCameraId();
    enum CAMERA_STATE state;

    ALOGI("INFO(%s[%d]):camera(%d) in", __FUNCTION__, __LINE__, cameraId);

    state = CAMERA_RECORDINGSTOPPED;
    if (check_camera_state(state, cameraId) == false) {
        ALOGE("ERR(%s):camera(%d) state(%d) is INVALID", __FUNCTION__, cameraId, state);
        return;
    }

    g_cam_recordingLock[cameraId].lock();

    obj(dev)->stopRecording();
    ALOGI("INFO(%s[%d]):camera(%d) out from stopRecording()",
        __FUNCTION__, __LINE__, cameraId);

    g_cam_recordingLock[cameraId].unlock();

    ALOGI("INFO(%s[%d]):camera(%d) unlocked..", __FUNCTION__, __LINE__, cameraId);

    cam_stateLock[cameraId].lock();
    cam_state[cameraId] = state;
    cam_stateLock[cameraId].unlock();
    ALOGI("INFO(%s[%d]):camera(%d) out", __FUNCTION__, __LINE__, cameraId);
}

static int HAL_camera_device_recording_enabled(struct camera_device *dev)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    ALOGV("DEBUG(%s):", __FUNCTION__);
    return obj(dev)->recordingEnabled();
}

static void HAL_camera_device_release_recording_frame(struct camera_device *dev,
                                const void *opaque)
{
    /* ExynosCameraAutoTimer autoTimer(__FUNCTION__); */

    ALOGV("DEBUG(%s):", __FUNCTION__);
    obj(dev)->releaseRecordingFrame(opaque);
}

static int HAL_camera_device_auto_focus(struct camera_device *dev)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    ALOGV("DEBUG(%s):", __FUNCTION__);
    return obj(dev)->autoFocus();
}

static int HAL_camera_device_cancel_auto_focus(struct camera_device *dev)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    ALOGV("DEBUG(%s):", __FUNCTION__);
    return obj(dev)->cancelAutoFocus();
}

static int HAL_camera_device_take_picture(struct camera_device *dev)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    ALOGV("DEBUG(%s):", __FUNCTION__);
    return obj(dev)->takePicture();
}

static int HAL_camera_device_cancel_picture(struct camera_device *dev)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    ALOGV("DEBUG(%s):", __FUNCTION__);
    return obj(dev)->cancelPicture();
}

static int HAL_camera_device_set_parameters(
        struct camera_device *dev,
        const char *parms)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    ALOGV("DEBUG(%s):", __FUNCTION__);
    String8 str(parms);
    CameraParameters p(str);
    return obj(dev)->setParameters(p);
}

char *HAL_camera_device_get_parameters(struct camera_device *dev)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    ALOGV("DEBUG(%s):", __FUNCTION__);
    String8 str;

/* HACK : to avoid compile error */
#if (defined BOARD_BACK_CAMERA_USES_EXTERNAL_CAMERA) && (defined BOARD_FRONT_CAMERA_USES_EXTERNAL_CAMERA)
    ALOGE("ERR(%s[%d]):invalid opertion on external camera", __FUNCTION__, __LINE__);
#else
    CameraParameters parms = obj(dev)->getParameters();
    str = parms.flatten();
#endif
    return strdup(str.c_str());
}

static void HAL_camera_device_put_parameters(
        __unused struct camera_device *dev,
        char *parms)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    ALOGV("DEBUG(%s):", __FUNCTION__);
    free(parms);
}

static int HAL_camera_device_send_command(
        struct camera_device *dev,
        int32_t cmd,
        int32_t arg1,
        int32_t arg2)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    ALOGV("DEBUG(%s):", __FUNCTION__);
    return obj(dev)->sendCommand(cmd, arg1, arg2);
}

static void HAL_camera_device_release(struct camera_device *dev)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    uint32_t cameraId = obj(dev)->getCameraId();
    enum CAMERA_STATE state;

    ALOGI("INFO(%s[%d]):camera(%d) in", __FUNCTION__, __LINE__, cameraId);

    state = CAMERA_RELEASED;
    if (check_camera_state(state, cameraId) == false) {
        ALOGE("ERR(%s):camera(%d) state(%d) is INVALID",
            __FUNCTION__, cameraId, state);
        return;
    }

    g_cam_openLock[cameraId].lock();

    obj(dev)->release();
    ALOGI("INFO(%s[%d]):camera(%d) out from release()",
        __FUNCTION__, __LINE__, cameraId);

    g_cam_openLock[cameraId].unlock();

    ALOGI("INFO(%s[%d]):camera(%d) unlocked..", __FUNCTION__, __LINE__, cameraId);

    cam_stateLock[cameraId].lock();
    cam_state[cameraId] = state;
    cam_stateLock[cameraId].unlock();
    ALOGI("INFO(%s[%d]):camera(%d) out", __FUNCTION__, __LINE__, cameraId);
}

static int HAL_camera_device_dump(struct camera_device *dev, int fd)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    ALOGV("DEBUG(%s):", __FUNCTION__);
    return obj(dev)->dump(fd);
}

static int HAL_getNumberOfCameras()
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    ALOGV("DEBUG(%s):", __FUNCTION__);
    return sizeof(sCameraInfo) / sizeof(sCameraInfo[0]);
}

static int HAL_set_callbacks(const camera_module_callbacks_t *callbacks)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    if (callbacks == NULL)
        ALOGE("ERR(%s[%d]):dev is NULL", __FUNCTION__, __LINE__);

    g_callbacks = callbacks;

    return 0;
}

static int HAL_getCameraInfo(int cameraId, struct camera_info *info)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);
    status_t ret = NO_ERROR;

    ALOGV("DEBUG(%s):", __FUNCTION__);
    if (cameraId < 0 || cameraId >= HAL_getNumberOfCameras()) {
        ALOGE("ERR(%s):Invalid camera ID %d", __FUNCTION__, cameraId);
        return -EINVAL;
    }

    info->facing = sCameraInfo[cameraId][0];
    info->orientation = sCameraInfo[cameraId][1];
    info->device_version = HARDWARE_DEVICE_API_VERSION(1, 0);

    if (g_cam_info[cameraId] == NULL) {
        ALOGD("DEBUG(%s[%d]):Return static information (%d)", __FUNCTION__, __LINE__, cameraId);
        ret = ExynosCamera1MetadataConverter::constructStaticInfo(cameraId, &g_cam_info[cameraId]);
        if (ret != 0) {
            ALOGE("ERR(%s[%d]): static information is NULL", __FUNCTION__, __LINE__);
            return -EINVAL;
        }
        info->static_camera_characteristics = g_cam_info[cameraId];
    } else {
        ALOGD("DEBUG(%s[%d]):Reuse!! Return static information (%d)", __FUNCTION__, __LINE__, cameraId);
        info->static_camera_characteristics = g_cam_info[cameraId];
    }

    /* set service arbitration (resource_cost, conflicting_devices, conflicting_devices_length */
    info->resource_cost = sCameraConfigInfo[cameraId].resource_cost;
    info->conflicting_devices = sCameraConfigInfo[cameraId].conflicting_devices;
    info->conflicting_devices_length = sCameraConfigInfo[cameraId].conflicting_devices_length;
    ALOGD("INFO(%s info->resource_cost = %d ", __FUNCTION__, info->resource_cost);
    if (info->conflicting_devices_length) {
        for (size_t i = 0; i < info->conflicting_devices_length; i++) {
            ALOGD("INFO(%s info->conflicting_devices = %s ", __FUNCTION__, info->conflicting_devices[i]);
        }
    } else {
        ALOGD("INFO(%s info->conflicting_devices_length is zero ", __FUNCTION__);
    }

    return NO_ERROR;
}

static int HAL_set_torch_mode(const char* camera_id, bool enabled)
{
    ExynosCameraAutoTimer autoTimer(__FUNCTION__);

    int cameraId = atoi(camera_id);
    FILE *fp = NULL;
    char flashFilePath[100] = {'\0',};
    CameraMetadata metadata;
    camera_metadata_entry flashAvailable;
    int ret = 0;

    ALOGI("INFO(%s[%d]):in =====", __FUNCTION__, __LINE__);
    if (cameraId < 0 || cameraId >= HAL_getNumberOfCameras()) {
        ALOGE("ERR(%s[%d]):Invalid camera ID %d", __FUNCTION__, __LINE__, cameraId);
        return -EINVAL;
    }

    /* Check the android.flash.info.available */
    /* If this camera device does not support flash, It have to return -ENOSYS */
    metadata = g_cam_info[cameraId];
    flashAvailable = metadata.find(ANDROID_FLASH_INFO_AVAILABLE);

    if (flashAvailable.count == 1 && flashAvailable.data.u8[0] == 1) {
        ALOGV("DEBUG(%s[%d]): Flash metadata exist", __FUNCTION__, __LINE__);
    } else {
        ALOGE("ERR(%s[%d]): Can not find flash metadata", __FUNCTION__, __LINE__);
        return -ENOSYS;
    }

    ALOGI("INFO(%s[%d]): Current Camera State (state = %d)", __FUNCTION__, __LINE__, cam_state[cameraId]);

    /* Add the check the camera state that camera in use or not */
    if (cam_state[cameraId] > CAMERA_CLOSED) {
        ALOGE("ERR(%s[%d]): Camera Device is busy (state = %d)", __FUNCTION__, __LINE__, cam_state[cameraId]);
        if (g_callbacks) {
            g_callbacks->torch_mode_status_change(g_callbacks, camera_id, TORCH_MODE_STATUS_AVAILABLE_OFF);
            ALOGI("INFO(%s[%d]):camera(%d) TORCH_MODE_STATUS_AVAILABLE_OFF", __FUNCTION__, __LINE__, cameraId);
        }
        return -EBUSY;
    }

    /* Add the sysfs file read (sys/class/camera/flash/torch_flash) then set 0 or 1 */
    if (cameraId == CAMERA_ID_BACK) {
        snprintf(flashFilePath, sizeof(flashFilePath), TORCH_REAR_FILE_PATH);
    } else {
        snprintf(flashFilePath, sizeof(flashFilePath), TORCH_FRONT_FILE_PATH);
    }

    fp = fopen(flashFilePath, "w+");

    if (fp == NULL) {
        ALOGE("ERR(%s[%d]):torch file open(%s) fail",
            __FUNCTION__, __LINE__, flashFilePath);
        return -ENOSYS;
    }

    if (enabled) {
        fwrite("1", sizeof(char), 1, fp);
    } else {
        fwrite("0", sizeof(char), 1, fp);
    }

    fflush(fp);

    ret = fclose(fp);
    if (ret != 0) {
        ALOGE("ERR(%s[%d]): file close failed(%d)", __FUNCTION__, __LINE__, ret);
    }

    if (enabled) {
        g_cam_torchEnabled[cameraId] = true;
        if (g_callbacks) {
            g_callbacks->torch_mode_status_change(g_callbacks, camera_id, TORCH_MODE_STATUS_AVAILABLE_ON);
            ALOGI("INFO(%s[%d]):camera(%d) TORCH_MODE_STATUS_AVAILABLE_ON", __FUNCTION__, __LINE__, cameraId);
        }
    } else {
        g_cam_torchEnabled[cameraId] = false;
        if (g_callbacks) {
            g_callbacks->torch_mode_status_change(g_callbacks, camera_id, TORCH_MODE_STATUS_AVAILABLE_OFF);
            ALOGI("INFO(%s[%d]):camera(%d) TORCH_MODE_STATUS_AVAILABLE_OFF", __FUNCTION__, __LINE__, cameraId);
        }
    }

    ALOGI("INFO(%s[%d]):out =====", __FUNCTION__, __LINE__);

    return NO_ERROR;
}

static int HAL_init()
{
    ALOGI("INFO(%s[%d]):in =====", __FUNCTION__, __LINE__);

    return OK;
}

static int HAL_open_legacy(__unused const struct hw_module_t* module, __unused const char* id,
                           __unused uint32_t halVersion, __unused struct hw_device_t** device)
{
    ALOGV("DEBUG(%s):", __FUNCTION__);
    return NO_ERROR;
}

static void HAL_get_vendor_tag_ops(__unused vendor_tag_ops_t* ops)
{
    ALOGV("INFO(%s):", __FUNCTION__);
/*
    SecCameraVendorTags::Ops = ops;

    ops->get_all_tags = SecCameraVendorTags::get_ext_all_tags;
    ops->get_tag_count = SecCameraVendorTags::get_ext_tag_count;
    ops->get_tag_type = SecCameraVendorTags::get_ext_tag_type;
    ops->get_tag_name = SecCameraVendorTags::get_ext_tag_name;
    ops->get_section_name = SecCameraVendorTags::get_ext_section_name;
    ops->reserved[0] = NULL;
*/
}

}; /* namespace android */
