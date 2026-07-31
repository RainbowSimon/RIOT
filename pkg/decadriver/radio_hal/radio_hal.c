#include <assert.h>
#include <string.h>
#include <errno.h>

#include "net/ieee802154.h"
#include "net/ieee802154/radio.h"

#define ENABLE_DEBUG        0
#include "debug.h"

#define DEBUG_PREFIX        "[dw3000 802.15.4 HAL]"

//static uint8_t rxbuf[IEEE802154_FRAME_LEN_MAX + 3]; /* len PHR + PSDU + LQI */
//static uint8_t txbuf[IEEE802154_FRAME_LEN_MAX + 3]; /* len PHR + PSDU + LQI */

//typedef enum {
//    STATE_IDLE,
//    STATE_TX,
//    STATE_RX,
//    STATE_CCA_CLEAR,
//    STATE_CCA_BUSY,
//} nrf802154_state_t;

//static volatile uint8_t _state;

//static uint8_t nrf802154_short_addr[IEEE802154_SHORT_ADDRESS_LEN];
//static uint8_t nrf802154_long_addr[IEEE802154_LONG_ADDRESS_LEN];
//static uint16_t nrf802154_pan_id;

//static struct {
//    bool ifs        : 1;    /**< if true, the device is currently inside the IFS period */
//    bool cca_send   : 1;    /**< whether the next transmission uses CCA or not */
//    bool ack_filter : 1;    /**< whether the ACK filter is activated or not */
//    bool promisc    : 1;    /**< whether the device is in promiscuous mode or not */
//    bool pending    : 1;    /**< whether there pending bit should be set in the ACK frame or not */
//} cfg = {
//    .cca_send   = true,
//    .ack_filter = true,
//};

static const ieee802154_radio_ops_t dw3000_ieee802254_ops;
static ieee802154_dev_t *dw3000_ieee802254_hal_dev;


//static bool _l2filter(uint8_t *mhr)
//{
//    uint8_t dst_addr[IEEE802154_LONG_ADDRESS_LEN];
//    uint8_t src_addr[IEEE802154_LONG_ADDRESS_LEN];
//    le_uint16_t dst_pan;
//    le_uint16_t src_pan;
//    uint8_t pan_bcast[] = IEEE802154_PANID_BCAST;

//    int dst_addr_len = ieee802154_get_dst(mhr, dst_addr, &dst_pan);

//    int src_addr_len = ieee802154_get_src(mhr, src_addr, &src_pan);

//    if ((mhr[0] & IEEE802154_FCF_TYPE_MASK) == IEEE802154_FCF_TYPE_BEACON) {
//        if (src_addr_len == IEEE802154_SHORT_ADDRESS_LEN ||
//            src_addr_len == IEEE802154_LONG_ADDRESS_LEN){
//            if ((memcmp(&nrf802154_pan_id, src_pan.u8, 2) == 0) ||
//                (memcmp(&nrf802154_pan_id, pan_bcast, 2) == 0)) {
//                return true;
//            }
//        }
//    }
//    /* filter PAN ID */
//    /* Will only work on little endian platform (all?) */

//    if ((memcmp(pan_bcast, dst_pan.u8, 2) != 0) &&
//        (memcmp(&nrf802154_pan_id, dst_pan.u8, 2) != 0)) {
//        return false;
//    }

//    /* check destination address */
//    if (((dst_addr_len == IEEE802154_SHORT_ADDRESS_LEN) &&
//          (memcmp(nrf802154_short_addr, dst_addr, dst_addr_len) == 0 ||
//           memcmp(ieee802154_addr_bcast, dst_addr, dst_addr_len) == 0)) ||
//        ((dst_addr_len == IEEE802154_LONG_ADDRESS_LEN) &&
//          (memcmp(nrf802154_long_addr, dst_addr, dst_addr_len) == 0))) {
//        return true;
//    }

//    return false;
//}

static int _write(ieee802154_dev_t *dev, const iolist_t *iolist)
{
    (void)dev;
    size_t len = 0;
    int32_t res;

    for (; iolist; iolist = iolist->iol_next) {
        /* Check if there is data to copy, prevents undefined behaviour with
         * memcpy when iolist->iol_base == NULL */
        if (iolist->iol_len) {
            res = dwt_writetxdata(iolist->iol_len, iolist->iol_base, len);
            
            if (res != DWT_SUCCESS) {
                DEBUG(DEBUG_PREFIX" write(): failed");
                return -ENOBUFS;
            }
            
            len += iolist->iol_len;
        }
    }

    dwt_writetxfctrl(len + IEEE802154_FCS_LEN, 0, 0);

    DEBUG(DEBUG_PREFIX" write(): put %i bytes into frame buffer\n", len);

    // TODO mrf24j40 saves the header, maybe we need this too?

    return 0;
}

static int _request_op(ieee802154_dev_t *dev, ieee802154_hal_op_t op, void *ctx)
{
    (void)dev;
    (void)op;
    (void)ctx;

    DEBUG(DEBUG_PREFIX" Request operation\n");

    return 0;

//    int res = -EBUSY;
//    int state = STATE_IDLE;

//    switch (op) {
//    case IEEE802154_HAL_OP_TRANSMIT:
//        if (cfg.ifs) {
//            goto end;
//        }
//        NRF_RADIO->SHORTS = cfg.cca_send ? CCA_SHORTS : DEFAULT_SHORTS;
//        NRF_RADIO->TASKS_TXEN = 1;
//        state = STATE_TX;
//        break;
//    case IEEE802154_HAL_OP_SET_RX:
//        if (_state != STATE_IDLE && _state != STATE_RX) {
//            goto end;
//        }
//        _disable_blocking();
//        state = STATE_RX;
//        NRF_RADIO->PACKETPTR = (uint32_t) rxbuf;
//        NRF_RADIO->SHORTS = DEFAULT_SHORTS;
//        NRF_RADIO->TASKS_RXEN = 1;
//        break;
//    case IEEE802154_HAL_OP_SET_IDLE: {
//        assert(ctx);
//        bool force = *((bool*) ctx);
//        if (!force && _state != STATE_IDLE && _state != STATE_RX) {
//            goto end;
//        }
//        _disable_blocking();
//        NRF_RADIO->SHORTS = DEFAULT_SHORTS;
//        NRF_RADIO->PACKETPTR = (uint32_t) txbuf;
//        state = STATE_IDLE;
//        break;
//    }
//    case IEEE802154_HAL_OP_CCA:
//        _disable_blocking();
//        NRF_RADIO->SHORTS = RADIO_SHORTS_RXREADY_CCASTART_Msk;
//        NRF_RADIO->TASKS_RXEN = 1;
//        state = STATE_IDLE;
//        break;
//    default:
//        assert(false);
//        state = 0;
//        break;
//    }

//    _state = state;
//    res = 0;

//end:
//    return res;
}

static int _confirm_op(ieee802154_dev_t *dev, ieee802154_hal_op_t op, void *ctx)
{
    (void)dev;
    (void)op;
    (void)ctx;

    DEBUG(DEBUG_PREFIX" Confirm op\n");
    //bool eagain;
    //ieee802154_tx_info_t *info = ctx;
    //int state = _state;
    //bool enable_shorts = false;
    //int radio_state = NRF_RADIO->STATE;
    //switch (op) {
    //case IEEE802154_HAL_OP_TRANSMIT:
    //    info = ctx;
    //    eagain = (state != STATE_IDLE
    //        && state != STATE_CCA_BUSY && NRF_RADIO->STATE != RADIO_STATE_STATE_Disabled);

    //    state = STATE_IDLE;
    //    enable_shorts = true;
    //    if (info) {
    //        info->status = (_state == STATE_CCA_BUSY) ? TX_STATUS_MEDIUM_BUSY : TX_STATUS_SUCCESS;
    //    }

    //    break;
    //case IEEE802154_HAL_OP_SET_RX:
    //    eagain = (radio_state == RADIO_STATE_STATE_RxRu);
    //    break;
    //case IEEE802154_HAL_OP_SET_IDLE:
    //    eagain = (radio_state == RADIO_STATE_STATE_TxDisable ||
    //              radio_state == RADIO_STATE_STATE_RxDisable);
    //    break;
    //case IEEE802154_HAL_OP_CCA:
    //    eagain = (state != STATE_CCA_BUSY && state != STATE_CCA_CLEAR);
    //    assert(ctx);
    //    *((bool*) ctx) = (state == STATE_CCA_CLEAR) ? true : false;
    //    state = STATE_IDLE;
    //    break;
    //default:
    //    eagain = false;
    //    assert(false);
    //    break;
    //}

    //if (eagain) {
    //    return -EAGAIN;
    //}

    //_state = state;

    //if (enable_shorts) {
    //    NRF_RADIO->SHORTS = DEFAULT_SHORTS;
    //    DEBUG("[nrf802154] TX Finished\n");
    //}
    return 0;
}

static int _len(ieee802154_dev_t *dev)
{
    (void)dev;
    return dwt_getframelength(NULL) - IEEE802154_FCS_LEN;
}

static int _read(ieee802154_dev_t *dev, void *buf, size_t max_size,
                          ieee802154_rx_info_t *info)
{
    (void)dev;
    size_t pktlen = (size_t)dwt_getframelength(NULL) - IEEE802154_FCS_LEN;
    int res = -ENOBUFS;

    if (max_size < pktlen) {
        DEBUG(DEBUG_PREFIX" read(): buffer is to small\n");
        return res;
    }

    DEBUG(DEBUG_PREFIX" read(): reading packet of length %i\n", pktlen);
    if (info != NULL) {
        // TODO read (calc) RSSI. From this also calc LQI which is not given
        info->lqi = info->rssi = 10;
    }
    dwt_readrxdata(buf, pktlen, 0);

    return pktlen;
}

static int _set_cca_threshold(ieee802154_dev_t *dev, int8_t threshold)
{
    (void)dev;
    (void)threshold;
    return 0;
}

int dw3000_ieee802154_init(void)
{
    DEBUG(DEBUG_PREFIX" Init\n");

    // decadriver init functions probably

    return 0;
}

//void isr_radio(void)
//{
//    ieee802154_dev_t *dev = dw3000_ieee802254_hal_dev;

//    if (NRF_RADIO->EVENTS_FRAMESTART) {
//        NRF_RADIO->EVENTS_FRAMESTART = 0;
//        if (_state == STATE_TX) {
//            dev->cb(dev, IEEE802154_RADIO_INDICATION_TX_START);
//        }
//        else if (_state == STATE_RX) {
//            dev->cb(dev, IEEE802154_RADIO_INDICATION_RX_START);
//        }
//    }

//    if (NRF_RADIO->EVENTS_CCAIDLE) {
//        NRF_RADIO->EVENTS_CCAIDLE = 0;
//        if (_state != STATE_TX) {
//            _state = STATE_CCA_CLEAR;
//            dev->cb(dev, IEEE802154_RADIO_CONFIRM_CCA);
//        }
//    }

//    if (NRF_RADIO->EVENTS_CCABUSY) {
//        NRF_RADIO->EVENTS_CCABUSY = 0;
//        if (_state == STATE_TX) {
//            _state = STATE_CCA_BUSY;
//            dev->cb(dev, IEEE802154_RADIO_CONFIRM_TX_DONE);
//        }
//        else {
//            _state = STATE_CCA_BUSY;
//            dev->cb(dev, IEEE802154_RADIO_CONFIRM_CCA);
//        }
//    }

//    if (NRF_RADIO->EVENTS_END) {
//        NRF_RADIO->EVENTS_END = 0;

//        switch (_state) {
//        case STATE_TX:
//            DEBUG("[nrf802154] TX state: %x\n", (uint8_t)NRF_RADIO->STATE);

//            _set_ifs_timer(txbuf[0] > IEEE802154_SIFS_MAX_FRAME_SIZE);
//            _state = STATE_IDLE;
//            dev->cb(dev, IEEE802154_RADIO_CONFIRM_TX_DONE);
//            break;
//        case STATE_RX:
//            if (NRF_RADIO->CRCSTATUS) {
//                bool is_ack = rxbuf[1] & IEEE802154_FCF_TYPE_ACK;

//                /* If radio is in promiscuous mode, indicate packet and
//                 * don't event think of sending an ACK frame :) */
//                if (cfg.promisc) {
//                    DEBUG("[nrf802154] Promiscuous mode is enabled.\n");
//                    _state = STATE_IDLE;
//                    dev->cb(dev, IEEE802154_RADIO_INDICATION_RX_DONE);
//                }
//                /* In case the packet is an ACK and the ACK filter is disabled,
//                 * indicate the frame reception */
//                else if (is_ack && !cfg.ack_filter) {
//                    DEBUG("[nrf802154] Received ACK.\n");
//                    _state = STATE_IDLE;
//                    dev->cb(dev, IEEE802154_RADIO_INDICATION_RX_DONE);
//                }
//                /* If the L2 filter passes the frame is indicated directly */
//                else if (_l2filter(rxbuf+1)) {
//                    DEBUG("[nrf802154] RX data frame.\n");
//                    _state = STATE_IDLE;
//                    dev->cb(dev, IEEE802154_RADIO_INDICATION_RX_DONE);
//                }
//                /* If all failed, simply drop the frame and continue listening
//                 * to incoming frames */
//                else {
//                    DEBUG("[nrf802154] Addr filter failed or ACK filter on.\n");
//                    NRF_RADIO->TASKS_START = 1;
//                }
//            }
//            else {
//                DEBUG("[nrf802154] CRC fail.\n");
//                dev->cb(dev, IEEE802154_RADIO_INDICATION_CRC_ERROR);
//            }
//            break;
//        default:
//            assert(false);
//        }
//    }

//    cortexm_isr_end();
//}

static int _confirm_on(ieee802154_dev_t *dev)
{
    (void)dev;
    return 0;
}

static int _request_on(ieee802154_dev_t *dev)
{
    (void)dev;
    //_state = STATE_IDLE;
    DEBUG(DEBUG_PREFIX" Request to turn on\n");

    return 0;
}

static int _config_phy(ieee802154_dev_t *dev, const ieee802154_phy_conf_t *conf)
{
    (void)dev;
    (void)conf;
    DEBUG(DEBUG_PREFIX" config phy\n");
    //int8_t pow = conf->pow;

    //if (pow < TX_POWER_MIN || pow > TX_POWER_MAX) {
    //    return -EINVAL;
    //}

    //assert(NRF_RADIO->STATE == RADIO_STATE_STATE_Disabled);

    ///* The value of this register represents the frequency offset (in MHz) from
    // * 2400 MHz.  Channel 11 (first 2.4 GHz band channel) starts at 2405 MHz
    // * and all channels have a bandwidth of 5 MHz. Thus, we subtract 10 to the
    // * channel number and multiply by 5 to calculate the offset.
    // */
    //NRF_RADIO->FREQUENCY = (((uint8_t) conf->channel) - 10) * 5;

    //DEBUG("[nrf802154] setting channel to %i\n", conf->channel);
    //DEBUG("[nrf802154] setting TX power to %i\n", conf->pow);
    //_set_txpower(pow);

    return 0;
}

static int _off(ieee802154_dev_t *dev)
{
    (void)dev;
    DEBUG(DEBUG_PREFIX" off\n");
    return -ENOTSUP;
}

static int _set_cca_mode(ieee802154_dev_t *dev, ieee802154_cca_mode_t mode)
{
    (void)dev;
    if (mode != IEEE802154_CCA_MODE_ALOHA) {
        // TODO add Peamble Detection special case
        return -ENOTSUP;
    }

    return 0;
}

static int _config_addr_filter(ieee802154_dev_t *dev, ieee802154_af_cmd_t cmd, const void *value)
{
    (void)dev;
    (void)cmd;
    (void)value;

    DEBUG(DEBUG_PREFIX" Config address filter\n");
    //const uint16_t *pan_id = value;
    //switch (cmd) {
    //    case IEEE802154_AF_SHORT_ADDR:
    //        memcpy(nrf802154_short_addr, value, IEEE802154_SHORT_ADDRESS_LEN);
    //        break;
    //    case IEEE802154_AF_EXT_ADDR:
    //        memcpy(nrf802154_long_addr, value, IEEE802154_LONG_ADDRESS_LEN);
    //        break;
    //    case IEEE802154_AF_PANID:
    //        nrf802154_pan_id = *pan_id;
    //        break;
    //    case IEEE802154_AF_PAN_COORD:
    //        return -ENOTSUP;
    //}

    return 0;
}

static int _config_src_addr_match(ieee802154_dev_t *dev, ieee802154_src_match_t cmd,
                                  const void *value)
{
    (void)dev;
    (void)cmd;
    (void)value;
    DEBUG(DEBUG_PREFIX" Src addr match\n");
    //switch (cmd) {
    //    case IEEE802154_SRC_MATCH_EN:
    //        cfg.pending = *((const bool*) value);
    //        break;
    //    default:
    //        return -ENOTSUP;
    //}
    return 0;
}

static int _set_frame_filter_mode(ieee802154_dev_t *dev, ieee802154_filter_mode_t mode)
{
    (void)dev;
    (void)mode;
    DEBUG(DEBUG_PREFIX" Set frame filter mode\n");
    //bool ackf = true;
    //bool _promisc = false;

    //switch (mode) {
    //    case IEEE802154_FILTER_ACCEPT:
    //        break;
    //    case IEEE802154_FILTER_PROMISC:
    //        _promisc = true;
    //        break;
    //    case IEEE802154_FILTER_ACK_ONLY:
    //        ackf = false;
    //        break;
    //    default:
    //        return -ENOTSUP;
    //}

    //cfg.ack_filter = ackf;
    //cfg.promisc = _promisc;

    return 0;
}

static int _get_frame_filter_mode(ieee802154_dev_t *dev, ieee802154_filter_mode_t *mode)
{
    (void) dev;
    (void) mode;
    DEBUG(DEBUG_PREFIX" Get frame filter mode\n");
    //if (cfg.promisc) {
    //    *mode = IEEE802154_FILTER_PROMISC;
    //}
    //else if (!cfg.ack_filter) {
    //    *mode = IEEE802154_FILTER_ACK_ONLY;
    //}
    //else {
    //    *mode = IEEE802154_FILTER_ACCEPT;
    //}
    return 0;
}

static int _set_csma_params(ieee802154_dev_t *dev, const ieee802154_csma_be_t *bd,
                            int8_t retries)
{
    (void)dev;
    (void)bd;

    if (retries > 0) {
        return -ENOTSUP;
    }
    // TODO case where == 0
    //cfg.cca_send = (retries == 0);

    return 0;
}

void dw3000_ieee802154_setup(void *dev)
{
    (void)dev;
    dw3000_ieee802154_init();
}

void dw3000_ieee802154_hal_setup(ieee802154_dev_t *hal)
{
    /* We don't set hal->priv because the context of this device is global */
    /* We need to store a reference to the HAL descriptor though for the ISR */
    hal->driver = &dw3000_ieee802254_ops;
    dw3000_ieee802254_hal_dev = hal;
}

static const ieee802154_radio_ops_t dw3000_ieee802254_ops = {
    .caps =  IEEE802154_CAP_AUTO_ACK
          | IEEE802154_CAP_IRQ_ACK_TIMEOUT
          | IEEE802154_CAP_PHY_HRP
          | IEEE802154_CAP_IRQ_CRC_ERROR
          | IEEE802154_CAP_IRQ_RX_START
          | IEEE802154_CAP_IRQ_TX_START
          | IEEE802154_CAP_IRQ_TX_DONE,

    .write = _write,                            // Done
    .read = _read,                              // Done, RSSI TODO
    .request_on = _request_on,
    .confirm_on = _confirm_on,
    .len = _len,                                // Done
    .off = _off,
    .request_op = _request_op,
    .confirm_op = _confirm_op,
    .set_cca_threshold = _set_cca_threshold,    // Done, ret val questionable
    .set_cca_mode = _set_cca_mode,              // Done
    .config_phy = _config_phy,
    .set_csma_params = _set_csma_params,
    .config_addr_filter = _config_addr_filter,
    .config_src_addr_match = _config_src_addr_match,
    .set_frame_filter_mode = _set_frame_filter_mode,
    .get_frame_filter_mode = _get_frame_filter_mode,
};
