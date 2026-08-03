#include <assert.h>
#include <string.h>
#include <errno.h>
#include <inttypes.h>

#include "net/ieee802154.h"
#include "net/ieee802154/radio.h"

#include "deca_device_api.h"
#include "dw3000.h"

#define ENABLE_DEBUG        1
#include "debug.h"

// TODO remove
#include "fmt.h"

#define DEBUG_PREFIX        "[dw3000 802.15.4 HAL]"

#define FRAME_FILTER_ALL (DWT_FF_BEACON_EN | DWT_FF_DATA_EN | \
                          DWT_FF_ACK_EN | DWT_FF_COORD_EN)

// SSADRAPE is not public but it should be
#define FRAME_FILTER_ALL_PENDING (FRAME_FILTER_ALL | 0xC000U)

#define AUTO_ACK_RESPONSE_DELAY (100)
// TODO Tune

typedef enum {
    STATE_IDLE,     /* = IDLE */
    STATE_TX,
    STATE_TX_AWAITING_ACK,
    STATE_RX,       /* = RX */
    STATE_SLEEPING  /* = OFF */
} dw3000_state_t;

static volatile uint8_t _state;
static volatile bool _pre_tx_cca = false;
static volatile bool _is_pan_coord = false;
static volatile bool _promisc = false;
static volatile bool _auto_pending_bit = false;

static uint8_t _last_tx_fcf;
static uint8_t _last_rx_fcf;

static volatile ieee802154_cca_mode_t _cca_mode = IEEE802154_CCA_MODE_ALOHA;

typedef enum {
    TX_ERROR_NONE = 0,
    TX_ERROR_NOT_YET,
    TX_ERROR_CCA,
    TX_ERROR_NO_ACK
} tx_error_t;

static volatile tx_error_t _tx_error;

static const ieee802154_radio_ops_t dw3000_ieee802254_ops;
static ieee802154_dev_t *dw3000_ieee802254_hal_dev;

static void _irq_tx_done_cb(const dwt_cb_data_t* dat);
static void _irq_rx_ok_cb(const dwt_cb_data_t* dat);
static void _irq_rx_err_cb(const dwt_cb_data_t* dat);
static void _irq_rx_to_cb(const dwt_cb_data_t* dat);

static void _apply_framefilter(void);

static dwt_callbacks_s _dwt_callbacks = {
    .cbTxDone = _irq_tx_done_cb,
    .cbRxOk = _irq_rx_ok_cb,
    .cbRxErr = _irq_rx_err_cb,
    .cbRxTo = _irq_rx_to_cb,
};

static dwt_config_t _config = {
    .chan = 9,
    .txPreambLength = DWT_PLEN_64,
    .rxPAC = DWT_PAC8,
    .txCode = 11,
    .rxCode = 11,
    .sfdType = DWT_SFD_IEEE_4Z,
    .dataRate = DWT_BR_6M8,
    .phrMode = DWT_PHRMODE_STD,
    .phrRate = DWT_PHRRATE_STD,
    .sfdTO = (64 + 1 + 8 - 8),   /* (plen + 1 + SFD length - PAC size) */
    .stsMode = DWT_STS_MODE_OFF, /* DWT_STS_MODE_1 | DWT_STS_MODE_SDC, */
    .stsLength = DWT_STS_LEN_64,
    .pdoaMode = DWT_PDOA_M0,     /* off */
};

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
                DEBUG(DEBUG_PREFIX" write(): failed\n");
                return -ENOBUFS;
            }

            if (len == 0) {
                /* On first iteration save the header */
                _last_tx_fcf = *(uint8_t*)(iolist->iol_base);
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

    uint8_t next_state = STATE_IDLE;

    switch (op) {
    case IEEE802154_HAL_OP_TRANSMIT:
        if (_state != STATE_IDLE) {
            DEBUG(DEBUG_PREFIX" request_op(TX): fail, from non IDLE\n");
            return -EBUSY;
        }
        else {
            uint8_t flags = DWT_START_TX_IMMEDIATE;
            if (_pre_tx_cca) {
                flags |= DWT_START_TX_CCA;
                // TODO set correct timeout
                dwt_setpreambledetecttimeout(10);
            }
            if (_last_tx_fcf & IEEE802154_FCF_ACK_REQ) {
                flags |= DWT_RESPONSE_EXPECTED;
                // TODO correct timeout, this currently is ~ 1 s
                dwt_setrxtimeout(0xFFFFF);
            }
            next_state = STATE_TX;
            _tx_error = TX_ERROR_NOT_YET;
            dwt_starttx(flags);
            break;
        }
    case IEEE802154_HAL_OP_SET_RX:
        switch (_state) {
        case STATE_IDLE:
            DEBUG(DEBUG_PREFIX" request_op(RX): success, from IDLE\n");
            dwt_setrxtimeout(0);
            dwt_setpreambledetecttimeout(0);
            dwt_rxenable(DWT_START_RX_IMMEDIATE);
            next_state = STATE_RX;
            break;
        case STATE_TX:
            DEBUG(DEBUG_PREFIX" request_op(RX): fail, from TX\n");
            return -EBUSY;
        case STATE_RX:
            DEBUG(DEBUG_PREFIX" request_op(RX): success, from RX\n");
            next_state = STATE_RX;
            break;
        default:
            // TODO
            break;
        }
        break;
    case IEEE802154_HAL_OP_SET_IDLE: {
        assert(ctx);
        bool force = *((bool*) ctx);
        if (force || _state != STATE_TX) {
            /* This function already puts the device in IDLE */
            dwt_forcetrxoff();
            DEBUG(DEBUG_PREFIX" request_op(IDLE): success\n");
            next_state = STATE_IDLE;
            break;
        }

        DEBUG(DEBUG_PREFIX" request_op(IDLE): fail, from TX without force\n");
        return -EBUSY;
    }
    case IEEE802154_HAL_OP_CCA:
        DEBUG(DEBUG_PREFIX" request_op(CCA)\n");
        /* TODO Preamble detection CCA */
        return 0;
    default:
        DEBUG(DEBUG_PREFIX" request_op(): op not implemented\n");
        assert(false);
        return -ENOTSUP;
    }

    _state = next_state;
    return 0;
}

static int _confirm_op(ieee802154_dev_t *dev, ieee802154_hal_op_t op, void *ctx)
{
    (void)dev;
    (void)op;
    (void)ctx;

    DEBUG(DEBUG_PREFIX" Confirm op\n");
    bool eagain = false;
    ieee802154_tx_info_t *info = ctx;
    int state = _state;
    int res = 0;

    switch (op) {
    case IEEE802154_HAL_OP_TRANSMIT:
        info = ctx;
        eagain = (state != STATE_IDLE);
        // TODO guarantee from every TX error state IDLE is reached automatically

        state = STATE_IDLE;
        if (info) {
            switch (_tx_error) {
            case TX_ERROR_NONE:
                info->status = TX_STATUS_SUCCESS;
                if (_last_rx_fcf & IEEE802154_FCF_FRAME_PEND) {
                    info->status = TX_STATUS_FRAME_PENDING;
                }
                break;
            case TX_ERROR_CCA:
                info->status = TX_STATUS_MEDIUM_BUSY;
                break;
            case TX_ERROR_NO_ACK:
                info->status = TX_STATUS_NO_ACK;
                break;
            default:
                break;
            }
        }

        break;
    case IEEE802154_HAL_OP_SET_RX:
    case IEEE802154_HAL_OP_SET_IDLE:
        /* RX and IDLE transisitons should always work */
        break;
    case IEEE802154_HAL_OP_CCA:
        if (_cca_mode == IEEE802154_CCA_MODE_ALOHA) {
            /* Always report clear channel */
            res = 1;
            *((bool*) ctx) = true;
        }
        state = STATE_IDLE;
        break;
    default:
        assert(false);
        break;
    }

    if (eagain) {
        return -EAGAIN;
    }

    _state = state;
    return res;
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

    if (dw3000_hw_init() != 0) {
        DEBUG_PUTS("[deca init] Error: Hardware initialization failed!");
        return 1;
    }

    dw3000_hw_reset();

    if (dwt_probe((struct dwt_probe_s *)&dw3000_probe_interf) != DWT_SUCCESS) {
        DEBUG_PUTS("[deca init] Device Probing failed!");
        return 1;
    }

    /* dwt_probe() initializes data structures used by the `deca tx` thread,
     * so it has to be called first */
    if (dw3000_hw_init_interrupt() != 0) {
        DEBUG_PUTS("[deca init] Error: Interrupt initialization failed!");
        return 1;
    }

    /* The API guide says it is recommended to check for idle rc before
     * dwt_initialise(), while libdeca does the other order. */
    while (!dwt_checkidlerc()) {};
    DEBUG_PUTS("[deca init] DW3xxx reached IDLE_RC");

    if (dwt_initialise(DWT_DW_IDLE) != DWT_SUCCESS) {
        DEBUG_PUTS("[deca init] Error initializing device");
        return 1;
    }

    /* After idle rc the SPI speed can be increased */
    dw3000_spi_speed_fast();

    if (dwt_configure(&_config) != DWT_SUCCESS) {
        DEBUG_PUTS("[deca init] Error configuring device to given parameters");
        return 1;
    }

    dwt_setcallbacks(&_dwt_callbacks);
    dwt_setinterrupt(DWT_INT_RXFCG_BIT_MASK | /* <-- RX success */
                     /* RX errors: */
                     DWT_INT_RXPHE_BIT_MASK |
                     DWT_INT_RXFCE_BIT_MASK |
                     DWT_INT_RXFSL_BIT_MASK |
                     DWT_INT_RXSTO_BIT_MASK |
                     DWT_INT_CIAERR_BIT_MASK |
                     DWT_INT_ARFE_BIT_MASK |
                     /* RX timeouts: */
                     DWT_INT_RXFTO_BIT_MASK |
                     DWT_INT_RXPTO_BIT_MASK |
                     /* TX done: */
                     DWT_INT_TXFRS_BIT_MASK,
                     0, DWT_ENABLE_INT_ONLY);

    DEBUG_PUTS("[deca init] Device initialized and configured");

    /* Default enable the framefilter */
    _apply_framefilter();

    return 0;
}

static void _irq_tx_done_cb(const dwt_cb_data_t* dat)
{
    (void) dat;
    ieee802154_dev_t *dev = dw3000_ieee802254_hal_dev;
    DEBUG(DEBUG_PREFIX" TX_DONE: 0x%"PRIx32"\n", dat->status);

    if (_last_tx_fcf & IEEE802154_FCF_ACK_REQ) {
        _tx_error = TX_ERROR_NOT_YET;
        _state = STATE_TX_AWAITING_ACK;
    }
    else {
        _tx_error = TX_ERROR_NONE;
        _state = STATE_IDLE;
        dev->cb(dev, IEEE802154_RADIO_CONFIRM_TX_DONE);
    }
}

static void _irq_rx_ok_cb(const dwt_cb_data_t* dat)
{
    (void) dat;
    ieee802154_dev_t *dev = dw3000_ieee802254_hal_dev;
    DEBUG(DEBUG_PREFIX" RX_OK: 0x%"PRIx32"\n", dat->status);

    dwt_readrxdata(&_last_rx_fcf, 1, 0);

    // TODO in promisc mode all received frames should be indicated
    if ((_last_rx_fcf & IEEE802154_FCF_TYPE_ACK) == 0 && _state == STATE_RX) {
        _state = STATE_IDLE;
        dev->cb(dev, IEEE802154_RADIO_INDICATION_RX_DONE);
    }
    /* Is ACK and we wait for an ACK */
    // TODO verify source address?
    else if (_last_rx_fcf & IEEE802154_FCF_TYPE_ACK &&
             _state == STATE_TX_AWAITING_ACK)
    {
        _state = STATE_IDLE;
        _tx_error = TX_ERROR_NONE;
        dev->cb(dev, IEEE802154_RADIO_CONFIRM_TX_DONE);
    }
}

static void _irq_rx_err_cb(const dwt_cb_data_t* dat)
{
    ieee802154_dev_t *dev = dw3000_ieee802254_hal_dev;
    DEBUG(DEBUG_PREFIX" RX_ERR: 0x%"PRIx32"\n", dat->status);

    if (dat->status & DWT_INT_RXFCE_BIT_MASK) {
        dev->cb(dev, IEEE802154_RADIO_INDICATION_CRC_ERROR); 
    }
    else {
        dwt_rxenable(DWT_START_RX_IMMEDIATE);
    }
}

static void _irq_rx_to_cb(const dwt_cb_data_t* dat)
{
    (void) dat;
    ieee802154_dev_t *dev = dw3000_ieee802254_hal_dev;
    DEBUG(DEBUG_PREFIX" RX_TO: 0x%"PRIx32"\n", dat->status);

    /* Normal RX does not time out, so it has to be waiting for an ACK */
    _tx_error = TX_ERROR_NO_ACK;
    _state = STATE_IDLE;
    dev->cb(dev, IEEE802154_RADIO_CONFIRM_TX_DONE);
}

static int _confirm_on(ieee802154_dev_t *dev)
{
    (void)dev;
    /* After sleep, IDLE will be automatically reached */
    return 0;
}

static int _request_on(ieee802154_dev_t *dev)
{
    (void)dev;
    DEBUG(DEBUG_PREFIX" request_on()\n");
    if (_state == STATE_SLEEPING) {
        dw3000_hw_wakeup();
        _state = STATE_IDLE;
    }

    return 0;
}

static int _config_phy(ieee802154_dev_t *dev, const ieee802154_phy_conf_t *conf)
{
    (void)dev;
    DEBUG(DEBUG_PREFIX" config_phy()\n");
    int8_t pow = conf->pow;
    uint16_t chan = conf->channel;

    assert(_state == STATE_IDLE);

    // TODO probably need full reconfiguration to recommended values at channel switch
    // copy the good value finder from libdeca

    if (chan != 9 && chan != 5) {
        DEBUG(DEBUG_PREFIX" config_phy(): invalid channel %"PRIu16"\n", chan);
        return -EINVAL;
    }

    DEBUG(DEBUG_PREFIX" config_phy(): channel %"PRIu16"\n", chan);
    DEBUG(DEBUG_PREFIX" config_phy(): power %"PRIi8"\n", pow);

    dwt_setchannel(chan);
    (void) pow;
    // TODO TX power calculation

    return 0;
}

static int _off(ieee802154_dev_t *dev)
{
    (void)dev;
    DEBUG(DEBUG_PREFIX" off(): (sleeping)\n");

    dwt_configuresleep(DWT_PGFCAL | DWT_GOTOIDLE | DWT_RUNSAR | DWT_CONFIG,
                       DWT_WAKE_CSN | DWT_WAKE_WUP | DWT_SLP_EN);

    dw3000_hw_wakeup_pin_low();
    dwt_entersleep(DWT_DW_IDLE_RC);
    _state = STATE_SLEEPING;

    return 0;
}

static int _set_cca_mode(ieee802154_dev_t *dev, ieee802154_cca_mode_t mode)
{
    (void)dev;
    if (mode != IEEE802154_CCA_MODE_ALOHA) {
        // TODO add Peamble Detection special case
        return -ENOTSUP;
    }

    _cca_mode = mode;

    return 0;
}

static int _config_addr_filter(ieee802154_dev_t *dev, ieee802154_af_cmd_t cmd, const void *value)
{
    (void)dev;

    DEBUG(DEBUG_PREFIX" config_addr_filter()\n");
    switch (cmd) {
    case IEEE802154_AF_SHORT_ADDR: {
        uint16_t addr_host = byteorder_ntohs(*((network_uint16_t*) value));
        dwt_setaddress16(addr_host);
        break;
    }
    case IEEE802154_AF_EXT_ADDR:
        /* They expect the value in LE not BE*/
        le_uint64_t addr = byteorder_btolll(((eui64_t*)value)->uint64);
        dwt_seteui(addr.u8);
        break;
    case IEEE802154_AF_PANID:
        dwt_setpanid(*((uint16_t*) value));
        // TODO maybe we need to save the PAN
        break;
    case IEEE802154_AF_PAN_COORD:
        _is_pan_coord = *(bool*) value;
        break;
    }

    return 0;
}

static void _apply_framefilter(void)
{
    if (_promisc) {
        dwt_configureframefilter(DWT_FF_DISABLE, 0);
        return;
    }
    /* FF enabled */
    dwt_configureframefilter(DWT_FF_ENABLE_802_15_4,
                             _auto_pending_bit ? FRAME_FILTER_ALL_PENDING :
                                                 FRAME_FILTER_ALL);
    dwt_enableautoack(AUTO_ACK_RESPONSE_DELAY, 1);
}

static int _config_src_addr_match(ieee802154_dev_t *dev, ieee802154_src_match_t cmd,
                                  const void *value)
{
    (void)dev;
    switch (cmd) {
    case IEEE802154_SRC_MATCH_EN:
        if (_promisc) {
            /* The auto ACK requires frame filtering */
            return -ENOTSUP;
        } else {
            _auto_pending_bit = *((const bool*) value);
            _apply_framefilter();
            break;
        }
    default:
        /* There are 4 slots for short addresses in such a table, but these are
         * currently not implemented */
        return -ENOTSUP;
    }
    return 0;
}

static int _set_frame_filter_mode(ieee802154_dev_t *dev, ieee802154_filter_mode_t mode)
{
    (void)dev;
    DEBUG(DEBUG_PREFIX" set_frame_filter_mode()\n");

    switch (mode) {
    case IEEE802154_FILTER_ACCEPT:
        _promisc = false;
        break;
    case IEEE802154_FILTER_PROMISC:
        /* This will also disable auto ACK */
        _promisc = true;
        break;
    default:
        return -ENOTSUP;
    }

    _apply_framefilter();

    return 0;
}

static int _get_frame_filter_mode(ieee802154_dev_t *dev, ieee802154_filter_mode_t *mode)
{
    (void) dev;
    DEBUG(DEBUG_PREFIX" get_frame_filter_mode()\n");

    // TODO update if function above updates
    *mode = IEEE802154_FILTER_ACCEPT;
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
    _pre_tx_cca = (retries == 0);

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
          | IEEE802154_CAP_PHY_HRP
          | IEEE802154_CAP_IRQ_ACK_TIMEOUT
          | IEEE802154_CAP_IRQ_CRC_ERROR
          //| IEEE802154_CAP_IRQ_RX_START
          //| IEEE802154_CAP_IRQ_TX_START
          | IEEE802154_CAP_IRQ_TX_DONE, 
          // TODO possibly CCA Done IRQ

    .write = _write,                                    // Done
    .read = _read,                                      // Done, RSSI TODO
    .request_on = _request_on,                          // Done
    .confirm_on = _confirm_on,                          // Done
    .len = _len,                                        // Done
    .off = _off,                                        // Done
    .request_op = _request_op,
    .confirm_op = _confirm_op,
    .set_cca_threshold = _set_cca_threshold,            // Done, ret val questionable
    .set_cca_mode = _set_cca_mode,                      // Done
    .config_phy = _config_phy,                          // Done, TX power missing
    .set_csma_params = _set_csma_params,                // Done
    .config_addr_filter = _config_addr_filter,          // Done
    .config_src_addr_match = _config_src_addr_match,    // Done
    .set_frame_filter_mode = _set_frame_filter_mode,    // Done (extendable)
    .get_frame_filter_mode = _get_frame_filter_mode,    // Done
};
