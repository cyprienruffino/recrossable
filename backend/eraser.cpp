#include "eraser.h"

#include <cerrno>
#include <climits>
#include <cstdio>
#include <iostream>
#include <string>

#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>

static bool hasKey(int fd, int key)
{
    unsigned long bits[(KEY_MAX + 1) / (sizeof(long) * CHAR_BIT) + 1] = {};
    if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof bits), bits) < 0) {
        return false;
    }
    return bits[key / (sizeof(long) * CHAR_BIT)] & (1UL << (key % (sizeof(long) * CHAR_BIT)));
}

EraserWatcher::~EraserWatcher()
{
    if (m_fd != -1) {
        close(m_fd);
    }
}

bool EraserWatcher::open()
{
    for (int i = 0; i < 32; i++) {
        const std::string path = "/dev/input/event" + std::to_string(i);
        const int fd = ::open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd == -1) {
            continue;
        }
        if (!hasKey(fd, BTN_TOOL_RUBBER)) {
            close(fd);
            continue;
        }

        char name[256] = "unknown";
        ioctl(fd, EVIOCGNAME(sizeof name), name);
        std::cerr << "Watching " << path << " (" << name << ") for the eraser" << std::endl;

        // The eraser may already be near the screen
        unsigned long keys[(KEY_MAX + 1) / (sizeof(long) * CHAR_BIT) + 1] = {};
        if (ioctl(fd, EVIOCGKEY(sizeof keys), keys) >= 0) {
            m_erasing = keys[BTN_TOOL_RUBBER / (sizeof(long) * CHAR_BIT)] & (1UL << (BTN_TOOL_RUBBER % (sizeof(long) * CHAR_BIT)));
        }
        m_fd = fd;
        return true;
    }
    std::cerr << "No input device reports an eraser" << std::endl;
    return false;
}

bool EraserWatcher::readEvents()
{
    const bool wasErasing = m_erasing;
    input_event events[64];
    while (true) {
        const ssize_t n = read(m_fd, events, sizeof events);
        if (n <= 0) {
            if (n < 0 && errno != EAGAIN) {
                perror("read eraser events");
                close(m_fd);
                m_fd = -1;
            }
            break;
        }
        for (size_t i = 0; i < n / sizeof(input_event); i++) {
            const input_event &event = events[i];
            if (event.type == EV_KEY && event.code == BTN_TOOL_RUBBER) {
                m_erasing = event.value != 0;
            } else if (event.type == EV_KEY && event.code == BTN_TOOL_PEN && event.value) {
                m_erasing = false;
            }
        }
    }
    return m_erasing != wasErasing;
}
