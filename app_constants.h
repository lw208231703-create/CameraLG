#pragma once

// Main window layout constants
static constexpr int LEFT_DOCK_MIN_WIDTH = 330;
static constexpr int DEVICE_DOCK_MIN_HEIGHT = 50;
static constexpr int IMAGE_DATA_DOCK_MIN_HEIGHT = 200;
static constexpr int IMAGE_ALGORITHM_DOCK_MIN_HEIGHT = 0;

// Display constants (used by display_dock.h)
static constexpr int DISPLAY_BUTTON_SIZE_X = 26;
static constexpr int DISPLAY_BUTTON_SIZE_Y = 26;
static constexpr int DISPLAY_BG_FILL_X = 150;
static constexpr int DISPLAY_BG_FILL_Y = 150;
static constexpr int DISPLAY_BG_ROTATION_ANGLE = 45;
static constexpr double DISPLAY_BG_TRANSPARENCY = 0.4;

// Default camera parameters
static constexpr int DEFAULT_BIT_SHIFT = 6;
static constexpr int DEFAULT_BUFFER_COUNT = 10;
static constexpr int DEFAULT_VIRTUAL_WIDTH = 640;
static constexpr int DEFAULT_VIRTUAL_HEIGHT = 512;
