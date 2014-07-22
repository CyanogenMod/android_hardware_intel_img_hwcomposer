/*
 * Copyright © 2012 Intel Corporation
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors:
 *    Jackie Li <yaodong.li@intel.com>
 *
 */
#ifndef DISPLAY_ANALYZER_H
#define DISPLAY_ANALYZER_H

#include <utils/threads.h>
#include <utils/Vector.h>


namespace android {
namespace intel {


class DisplayAnalyzer {
public:
    DisplayAnalyzer();
    virtual ~DisplayAnalyzer();

public:
    bool initialize();
    void deinitialize();
    void analyzeContents(size_t numDisplays, hwc_display_contents_1_t** displays);
    bool checkVideoExtendedMode();
    bool isVideoExtendedModeEnabled();
    bool isVideoLayer(hwc_layer_1_t &layer);
    bool isVideoFullScreen(int device, hwc_layer_1_t &layer);
    bool isVideoPlaying();
    bool isOverlayAllowed();
    void postHotplugEvent(bool connected);
    void postVideoEvent(int instances, int instanceID, bool preparing, bool playing);
    void postBlankEvent(bool blank);
    bool isPresentationLayer(hwc_layer_1_t &layer);
    bool isProtectedLayer(hwc_layer_1_t &layer);

private:
    enum DisplayEventType {
        HOTPLUG_EVENT,
        BLANK_EVENT,
        VIDEO_EVENT,
    };

    struct Event {
        int type;

        struct VideoEvent {
            int instances;
            int instanceID;
            bool preparing;
            bool playing;
        };

        union {
            bool connected;
            bool blank;
            VideoEvent videoEvent;
        };
    };
    inline void postEvent(Event& e);
    void handlePendingEvents();
    void handleHotplugEvent(bool connected);
    void handleBlankEvent(bool blank);
    void handleVideoEvent(int instances, int instanceID, bool preparing, bool playing);

    void blankSecondaryDevice();
    void detectVideoExtendedMode();
    void detectTrickMode(hwc_display_contents_1_t *list);
    void handleModeSwitch();
    inline void resetCompositionType(hwc_display_contents_1_t *content);

private:
    bool mInitialized;
    bool mEnableVideoExtendedMode;
    bool mVideoExtendedMode;
    bool mForceCloneMode;
    bool mBlankDevice;
    bool mVideoPlaying;
    bool mVideoPreparing;
    bool mVideoStateChanged;
    bool mOverlayAllowed;
    int mVideoInstances;
    int mVideoInstanceId;
    int mCachedNumDisplays;
    hwc_display_contents_1_t** mCachedDisplays;
    Vector<Event> mPendingEvents;
    Mutex mEventMutex;
    Condition mEventHandledCondition;
};

} // namespace intel
} // namespace android



#endif /* DISPLAY_ANALYZER_H */
