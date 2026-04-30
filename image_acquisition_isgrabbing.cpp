#include "image_acquisition.h"

#ifdef ENABLE_SAPERA_CAMERA

bool Image_Acquisition::isGrabbing() const
{
    if (m_Xfer) {
        return m_Xfer->IsGrabbing();
    }
    return false;
}

#endif // ENABLE_SAPERA_CAMERA
