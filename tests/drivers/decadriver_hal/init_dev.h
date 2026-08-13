/*
 * SPDX-FileCopyrightText: 2022 HAW Hamburg
 * SPDX-License-Identifier: LGPL-2.1-only
 */

#pragma once

/**
 * @ingroup     tests
 * @{
 *
 * @file
 * @brief   Device-specific test header file DW3xxx IEEE 802.15.4 device driver
 *
 * @author  Leandro Lanzieri <leandro.lanzieri@haw-hamburg.de>
 */

#ifdef __cplusplus
extern "C" {
#endif

/* The ported decadriver only allows one device */
#define NETDEV_IEEE802154_MINIMAL_NUMOF 1

#ifdef __cplusplus
}
#endif

/** @} */
