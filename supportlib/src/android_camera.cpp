#include "android_camera.h"
#include "log.h"
#include <camera/NdkCameraDevice.h>
#include <camera/NdkCameraManager.h>
#include <media/NdkImage.h>
#include <string>

void onDisconnected(void* context, ACameraDevice* device) {
    LOGE("onDisconnected");
}

void onError(void* context, ACameraDevice* device, int error) {
    LOGE("onError");
}

void onSessionActive(void* context, ACameraCaptureSession* session) {
}

void onSessionReady(void* context, ACameraCaptureSession* session) {
}

void onSessionClosed(void* context, ACameraCaptureSession* session) {
    LOGE("onSessionClosed");
}

void onCaptureFailed(void* context, ACameraCaptureSession* session, ACaptureRequest* request, ACameraCaptureFailure* failure) {
    LOGE("onCaptureFailed");
}

void onCaptureSequenceCompleted(void* context, ACameraCaptureSession* session, int sequenceId, int64_t frameNumber) {
}

void onCaptureSequenceAborted(void* context, ACameraCaptureSession* session, int sequenceId) {
    LOGE("onCaptureSequenceAborted");
}

void onCaptureCompleted(void* context, ACameraCaptureSession* session, ACaptureRequest* request, const ACameraMetadata* result) {
}

ACameraDevice* docscanner::find_and_open_back_camera(uint32_t &width, uint32_t &height) {
    ACameraManager* mng = ACameraManager_create();

    ACameraIdList* camera_ids = nullptr;
    ACameraManager_getCameraIdList(mng, &camera_ids);
    size_t camera_index = -1;

    for (size_t c = 0; c < camera_ids->numCameras; c++) {
        const char* id = camera_ids->cameraIds[c];

        ACameraMetadata* metadata = nullptr;
        ACameraManager_getCameraCharacteristics(mng, id, &metadata);

        int32_t tags_size = 0;
        const uint32_t* tags = nullptr;
        ACameraMetadata_getAllTags(metadata, &tags_size, &tags);

        for (int32_t t = 0; t < tags_size; t++) {
            if (tags[t] == ACAMERA_LENS_FACING) {
                ACameraMetadata_const_entry entry = {};
                ACameraMetadata_getConstEntry(metadata, tags[c], &entry);

                auto facing = (acamera_metadata_enum_android_lens_facing_t) (entry.data.u8[0]);
                if (facing != ACAMERA_LENS_FACING_FRONT) continue;

                ACameraMetadata_getConstEntry(metadata, ACAMERA_SCALER_AVAILABLE_STREAM_CONFIGURATIONS, &entry);

                size_t max_index = 0;
                int32_t max_resolution = 0;
                for (size_t e = 0; e < entry.count; e += 4) {
                    if (entry.data.i32[e + 3]) continue; // skip input streams
                    if (entry.data.i32[e + 0] != AIMAGE_FORMAT_YUV_420_888) continue; // skip wrong input formats
                    // todo: support raw photography for increased quality!

                    int32_t resolution = entry.data.i32[e + 1] * entry.data.i32[e + 2];
                    if (resolution > max_resolution) {
                        max_index = e;
                        max_resolution = resolution;
                        width = (uint32_t) entry.data.i32[e + 1];
                        height = (uint32_t) entry.data.i32[e + 2];
                    }
                }

                LOGI("Index %zu, Resolution of %dMP", max_index, max_resolution / (1000000));

                camera_index = c;
                break;
            }
        }

        ACameraMetadata_free(metadata);

        if (camera_index != -1) break;
    }

    if (camera_index == -1) {
        return nullptr;
    }

    ACameraDevice* device;

    ACameraDevice_StateCallbacks device_callbacks = {
            .context = nullptr,
            .onDisconnected = onDisconnected,
            .onError = onError
    };

    std::string id = camera_ids->cameraIds[camera_index]; // todo: cleanup
    ACameraManager_openCamera(mng, id.c_str(), &device_callbacks, &device);

    ACameraManager_deleteCameraIdList(camera_ids);

    return device;
}

void docscanner::init_camera_capture_to_native_window(ACameraDevice* cam, ANativeWindow* texture_window) {
    // prepare request with desired template
    ACaptureRequest* request = nullptr;
    ACameraDevice_createCaptureRequest(cam, TEMPLATE_STILL_CAPTURE, &request);

    // prepare temp_compute_output surface
    ANativeWindow_acquire(texture_window);

    // finalize capture request
    ACameraOutputTarget* texture_target = nullptr;
    ACameraOutputTarget_create(texture_window, &texture_target);
    ACaptureRequest_addTarget(request, texture_target);

    // prepare capture session output...
    ACaptureSessionOutput* texture_output = nullptr;
    ACaptureSessionOutput_create(texture_window, &texture_output);

    // ...and container
    ACaptureSessionOutputContainer* outputs = nullptr;
    ACaptureSessionOutputContainer_create(&outputs);
    ACaptureSessionOutputContainer_add(outputs, texture_output);

    ACameraCaptureSession_stateCallbacks state_callbacks = {
        .context = nullptr,
        .onClosed = onSessionClosed,
        .onReady = onSessionReady,
        .onActive = onSessionActive
    };

    ACameraCaptureSession_captureCallbacks capture_callbacks = {
            .context = nullptr,
            .onCaptureStarted = nullptr,
            .onCaptureProgressed = nullptr,
            .onCaptureCompleted = onCaptureCompleted,
            .onCaptureFailed = onCaptureFailed,
            .onCaptureSequenceCompleted = onCaptureSequenceCompleted,
            .onCaptureSequenceAborted = onCaptureSequenceAborted,
            .onCaptureBufferLost = nullptr,
    };

    // prepare capture session
    ACameraCaptureSession* session = nullptr;
    ACameraDevice_createCaptureSession(cam, outputs, &state_callbacks, &session);

    // start capturing continuously
    ACameraCaptureSession_setRepeatingRequest(session, &capture_callbacks, 1, &request, nullptr);
}