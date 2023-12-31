TEMPLATE = app
CONFIG  -= qt
CONFIG  += console


TARGET = spi_flash_quad_test_mxic

DEFINES += FT4222_STATIC


#####################
debug:OBJECTS_DIR   = ./tmp/debug_obj
release:OBJECTS_DIR = ./tmp/release_obj
MOC_DIR     = ./tmp
UI_DIR      = ./tmp


#####################
# ftd2xx

INCLUDEPATH  += ../../../imports/ftd2xx
QMAKE_LIBDIR += ../../../imports/ftd2xx/i386
LIBS += -lftd2xx


#####################
# LibFT4222

INCLUDEPATH  += ../../../imports/LibFT4222/inc
QMAKE_LIBDIR += ../../imports/LibFT4222/lib/i386
debug:LIBS += -lLibFT4222


#####################

SOURCES += spi_flash_quad_test_mxic.cpp
