#ifndef ERASER_H
#define ERASER_H

// Watches the pen digitizer for the eraser end of the pen. xochitl hands the
// pen to AppLoad apps as plain mouse events, which can't tell the two apart.
class EraserWatcher
{
public:
    ~EraserWatcher();

    // Finds the input device reporting BTN_TOOL_RUBBER, returns false if there's none
    bool open();
    int fd() const { return m_fd; }

    // Reads pending events, returns true if the eraser state changed
    bool readEvents();
    bool isErasing() const { return m_erasing; }

private:
    int m_fd = -1;
    bool m_erasing = false;
};

#endif // ERASER_H
