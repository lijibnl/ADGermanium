TOP = .
include $(TOP)/configure/CONFIG
DIRS += configure
DIRS += $(wildcard *App)
DIRS += $(wildcard iocBoot)
iocBoot_DEPEND_DIRS += $(filter %App,$(DIRS))
include $(TOP)/configure/RULES_TOP
