/*
 * Copyright (C) 2016 Intel Corporation
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

#include <media/AudioSystem.h>
#include <utils/String8.h>
#include <iostream>
#include <cutils/sockets.h>
#include <sys/socket.h>
#include <utils/Log.h>
#include <sys/stat.h>

static const uint8_t RECOVER = 8;
static const uint8_t CRASH = 9;
static const char *const uevent_socket_name = "uevent_emulation";

int main(int argc, char *argv[])
{
    if (argc != 2) {
        std::cerr << "usage: " << argv[0] << " <uevent message>" << std::endl;
        std::cerr << "  uevent message:" << std::endl;
        std::cerr << "    RECOVER or CRASH" << std::endl;
        return 1;
    }
    int socketFd = socket_local_client(uevent_socket_name,
                                       ANDROID_SOCKET_NAMESPACE_ABSTRACT,
                                       SOCK_STREAM);
    if (socketFd == -1) {
        std::cerr << "socket_local_client connection failed" << std::endl;
        return 0;
    }
    if (strcmp(argv[1], "RECOVER") && strcmp(argv[1], "CRASH")) {
        std::cerr << "send only RECOVER or CRASH" << std::endl;
        close(socketFd);
        return 0;
    }
    const uint8_t msg = !strcmp(argv[1], "RECOVER") ? RECOVER : CRASH;
    uint32_t offset = 0;
    const uint8_t *pData = &msg;
    uint32_t size = sizeof(msg);
    while (size) {
        int32_t accessedSize = ::send(socketFd, &pData[offset], size, MSG_NOSIGNAL);
        if (!accessedSize || accessedSize == -1) {
            return false;
        }
        size -= accessedSize;
        offset += accessedSize;
    }
    close(socketFd);

    return 0;
}
