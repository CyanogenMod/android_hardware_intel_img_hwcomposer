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
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/queue.h>
#include <linux/netlink.h>
#include <sys/types.h>
#include <unistd.h>
#include <HwcTrace.h>
#include <DrmConfig.h>
#include <common/HotplugControl.h>

namespace android {
namespace intel {

HotplugControl::HotplugControl()
    : IHotplugControl(),
      mUeventFd(-1)
{
    CTRACE();
}

HotplugControl::~HotplugControl()
{
    if (mUeventFd != -1) {
        ETRACE("object is not deinitialized");
    }
}

bool HotplugControl::initialize()
{
    if (mUeventFd != -1) {
        WTRACE("object has been initialized");
        return true;
    }

    // init uevent socket
    struct sockaddr_nl addr;
    // set the socket receive buffer to 64K
    // NOTE: this is only called for once
    int sz = 64 * 1024;

    memset(&addr, 0, sizeof(addr));
    addr.nl_family = AF_NETLINK;
    addr.nl_pid =  pthread_self() | getpid();
    addr.nl_groups = 0xffffffff;

    mUeventFd = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
    if (mUeventFd < 0) {
        DTRACE("failed to create uevent socket");
        return false;
    }

    setsockopt(mUeventFd, SOL_SOCKET, SO_RCVBUFFORCE, &sz, sizeof(sz));

    if (bind(mUeventFd, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        ETRACE("failed to bind scoket");
        deinitialize();
        return false;
    }

    memset(mUeventMessage, 0, UEVENT_MSG_LEN);
    return true;
}

void HotplugControl::deinitialize()
{
    if (mUeventFd != -1) {
        close(mUeventFd);
        mUeventFd = -1;
    }
}

bool HotplugControl::waitForEvent()
{
    if (mUeventFd == -1) {
        ETRACE("invalid uEvent file descriptor");
        return false;
    }

    struct pollfd fds;
    int nr;

    fds.fd = mUeventFd;
    fds.events = POLLIN;
    fds.revents = 0;
    nr = poll(&fds, 1, -1);

    if (nr > 0 && fds.revents == POLLIN) {
        int count = recv(mUeventFd, mUeventMessage, UEVENT_MSG_LEN - 2, 0);
        if (count > 0)
            return isHotplugEvent(mUeventMessage, UEVENT_MSG_LEN - 2);
    } else {
        ITRACE("exiting wait");
    }

    return false;
}

bool HotplugControl::isHotplugEvent(const char *msg, int msgLen)
{
    if (strcmp(msg, DrmConfig::getHotplugEnvelope()) != 0)
        return false;

    msg += strlen(msg) + 1;
    const char* hotplugString = DrmConfig::getHotplugString();

    do {
        if (strncmp(msg, hotplugString, strlen(hotplugString)) == 0) {
            return true;
        }
        msg += strlen(msg) + 1;
    } while (*msg);

    return false;
}

} // namespace intel
} // namespace android
