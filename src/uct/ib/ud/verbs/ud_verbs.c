/**
* Copyright (c) NVIDIA CORPORATION & AFFILIATES, 2001-2019. ALL RIGHTS RESERVED.
* Copyright (c) Google, LLC, 2024. ALL RIGHTS RESERVED.
*
* See file LICENSE for terms.
*/

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <uct/api/uct.h>
#include <uct/ib/base/ib_iface.h>
#include <uct/base/uct_md.h>
#include <uct/base/uct_log.h>
#include <ucs/debug/log.h>
#include <ucs/debug/memtrack_int.h>
#include <ucs/type/class.h>
#include <string.h>
#include <arpa/inet.h> /* For htonl */

#include <uct/ib/base/ib_log.h>

#include <uct/ib/ud/base/ud_iface.h>
#include <uct/ib/ud/base/ud_ep.h>
#include <uct/ib/ud/base/ud_def.h>

#include "ud_verbs.h"
#include "ucs/sys/math.h"

#include <uct/ib/ud/base/ud_inl.h>


#define UCT_UD_VERBS_IFACE_OVERHEAD 105e-9


static UCS_F_NOINLINE void
uct_ud_verbs_iface_post_recv_always(uct_ud_verbs_iface_t *iface, int max);

static inline void
uct_ud_verbs_iface_post_recv(uct_ud_verbs_iface_t *iface);

static ucs_config_field_t uct_ud_verbs_iface_config_table[] = {
  {"UD_", UCT_IB_SEND_OVERHEAD_DEFAULT(UCT_UD_VERBS_IFACE_OVERHEAD), NULL,
   0, UCS_CONFIG_TYPE_TABLE(uct_ud_iface_config_table)},

  {NULL}
};


UCS_CLASS_INIT_FUNC(uct_ud_verbs_ep_t, const uct_ep_params_t *params)
{
    uct_ud_verbs_iface_t *iface = ucs_derived_of(params->iface,
                                                 uct_ud_verbs_iface_t);

    ucs_trace_func("");
    UCS_CLASS_CALL_SUPER_INIT(uct_ud_ep_t, &iface->super, params);
    self->peer_address.ah = NULL;
    return UCS_OK;
}

static UCS_CLASS_CLEANUP_FUNC(uct_ud_verbs_ep_t)
{
    ucs_trace_func("");
}

UCS_CLASS_DEFINE(uct_ud_verbs_ep_t, uct_ud_ep_t);
static UCS_CLASS_DEFINE_NEW_FUNC(uct_ud_verbs_ep_t, uct_ep_t,
                                 const uct_ep_params_t *);
UCS_CLASS_DEFINE_DELETE_FUNC(uct_ud_verbs_ep_t, uct_ep_t);

static inline void
uct_ud_verbs_post_send(uct_ud_verbs_iface_t *iface, uct_ud_verbs_ep_t *ep,
                       struct ibv_send_wr *wr, unsigned send_flags,
                       unsigned max_log_sge)
{
    uct_ib_device_t *dev = uct_ib_iface_device(&iface->super.super);
    struct ibv_send_wr *bad_wr;
    int UCS_V_UNUSED ret;

    if (!dev->ordered_send_comp) {
        wr->send_flags             = send_flags | IBV_SEND_SIGNALED;
        wr->wr_id                  = iface->tx.send_sn;
    } else if ((send_flags & IBV_SEND_SIGNALED) ||
               (iface->super.tx.unsignaled >= (UCT_UD_TX_MODERATION - 1))) {
        wr->send_flags             = send_flags | IBV_SEND_SIGNALED;
        wr->wr_id                  = iface->super.tx.unsignaled;
        iface->super.tx.unsignaled = 0;
    } else {
        wr->send_flags             = send_flags;
#if UCS_ENABLE_ASSERT
        wr->wr_id                  = UINT64_MAX;
#endif
        ++iface->super.tx.unsignaled;
    }

    wr->wr.ud.remote_qpn = ep->peer_address.dest_qpn;
    wr->wr.ud.ah         = ep->peer_address.ah;

    UCT_UD_EP_HOOK_CALL_TX(&ep->super, (uct_ud_neth_t*)iface->tx.sge[0].addr);
    ret = ibv_post_send(iface->super.qp, wr, &bad_wr);
    ucs_assertv(ret == 0, "ibv_post_send() returned %d (%m)", ret);

    uct_ib_log_post_send(&iface->super.super, iface->super.qp, wr, max_log_sge,
                         uct_ud_dump_packet);
    --iface->super.tx.available;
    ++iface->tx.send_sn;
}

static inline void
uct_ud_verbs_ep_tx_inlv(uct_ud_verbs_iface_t *iface, uct_ud_verbs_ep_t *ep,
                        const void *buffer, unsigned length)
{
    iface->tx.sge[1].addr    = (uintptr_t)buffer;
    iface->tx.sge[1].length  = length;
    iface->tx.wr_inl.num_sge = 2;
    uct_ud_verbs_post_send(iface, ep, &iface->tx.wr_inl, IBV_SEND_INLINE, 2);
}

static inline void
uct_ud_verbs_ep_tx_skb(uct_ud_verbs_iface_t *iface, uct_ud_verbs_ep_t *ep,
                       uct_ud_send_skb_t *skb, unsigned send_flags,
                       unsigned max_log_sge)
{
    iface->tx.sge[0].lkey   = skb->lkey;
    iface->tx.sge[0].length = skb->len;
    iface->tx.sge[0].addr   = (uintptr_t)skb->neth;
    uct_ud_verbs_post_send(iface, ep, &iface->tx.wr_skb, send_flags, max_log_sge);
}

static uint16_t
uct_ud_verbs_ep_send_ctl(uct_ud_ep_t *ud_ep, uct_ud_send_skb_t *skb,
                         const uct_ud_iov_t *iov, uint16_t iovcnt, int flags,
                         int max_log_sge)
{
    uct_ud_verbs_iface_t *iface = ucs_derived_of(ud_ep->super.super.iface,
                                                 uct_ud_verbs_iface_t);
    uct_ib_device_t *dev  = uct_ib_iface_device(&iface->super.super);
    uct_ud_verbs_ep_t *ep = ucs_derived_of(ud_ep, uct_ud_verbs_ep_t);
    unsigned send_flags;
    uint16_t iov_index;

    /* set send flags */
    send_flags = 0;
    if ((skb->len <= iface->super.config.max_inline) && (iovcnt == 0)) {
        send_flags |= IBV_SEND_INLINE;
    } else {
        ucs_assert(!(flags & UCT_UD_IFACE_SEND_CTL_FLAG_INLINE));
    }
    if ((flags & UCT_UD_IFACE_SEND_CTL_FLAG_SOLICITED) &&
        dev->req_notify_cq_support) {
        send_flags |= IBV_SEND_SOLICITED;
    }
    if (flags & UCT_UD_IFACE_SEND_CTL_FLAG_SIGNALED) {
        send_flags |= IBV_SEND_SIGNALED;
    }

    /* copy iov array */
    for (iov_index = 0; iov_index < iovcnt; ++iov_index) {
        iface->tx.sge[iov_index + 1].addr   = (uintptr_t)iov[iov_index].buffer;
        iface->tx.sge[iov_index + 1].length = iov[iov_index].length;
        iface->tx.sge[iov_index + 1].lkey   = iov[iov_index].lkey;
    }
    iface->tx.wr_skb.num_sge = iovcnt + 1;

    uct_ud_verbs_ep_tx_skb(iface, ep, skb, send_flags, max_log_sge);
    iface->tx.wr_skb.num_sge = 1;

    return iface->tx.send_sn;
}

static
ucs_status_t uct_ud_verbs_ep_am_short(uct_ep_h tl_ep, uint8_t id, uint64_t hdr,
                                      const void *buffer, unsigned length)
{
    uct_ud_verbs_ep_t *ep = ucs_derived_of(tl_ep, uct_ud_verbs_ep_t);
    uct_ud_verbs_iface_t *iface = ucs_derived_of(tl_ep->iface,
                                                 uct_ud_verbs_iface_t);
    uct_ud_send_skb_t *skb;
    uct_ud_am_short_hdr_t *am_hdr;
    ucs_status_t status;

    UCT_CHECK_LENGTH(sizeof(uct_ud_neth_t) + sizeof(hdr) + length,
                     0, iface->super.config.max_inline, "am_short");

    uct_ud_enter(&iface->super);

    status = uct_ud_am_skb_common(&iface->super, &ep->super, id, &skb);
    if (status != UCS_OK) {
        uct_ud_leave(&iface->super);
        return status;
    }

    am_hdr = (uct_ud_am_short_hdr_t *)(skb->neth+1);
    am_hdr->hdr = hdr;
    iface->tx.sge[0].length = sizeof(uct_ud_neth_t) + sizeof(*am_hdr);
    iface->tx.sge[0].addr   = (uintptr_t)skb->neth;

    uct_ud_verbs_ep_tx_inlv(iface, ep, buffer, length);

    skb->len = iface->tx.sge[0].length;

    uct_ud_iface_complete_tx_inl(&iface->super, &ep->super, skb,
                                 am_hdr+1, buffer, length);
    UCT_TL_EP_STAT_OP(&ep->super.super, AM, SHORT, sizeof(hdr) + length);
    uct_ud_leave(&iface->super);
    return UCS_OK;
}

static ucs_status_t uct_ud_verbs_ep_am_short_iov(uct_ep_h tl_ep, uint8_t id,
                                                 const uct_iov_t *iov, size_t iovcnt)
{
    uct_ud_verbs_ep_t *ep       = ucs_derived_of(tl_ep, uct_ud_verbs_ep_t);
    uct_ud_verbs_iface_t *iface = ucs_derived_of(tl_ep->iface, uct_ud_verbs_iface_t);
    uct_ud_send_skb_t *skb;
    ucs_status_t status;

    UCT_CHECK_IOV_SIZE(iovcnt, (size_t)iface->config.max_send_sge,
                       "uct_ud_verbs_ep_am_short_iov");
    UCT_CHECK_LENGTH(sizeof(uct_ud_neth_t) + uct_iov_total_length(iov, iovcnt), 0,
                     iface->super.config.max_inline, "am_short");

    uct_ud_enter(&iface->super);

    status = uct_ud_am_skb_common(&iface->super, &ep->super, id, &skb);
    if (status != UCS_OK) {
        uct_ud_leave(&iface->super);
        return status;
    }

    skb->len = iface->tx.sge[0].length = sizeof(uct_ud_neth_t);
    iface->tx.sge[0].addr              = (uintptr_t)skb->neth;
    iface->tx.wr_inl.num_sge           = uct_ib_verbs_sge_fill_iov(iface->tx.sge + 1,
                                                                    iov, iovcnt) + 1;
    uct_ud_verbs_post_send(iface, ep, &iface->tx.wr_inl, IBV_SEND_INLINE,
                           iface->tx.wr_inl.num_sge);

    uct_ud_iov_to_skb(skb, iov, iovcnt);
    uct_ud_iface_complete_tx_skb(&iface->super, &ep->super, skb);
    UCT_TL_EP_STAT_OP(&ep->super.super, AM, SHORT, uct_iov_total_length(iov, iovcnt));
    uct_ud_leave(&iface->super);

    return UCS_OK;
}

static ssize_t uct_ud_verbs_ep_am_bcopy(uct_ep_h tl_ep, uint8_t id,
                                        uct_pack_callback_t pack_cb, void *arg,
                                        unsigned flags)
{
    uct_ud_verbs_ep_t *ep = ucs_derived_of(tl_ep, uct_ud_verbs_ep_t);
    uct_ud_verbs_iface_t *iface = ucs_derived_of(tl_ep->iface,
                                                 uct_ud_verbs_iface_t);
    uct_ud_send_skb_t *skb;
    ucs_status_t status;
    size_t length;

    uct_ud_enter(&iface->super);

    status = uct_ud_am_skb_common(&iface->super, &ep->super, id, &skb);
    if (status != UCS_OK) {
        uct_ud_leave(&iface->super);
        return status;
    }

    length = uct_ud_skb_bcopy(skb, pack_cb, arg);
    UCT_UD_CHECK_BCOPY_LENGTH(&iface->super, length);

    ucs_assert(iface->tx.wr_skb.num_sge == 1);
    uct_ud_verbs_ep_tx_skb(iface, ep, skb, 0, INT_MAX);
    uct_ud_iface_complete_tx_skb(&iface->super, &ep->super, skb);
    UCT_TL_EP_STAT_OP(&ep->super.super, AM, BCOPY, length);
    uct_ud_leave(&iface->super);
    return length;
}

static ucs_status_t
uct_ud_verbs_ep_am_zcopy(uct_ep_h tl_ep, uint8_t id, const void *header,
                         unsigned header_length, const uct_iov_t *iov,
                         size_t iovcnt, unsigned flags, uct_completion_t *comp)
{
    uct_ud_verbs_ep_t *ep = ucs_derived_of(tl_ep, uct_ud_verbs_ep_t);
    uct_ud_verbs_iface_t *iface = ucs_derived_of(tl_ep->iface,
                                                 uct_ud_verbs_iface_t);
    uct_ud_send_skb_t *skb;
    ucs_status_t status;

    UCT_CHECK_IOV_SIZE(iovcnt, (size_t)iface->config.max_send_sge,
                       "uct_ud_verbs_ep_am_zcopy");

    UCT_CHECK_LENGTH(sizeof(uct_ud_neth_t) + sizeof(uct_ud_zcopy_desc_t) + header_length,
                     0, iface->super.super.config.seg_size, "am_zcopy header");

    UCT_UD_CHECK_ZCOPY_LENGTH(&iface->super, header_length,
                              uct_iov_total_length(iov, iovcnt));

    uct_ud_enter(&iface->super);

    status = uct_ud_am_skb_common(&iface->super, &ep->super, id, &skb);
    if (status != UCS_OK) {
        uct_ud_leave(&iface->super);
        return status;
    }
    /* force ACK_REQ because we want to call user completion ASAP */
    skb->neth->packet_type |= UCT_UD_PACKET_FLAG_ACK_REQ;
    memcpy(skb->neth + 1, header, header_length);
    skb->len = sizeof(uct_ud_neth_t) + header_length;

    iface->tx.wr_skb.num_sge = uct_ib_verbs_sge_fill_iov(iface->tx.sge + 1,
                                                         iov, iovcnt) + 1;
    uct_ud_verbs_ep_tx_skb(iface, ep, skb, 0,
                           UCT_IB_MAX_ZCOPY_LOG_SGE(&iface->super.super));
    iface->tx.wr_skb.num_sge = 1;

    uct_ud_skb_set_zcopy_desc(skb, iov, iovcnt, comp);
    uct_ud_iface_complete_tx_skb(&iface->super, &ep->super, skb);
    UCT_TL_EP_STAT_OP(&ep->super.super, AM, ZCOPY, header_length +
                      uct_iov_total_length(iov, iovcnt));
    uct_ud_leave(&iface->super);
    return UCS_INPROGRESS;
}

static
ucs_status_t uct_ud_verbs_ep_put_short(uct_ep_h tl_ep,
                                       const void *buffer, unsigned length,
                                       uint64_t remote_addr, uct_rkey_t rkey)
{
    uct_ud_verbs_ep_t *ep = ucs_derived_of(tl_ep, uct_ud_verbs_ep_t);
    uct_ud_verbs_iface_t *iface = ucs_derived_of(tl_ep->iface,
                                                 uct_ud_verbs_iface_t);
    uct_ud_send_skb_t *skb;
    uct_ud_put_hdr_t *put_hdr;
    uct_ud_neth_t *neth;

    UCT_CHECK_LENGTH(sizeof(*neth) + sizeof(*put_hdr) + length,
                     0, iface->super.config.max_inline, "put_short");

    uct_ud_enter(&iface->super);

    skb = uct_ud_ep_get_tx_skb(&iface->super, &ep->super);
    if (!skb) {
        uct_ud_leave(&iface->super);
        return UCS_ERR_NO_RESOURCE;
    }

    neth = skb->neth;
    uct_ud_neth_set_packet_type(&ep->super, neth, 0, UCT_UD_PACKET_FLAG_PUT);
    uct_ud_neth_init_data(&ep->super, neth);
    uct_ud_neth_ack_req(&ep->super, neth);

    put_hdr = (uct_ud_put_hdr_t *)(neth+1);
    put_hdr->rva = remote_addr;
    iface->tx.sge[0].addr   = (uintptr_t)neth;
    iface->tx.sge[0].length = sizeof(*neth) + sizeof(*put_hdr);

    uct_ud_verbs_ep_tx_inlv(iface, ep, buffer, length);

    skb->len = iface->tx.sge[0].length;
    uct_ud_iface_complete_tx_inl(&iface->super, &ep->super, skb,
                                 put_hdr+1, buffer, length);
    UCT_TL_EP_STAT_OP(&ep->super.super, PUT, SHORT, length);
    uct_ud_leave(&iface->super);
    return UCS_OK;
}


static UCS_F_ALWAYS_INLINE unsigned
uct_ud_verbs_iface_poll_tx(uct_ud_verbs_iface_t *iface, int is_async)
{
    unsigned num_wcs =
            uct_ib_iface_device(&iface->super.super)->ordered_send_comp ?
                    1 :
                    iface->super.super.config.tx_max_poll;
    struct ibv_wc wc[num_wcs];
    unsigned num_completed;
    int i, ret;

    ret = ibv_poll_cq(iface->super.super.cq[UCT_IB_DIR_TX], num_wcs, wc);
    if (ucs_unlikely(ret < 0)) {
        ucs_fatal("Failed to poll send CQ");
        return 0;
    }

    if (ret == 0) {
        return 0;
    }

    UCS_STATS_UPDATE_COUNTER(iface->super.super.stats,
                             UCT_IB_IFACE_STAT_TX_COMPLETION, ret);

    for (i = 0; i < ret; i++) {
        if (ucs_unlikely(wc[i].status != IBV_WC_SUCCESS)) {
            ucs_fatal("Send completion (wr_id=0x%0X with error: %s ",
                      (unsigned)wc[i].wr_id, ibv_wc_status_str(wc[i].status));
            continue;
        }

        if (uct_ib_iface_device(&iface->super.super)->ordered_send_comp) {
            num_completed              = wc[i].wr_id + 1;
            iface->tx.comp_sn         += num_completed;
            iface->super.tx.available += num_completed;

            ucs_assertv(num_completed <= UCT_UD_TX_MODERATION,
                        "num_completed=%u", num_completed);

            uct_ud_iface_send_completion_ordered(&iface->super,
                                                 iface->tx.comp_sn, is_async);
        } else {
            iface->tx.comp_sn = wc[i].wr_id + 1;
            iface->super.tx.available++;

            uct_ud_iface_send_completion_unordered(&iface->super,
                                                   iface->tx.comp_sn, is_async);
        }
    }

    return 1;
}

static UCS_F_ALWAYS_INLINE size_t uct_ud_verbs_iface_get_gid_len(void *packet)
{
  /* The GRH will contain either an IPv4 or IPv6 header. If the former is
   * present the header will start at offset 20 in the buffer otherwise it
   * will start at offset 0. Since the two headers are of fixed size (20 or
   * 40 bytes) this means we will either see 0x6? at offset 0 (IPv6) or 0x45
   * at offset 20. The detection is a little tricky for IPv6 given that the
   * first 20B are undefined for IPv4. To overcome this the first byte of
   * the posted receive buffer is set to 0xff.
   */
  return ((((uint8_t*)packet)[0] & 0xf0) == 0x60) ? UCS_IPV6_ADDR_LEN :
                                                    UCS_IPV4_ADDR_LEN;
}

static UCS_F_ALWAYS_INLINE unsigned
uct_ud_verbs_iface_poll_rx(uct_ud_verbs_iface_t *iface, int is_async)
{
    unsigned num_wcs = iface->super.super.config.rx_max_poll;
    struct ibv_wc wc[num_wcs];
    ucs_status_t status;
    void *packet;
    int i;

    status = uct_ib_poll_cq(iface->super.super.cq[UCT_IB_DIR_RX], &num_wcs, wc);
    if (status != UCS_OK) {
        num_wcs = 0;
        goto out;
    }

    UCS_STATS_UPDATE_COUNTER(iface->super.super.stats,
                             UCT_IB_IFACE_STAT_RX_COMPLETION, num_wcs);

    UCT_IB_IFACE_VERBS_FOREACH_RXWQE(&iface->super.super, i, packet, wc, num_wcs) {
        if (!uct_ud_iface_check_grh(&iface->super, packet,
                                    wc[i].wc_flags & IBV_WC_GRH,
                                    uct_ud_verbs_iface_get_gid_len(packet))) {
            ucs_mpool_put_inline((void*)wc[i].wr_id);
            continue;
        }
        uct_ib_log_recv_completion(&iface->super.super, &wc[i], packet,
                                   wc[i].byte_len, uct_ud_dump_packet);
        uct_ud_ep_process_rx(&iface->super,
                             (uct_ud_neth_t *)UCS_PTR_BYTE_OFFSET(packet, UCT_IB_GRH_LEN),
                             wc[i].byte_len - UCT_IB_GRH_LEN,
                             (uct_ud_recv_skb_t *)wc[i].wr_id,
                             is_async);

    }
    iface->super.rx.available += num_wcs;
out:
    uct_ud_verbs_iface_post_recv(iface);
    return num_wcs;
}

static unsigned uct_ud_verbs_iface_async_progress(uct_ud_iface_t *ud_iface)
{
    uct_ud_verbs_iface_t *iface = ucs_derived_of(ud_iface, uct_ud_verbs_iface_t);
    unsigned count, n;

    count = 0;
    do {
        n = uct_ud_verbs_iface_poll_rx(iface, 1);
        count += n;
    } while ((n > 0) && (count < iface->super.rx.async_max_poll));

    count += uct_ud_verbs_iface_poll_tx(iface, 1);

    uct_ud_iface_progress_pending(&iface->super, 1);
    return count;
}

static unsigned uct_ud_verbs_iface_progress(uct_iface_h tl_iface)
{
    uct_ud_verbs_iface_t *iface = ucs_derived_of(tl_iface, uct_ud_verbs_iface_t);
    unsigned count;

    uct_ud_enter(&iface->super);

    count  = uct_ud_iface_dispatch_async_comps(&iface->super, NULL);
    count += uct_ud_iface_dispatch_pending_rx(&iface->super);

    if (ucs_likely(count == 0)) {
        count = uct_ud_verbs_iface_poll_rx(iface, 0);
        if (count == 0) {
            count += uct_ud_verbs_iface_poll_tx(iface, 0);
        }
    }

    uct_ud_iface_progress_pending(&iface->super, 0);
    uct_ud_leave(&iface->super);

    return count;
}

static ucs_status_t
uct_ud_verbs_iface_event_arm(uct_iface_h tl_iface, unsigned events)
{
    uct_ud_iface_t *iface = ucs_derived_of(tl_iface, uct_ud_iface_t);
    ucs_status_t status;
    uint64_t dirs;
    int dir;

    uct_ud_enter(iface);

    status = uct_ud_iface_event_arm_common(iface, events, &dirs);
    if (status != UCS_OK) {
        goto out;
    }

    ucs_for_each_bit(dir, dirs) {
        ucs_assert(dir < UCT_IB_DIR_LAST);
        status = uct_ib_iface_arm_cq(&iface->super, dir, 0);
        if (status != UCS_OK) {
            goto out;
        }
    }

    ucs_trace("iface %p: arm cq ok", iface);
out:
    uct_ud_leave(iface);
    return status;
}

static void uct_ud_verbs_iface_async_handler(int fd,
                                             ucs_event_set_types_t events,
                                             void *arg)
{
    uct_ud_iface_t *iface = arg;

    uct_ud_iface_async_progress(iface);

    /* arm for new solicited events
     * if user asks to provide notifications for all completion
     * events by calling uct_iface_event_arm(), RX CQ will be
     * armed again with solicited flag = 0 */
    uct_ib_iface_pre_arm(&iface->super);
    uct_ib_iface_arm_cq(&iface->super, UCT_IB_DIR_RX, 1);

    ucs_assert(iface->async.event_cb != NULL);
    /* notify user */
    iface->async.event_cb(iface->async.event_arg, 0);
}

static ucs_status_t
uct_ud_verbs_iface_query(uct_iface_h tl_iface, uct_iface_attr_t *iface_attr)
{
    uct_ud_verbs_iface_t *iface = ucs_derived_of(tl_iface, uct_ud_verbs_iface_t);
    size_t am_max_hdr;
    ucs_status_t status;

    ucs_trace_func("");

    am_max_hdr = uct_ib_iface_hdr_size(iface->super.super.config.seg_size,
                                       sizeof(uct_ud_neth_t) +
                                       sizeof(uct_ud_zcopy_desc_t));
    status     = uct_ud_iface_query(&iface->super, iface_attr,
                                    iface->config.max_send_sge, am_max_hdr);
    if (status != UCS_OK) {
        return status;
    }

    iface_attr->overhead = UCT_UD_VERBS_IFACE_OVERHEAD; /* Software overhead */

    return UCS_OK;
}

static ucs_status_t
uct_ud_verbs_iface_unpack_peer_address(uct_ud_iface_t *iface,
                                       const uct_ib_address_t *ib_addr,
                                       const uct_ud_iface_addr_t *if_addr,
                                       int path_index, void *address_p)
{
    uct_ib_iface_t *ib_iface                     = &iface->super;
    uct_ud_verbs_ep_peer_address_t *peer_address =
        (uct_ud_verbs_ep_peer_address_t*)address_p;
    struct ibv_ah_attr ah_attr;
    enum ibv_mtu path_mtu;
    ucs_status_t status;

    memset(peer_address, 0, sizeof(*peer_address));

    uct_ib_iface_fill_ah_attr_from_addr(ib_iface, ib_addr, path_index,
                                        &ah_attr, &path_mtu);
    status = uct_ib_iface_create_ah(ib_iface, &ah_attr, "UD verbs connect",
                                    &peer_address->ah);
    if (status != UCS_OK) {
        return status;
    }

    peer_address->dest_qpn = uct_ib_unpack_uint24(if_addr->qp_num);

    return UCS_OK;
}

static void *uct_ud_verbs_ep_get_peer_address(uct_ud_ep_t *ud_ep)
{
    uct_ud_verbs_ep_t *ep = ucs_derived_of(ud_ep, uct_ud_verbs_ep_t);
    return &ep->peer_address;
}

int uct_ud_verbs_ep_is_connected(const uct_ep_h tl_ep,
                                 const uct_ep_is_connected_params_t *params)
{
    uct_ud_verbs_ep_t *ep     = ucs_derived_of(tl_ep, uct_ud_verbs_ep_t);
    uct_ib_iface_t *ib_iface  = ucs_derived_of(tl_ep->iface, uct_ib_iface_t);
    uct_ib_address_t *ib_addr = (uct_ib_address_t*)params->device_addr;

    if (!uct_ud_ep_is_connected_to_addr(&ep->super, params,
                                        ep->peer_address.dest_qpn)) {
        return 0;
    }

    return uct_ib_iface_is_connected(ib_iface, ib_addr, ep->super.path_index,
                                     ep->peer_address.ah);
}

static size_t uct_ud_verbs_get_peer_address_length()
{
    return sizeof(uct_ud_verbs_ep_peer_address_t);
}

static const char*
uct_ud_verbs_iface_peer_address_str(const uct_ud_iface_t *iface,
                                    const void *address,
                                    char *str, size_t max_size)
{
    const uct_ud_verbs_ep_peer_address_t *peer_address =
        (const uct_ud_verbs_ep_peer_address_t*)address;

    ucs_snprintf_zero(str, max_size, "ah=%p dest_qpn=%u",
                      peer_address->ah, peer_address->dest_qpn);
    return str;
}

static void uct_ud_verbs_iface_destroy_qp(uct_ud_iface_t *ud_iface)
{
    uct_ib_destroy_qp(ud_iface->qp);
}

static void UCS_CLASS_DELETE_FUNC_NAME(uct_ud_verbs_iface_t)(uct_iface_t*);

static uct_ud_iface_ops_t uct_ud_verbs_iface_ops = {
    .super = {
        .super = {
            .iface_estimate_perf   = uct_ib_iface_estimate_perf,
            .iface_vfs_refresh     = (uct_iface_vfs_refresh_func_t)uct_ud_iface_vfs_refresh,
            .ep_query              = (uct_ep_query_func_t)ucs_empty_function_return_unsupported,
            .ep_invalidate         = uct_ud_ep_invalidate,
            .ep_connect_to_ep_v2   = uct_ud_ep_connect_to_ep_v2,
            .iface_is_reachable_v2 = uct_ib_iface_is_reachable_v2,
            .ep_is_connected       = uct_ud_verbs_ep_is_connected
        },
        .create_cq      = uct_ib_verbs_create_cq,
        .destroy_cq     = uct_ib_verbs_destroy_cq,
        .event_cq       = (uct_ib_iface_event_cq_func_t)ucs_empty_function,
        .handle_failure = (uct_ib_iface_handle_failure_func_t)ucs_empty_function_do_assert,
    },
    .async_progress          = uct_ud_verbs_iface_async_progress,
    .send_ctl                = uct_ud_verbs_ep_send_ctl,
    .ep_new                  = uct_ud_verbs_ep_t_new,
    .ep_free                 = UCS_CLASS_DELETE_FUNC_NAME(uct_ud_verbs_ep_t),
    .create_qp               = uct_ib_iface_create_qp,
    .destroy_qp              = uct_ud_verbs_iface_destroy_qp,
    .unpack_peer_address     = uct_ud_verbs_iface_unpack_peer_address,
    .ep_get_peer_address     = uct_ud_verbs_ep_get_peer_address,
    .get_peer_address_length = uct_ud_verbs_get_peer_address_length,
    .peer_address_str        = uct_ud_verbs_iface_peer_address_str,
};

static uct_iface_ops_t uct_ud_verbs_iface_tl_ops = {
    .ep_put_short             = uct_ud_verbs_ep_put_short,
    .ep_am_short              = uct_ud_verbs_ep_am_short,
    .ep_am_short_iov          = uct_ud_verbs_ep_am_short_iov,
    .ep_am_bcopy              = uct_ud_verbs_ep_am_bcopy,
    .ep_am_zcopy              = uct_ud_verbs_ep_am_zcopy,
    .ep_pending_add           = uct_ud_ep_pending_add,
    .ep_pending_purge         = uct_ud_ep_pending_purge,
    .ep_flush                 = uct_ud_ep_flush,
    .ep_fence                 = uct_base_ep_fence,
    .ep_check                 = uct_ud_ep_check,
    .ep_create                = uct_ud_ep_create,
    .ep_destroy               = uct_ud_ep_disconnect,
    .ep_get_address           = uct_ud_ep_get_address,
    .ep_connect_to_ep         = uct_base_ep_connect_to_ep,
    .iface_flush              = uct_ud_iface_flush,
    .iface_fence              = uct_base_iface_fence,
    .iface_progress_enable    = uct_ud_iface_progress_enable,
    .iface_progress_disable   = uct_ud_iface_progress_disable,
    .iface_progress           = uct_ud_verbs_iface_progress,
    .iface_event_fd_get       = (uct_iface_event_fd_get_func_t)
                                ucs_empty_function_return_unsupported,
    .iface_event_arm          = uct_ud_verbs_iface_event_arm,
    .iface_close              = UCS_CLASS_DELETE_FUNC_NAME(uct_ud_verbs_iface_t),
    .iface_query              = uct_ud_verbs_iface_query,
    .iface_get_device_address = uct_ib_iface_get_device_address,
    .iface_get_address        = uct_ud_iface_get_address,
    .iface_is_reachable       = uct_base_iface_is_reachable
};

static UCS_F_NOINLINE void
uct_ud_verbs_iface_post_recv_always(uct_ud_verbs_iface_t *iface, int max)
{
    struct ibv_recv_wr *bad_wr;
    uct_ib_recv_wr_t *wrs;
    unsigned count;
    int ret, i;

    wrs  = ucs_alloca(sizeof *wrs  * max);

    count = uct_ib_iface_prepare_rx_wrs(&iface->super.super, &iface->super.rx.mp,
                                        wrs, max);
    if (count == 0) {
        return;
    }

    /* Set the first byte in the receive buffer grh to a known value not equal to
     * 0x6?. This should aid in the detection of IPv6 vs IPv4 because the first
     * byte is undefined in the later and 0x6? in the former. It is unlikely
     * this byte is touched with IPv4. */
    for (i = 0; i < count; ++i) {
        ((uint8_t*)wrs[i].sg.addr)[0] = 0xff;
    }

    ret = ibv_post_recv(iface->super.qp, &wrs[0].ibwr, &bad_wr);
    if (ret != 0) {
        ucs_fatal("ibv_post_recv() returned %d: %m", ret);
    }
    iface->super.rx.available -= count;
}

static UCS_F_ALWAYS_INLINE void
uct_ud_verbs_iface_post_recv(uct_ud_verbs_iface_t *iface)
{
    unsigned batch = iface->super.super.config.rx_max_batch;

    if (iface->super.rx.available < batch)
        return;

    uct_ud_verbs_iface_post_recv_always(iface, batch);
}

/* Used for am zcopy only */
ucs_status_t uct_ud_verbs_qp_max_send_sge(uct_ud_verbs_iface_t *iface,
                                          size_t *max_send_sge)
{
    uint32_t max_sge;
    ucs_status_t status;

    status = uct_ib_qp_max_send_sge(iface->super.qp, &max_sge);
    if (status != UCS_OK) {
        return status;
    }

    /* need to reserve 1 iov for am zcopy header */
    ucs_assert_always(max_sge > 1);

    *max_send_sge = ucs_min(max_sge - 1, UCT_IB_MAX_IOV);

    return UCS_OK;
}

void uct_ud_send_wr_init(struct ibv_send_wr *wr, struct ibv_sge *sge,
                         int is_inline)
{
    memset(wr, 0, sizeof(*wr));

    wr->opcode            = IBV_WR_SEND;
    wr->wr.ud.remote_qkey = UCT_IB_KEY;
    wr->imm_data          = 0;
    wr->next              = 0;
    wr->sg_list           = sge;

    if (is_inline) {
        wr->wr_id   = 0xBEEBBEEB;
    } else {
        wr->wr_id   = 0xFAAFFAAF;
        wr->num_sge = 1;
    }
}

static UCS_CLASS_INIT_FUNC(uct_ud_verbs_iface_t, uct_md_h md, uct_worker_h worker,
                           const uct_iface_params_t *params,
                           const uct_iface_config_t *tl_config)
{
    uct_ud_iface_config_t *config      = ucs_derived_of(tl_config,
                                                        uct_ud_iface_config_t);
    uct_ib_iface_init_attr_t init_attr = {};
    ucs_status_t status;

    ucs_trace_func("");

    init_attr.cq_len[UCT_IB_DIR_TX] = config->super.tx.queue_len;
    init_attr.cq_len[UCT_IB_DIR_RX] = config->super.rx.queue_len;

    UCS_CLASS_CALL_SUPER_INIT(uct_ud_iface_t, &uct_ud_verbs_iface_ops, &uct_ud_verbs_iface_tl_ops, md,
                              worker, params, config, &init_attr);

    self->super.super.config.sl = uct_ib_iface_config_select_sl(&config->super);
    uct_ib_iface_set_reverse_sl(&self->super.super, &config->super);

    uct_ud_send_wr_init(&self->tx.wr_inl, self->tx.sge, 1);
    uct_ud_send_wr_init(&self->tx.wr_skb, self->tx.sge, 0);

    self->tx.send_sn                  = 0;
    self->tx.comp_sn                  = 0;

    if (self->super.super.config.rx_max_batch < UCT_UD_RX_BATCH_MIN) {
        ucs_warn("rx max batch is too low (%d < %d), performance may be impacted",
                self->super.super.config.rx_max_batch,
                UCT_UD_RX_BATCH_MIN);
    }

    status = uct_ud_verbs_qp_max_send_sge(self, &self->config.max_send_sge);
    if (status != UCS_OK) {
        return status;
    }

    while (self->super.rx.available >= self->super.super.config.rx_max_batch) {
        uct_ud_verbs_iface_post_recv(self);
    }

    status = uct_ud_iface_complete_init(&self->super);
    if (status != UCS_OK) {
        return status;
    }

    if ((self->super.async.event_cb != NULL) &&
        uct_ib_iface_device(&self->super.super)->req_notify_cq_support) {
        status = uct_ud_iface_set_event_cb(&self->super,
                                           uct_ud_verbs_iface_async_handler);
        if (status != UCS_OK) {
            return status;
        }

        status = uct_ib_iface_arm_cq(&self->super.super, UCT_IB_DIR_RX, 1);
    }

    return status;
}

static UCS_CLASS_CLEANUP_FUNC(uct_ud_verbs_iface_t)
{
    ucs_trace_func("");
}

UCS_CLASS_DEFINE(uct_ud_verbs_iface_t, uct_ud_iface_t);

static UCS_CLASS_DEFINE_NEW_FUNC(uct_ud_verbs_iface_t, uct_iface_t, uct_md_h,
                                 uct_worker_h, const uct_iface_params_t*,
                                 const uct_iface_config_t*);

static UCS_CLASS_DEFINE_DELETE_FUNC(uct_ud_verbs_iface_t, uct_iface_t);

static ucs_status_t
uct_ud_verbs_query_tl_devices(uct_md_h md,
                              uct_tl_device_resource_t **tl_devices_p,
                              unsigned *num_tl_devices_p)
{
    uct_ib_md_t *ib_md = ucs_derived_of(md, uct_ib_md_t);
    return uct_ib_device_query_ports(&ib_md->dev, 0, tl_devices_p,
                                     num_tl_devices_p);
}

UCT_TL_DEFINE_ENTRY(&uct_ib_component, ud_verbs, uct_ud_verbs_query_tl_devices,
                    uct_ud_verbs_iface_t, "UD_VERBS_",
                    uct_ud_verbs_iface_config_table, uct_ud_iface_config_t);
