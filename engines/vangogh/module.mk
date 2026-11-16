MODULE := engines/vangogh

MODULE_OBJS = \
	vangogh.o \
	console.o \
	lzss.o \
	tgp.o \
	spr.o \
	bfg.o \
	box3di.o \
	model3d.o \
	anim3da.o \
	gamestate.o \
	movietable.o \
	cvy.o \
	clip.o \
	scenelogic.o \
	renderer3d.o \
	world.o \
	sound.o \
	hnmplayer.o \
	metaengine.o

# This module can be built as a plugin
ifeq ($(ENABLE_VANGOGH), DYNAMIC_PLUGIN)
PLUGIN := 1
endif

# Include common rules
include $(srcdir)/rules.mk

# Detection objects
DETECT_OBJS += $(MODULE)/detection.o
