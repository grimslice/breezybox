# BreezyBox demo for the Tanmatsu cyberdeck (ESP32-P4)
cmake_minimum_required(VERSION 3.16)

# Local (unpublished) BreezyBox components used by this firmware.
set(EXTRA_COMPONENT_DIRS "../../src/components/breezy_sound")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(breezy_tanmatsu)
