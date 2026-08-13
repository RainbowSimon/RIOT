INCLUDES += -I$(APPDIR)
BOARD ?= nrf52840dk

include ../Makefile.drivers_common

USEMODULE += test_utils_netdev_ieee802154_minimal

# select the driver to test
USEPKG += decadriver
USEMODULE += decadriver_ieee802154_hal
USEMODULE += netdev_ieee802154_submac


CFLAGS += -DEVENT_THREAD_STACKSIZE_DEFAULT=2048
CFLAGS += -DTHREAD_STACKSIZE_DEFAULT=4096
CFLAGS += -DISR_STACKSIZE=2048

USEMODULE += shell_cmd_ps

include $(RIOTBASE)/Makefile.include
