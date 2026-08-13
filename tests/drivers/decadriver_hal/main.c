/*
 * SPDX-FileCopyrightText: 2022 HAW Hamburg
 * SPDX-License-Identifier: LGPL-2.1-only
 */

/**
 * @ingroup     tests
 * @{
 *
 * @file
 * @brief       Test application for dw3xxx IEEE 802.15.4 device driver
 *
 * @author      Leandro Lanzieri <leandro.lanzieri@haw-hamburg.de>
 *
 * @}
 */

#include <stdio.h>

#include "decadriver_802154_hal.h"
#include "init_dev.h"
#include "net/netdev/ieee802154_submac.h"
#include "shell.h"
#include "test_utils/netdev_ieee802154_minimal.h"

static netdev_ieee802154_submac_t dw3xxx;

int netdev_ieee802154_minimal_init_devs(netdev_event_cb_t cb) {
    netdev_t *netdev = &dw3xxx.dev.netdev;

    puts("Initializing DW3xxx device");

    netdev_register(netdev, NETDEV_DECADRIVER, 0);
    netdev_ieee802154_submac_init(&dw3xxx);

    /* set the application-provided callback */
    netdev->event_callback = cb;

    /* setup and initialize the specific driver */
    dw3000_ieee802154_hal_setup(&dw3xxx.submac.dev);
    dw3000_ieee802154_init();

    /* initialize the device driver */
    int res = netdev->driver->init(netdev);
    if (res != 0) {
        return -1;
    }

    return 0;
}

int main(void)
{
    puts("Test application for dw3xxx IEEE 802.15.4 device driver");

    int res = netdev_ieee802154_minimal_init();
    if (res) {
        puts("Error initializing devices");
        return 1;
    }

    /* start the shell */
    puts("Initialization successful - starting the shell now");

    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(NULL, line_buf, SHELL_DEFAULT_BUFSIZE);

    return 0;
}
