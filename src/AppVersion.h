#pragma once
// Single source of truth for app version.
// Only main.cpp should implement appVersion(). Other files must call this.
const char* appVersion();
