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
    bool isVideoExtModeActive();
    bool isVideoExtModeEnabled();
    bool isVideoLayer(hwc_layer_1_t &layer);
    bool isVideoFullScreen(int device, hwc_layer_1_t &layer);
    bool isOverlayAllowed();
    int  getVideoInstances();
    void postHotplugEvent(bool connected);
    void postVideoEvent(int instanceID, int state);
    void postInputEvent(bool active);
    void postVideoEvent(int instances, int instanceID, bool preparing, bool playing);
    void postBlankEvent(bool blank);
    void postIdleEntryEvent();
    bool isPresentationLayer(hwc_layer_1_t &layer);
    bool isProtectedLayer(hwc_layer_1_t &layer);

private:
    enum DisplayEventType {
        HOTPLUG_EVENT,
        BLANK_EVENT,
        VIDEO_EVENT,
        TIMING_EVENT,
        INPUT_EVENT,
        DPMS_EVENT,
        IDLE_ENTRY_EVENT,
        IDLE_EXIT_EVENT,
    };

    struct Event {
        int type;

        struct VideoEvent {
            int instanceID;
            int state;
        };

        union {
            bool bValue;
            int  nValue;
            VideoEvent videoEvent;
        };
    };
    inline void postEvent(Event& e);
    inline bool getEvent(Event& e);
    void handlePendingEvents();
    void handleHotplugEvent(bool connected);
    void handleBlankEvent(bool blank);
    void handleVideoEvent(int instanceID, int state);
    void handleTimingEvent();
    void handleInputEvent(bool active);
    void handleDpmsEvent(int delayCount);
    void handleIdleEntryEvent(int count);
    void handleIdleExitEvent();

    void blankSecondaryDevice();
    void handleVideoExtMode();
    void checkVideoExtMode();
    void enterVideoExtMode();
    void exitVideoExtMode();
    bool hasProtectedLayer();
    inline void setCompositionType(hwc_display_contents_1_t *content, int type);
    inline void setCompositionType(int device, int type, bool reset);

private:
    // Video playback state, must match defintion in Multi Display Service
    enum
    {
        VIDEO_PLAYBACK_IDLE,
        VIDEO_PLAYBACK_STARTING,
        VIDEO_PLAYBACK_STARTED,
        VIDEO_PLAYBACK_STOPPING,
        VIDEO_PLAYBACK_STOPPED,
    };

    enum
    {
        // number of idle flips before display can enter idle mode
        // minimum delay is 1
        DELAY_BEFORE_IDLE_ENTRY = 2,

        // number of flips before display can be powered off in video extended mode
        DELAY_BEFORE_DPMS_OFF = 10,
    };

private:
    bool mInitialized;
    bool mVideoExtModeEnabled;
    bool mVideoExtModeEligible;
    bool mVideoExtModeActive;
    bool mBlankDevice;
    bool mOverlayAllowed;
    bool mActiveInputState;
    // map video instance ID to video state
    KeyedVector<int, int> mVideoStateMap;
    int mCachedNumDisplays;
    hwc_display_contents_1_t** mCachedDisplays;
    Vector<Event> mPendingEvents;
    Mutex mEventMutex;
    Condition mEventHandledCondition;
};

} // namespace intel
} // namespace android



#endif /* DISPLAY_ANALYZER_H */
