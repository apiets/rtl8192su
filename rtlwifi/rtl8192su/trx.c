/******************************************************************************
 *
 * Copyright(c) 2009-2012  Realtek Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110, USA
 *
 * The full GNU General Public License is included in this distribution in the
 * file called LICENSE.
 *
 * Contact Information:
 * wlanfae <wlanfae@realtek.com>
 * Realtek Corporation, No. 2, Innovation Road II, Hsinchu Science Park,
 * Hsinchu 300, Taiwan.
 *
 * Larry Finger <Larry.Finger@lwfinger.net>
 *
 *****************************************************************************/

#include "../wifi.h"
#include "../usb.h"
#include "../ps.h"
#include "../base.h"
#include "reg.h"
#include "def.h"
#include "phy.h"
#include "fw.h"
#include "trx.h"
#include "led.h"
#include "rf.h"
#include "dm.h"

/* endpoint mapping */
int rtl8192su_endpoint_mapping(struct ieee80211_hw *hw)
{
	struct rtl_usb_priv *usb_priv = rtl_usbpriv(hw);
	struct rtl_usb *rtlusb = rtl_usbdev(usb_priv);
	struct rtl_ep_map *ep_map = &(rtlusb->ep_map);

	/*
	 * The vendor driver uses the same EP when TX aggregation
	 * is enabled. Since we can't change the EPs "on demand",
	 * we always presume that it is enabled.
	 */
	ep_map->ep_mapping[RTL_TXQ_BK]	= 0x0d;
	ep_map->ep_mapping[RTL_TXQ_BE]	= 0x0d;
	ep_map->ep_mapping[RTL_TXQ_VI]	= 0x0d;
	ep_map->ep_mapping[RTL_TXQ_VO]	= 0x0d;
	ep_map->ep_mapping[RTL_TXQ_MGT] = 0x0d;
	ep_map->ep_mapping[RTL_TXQ_BCN] = 0x0d;
	ep_map->ep_mapping[RTL_TXQ_HI]	= 0x0d;
	return 0;
}

u16 rtl8192su_mq_to_hwq(__le16 fc, u16 mac80211_queue_index)
{
	u16 hw_queue_index;

	if (unlikely(ieee80211_is_beacon(fc))) {
		hw_queue_index = RTL_TXQ_BCN;
		goto out;
	}
	if (ieee80211_is_mgmt(fc)) {
		hw_queue_index = RTL_TXQ_MGT;
		goto out;
	}
	switch (mac80211_queue_index) {
	case 0:
		hw_queue_index = RTL_TXQ_VO;
		break;
	case 1:
		hw_queue_index = RTL_TXQ_VI;
		break;
	case 2:
		hw_queue_index = RTL_TXQ_BE;
		break;
	case 3:
		hw_queue_index = RTL_TXQ_BK;
		break;
	default:
		hw_queue_index = RTL_TXQ_BE;
		RT_ASSERT(false, "QSLT_BE queue, skb_queue:%d\n",
			  mac80211_queue_index);
		break;
	}
out:
	return hw_queue_index;
}

static u8 _rtl92se_map_hwqueue_to_fwqueue(struct sk_buff *skb, u8 skb_queue)
{
	__le16 fc = rtl_get_fc(skb);

	return 0;

	if (unlikely(ieee80211_is_beacon(fc)))
		return QSLT_BEACON;
	if (ieee80211_is_mgmt(fc))
		return QSLT_MGNT;
	if (ieee80211_is_nullfunc(fc))
		return QSLT_HIGH;

	return skb_get_queue_mapping(skb);
}

#if 0
/* rtl8712_recv.c query_rx_pwr_percentage */
static u8 _rtl92s_query_rxpwrpercentage(char antpower)
{
	if ((antpower <= -100) || (antpower >= 20))
		return 0;
	else if (antpower >= 0)
		return 100;
	else
		return 100 + antpower;
}

/* rtl8712_recv.c evm_db2percentage */
static u8 _rtl92s_evm_db_to_percentage(char value)
{
	char ret_val;
	ret_val = value;

	if (ret_val >= 0)
		ret_val = 0;

	if (ret_val <= -33)
		ret_val = -33;

	ret_val = 0 - ret_val;
	ret_val *= 3;

	if (ret_val == 99)
		ret_val = 100;

	return ret_val;
}

static long _rtl92se_translate_todbm(struct ieee80211_hw *hw,
				     u8 signal_strength_index)
{
	long signal_power;

	signal_power = (long)((signal_strength_index + 1) >> 1);
	signal_power -= 95;
	return signal_power;
}

/* rtl8712_recv.c r8712_signal_scale_mapping */
static long _rtl92se_signal_scale_mapping(struct ieee80211_hw *hw,
		long currsig)
{
	long retsig = 0;

	/* Step 1. Scale mapping. */
	if (currsig > 47)
		retsig = 100;
	else if (currsig > 14 && currsig <= 47)
		retsig = 100 - ((47 - currsig) * 3) / 2;
	else if (currsig > 2 && currsig <= 14)
		retsig = 48 - ((14 - currsig) * 15) / 7;
	else if (currsig >= 0)
		retsig = currsig * 9 + 1;

	return retsig;
}

/* rtl8712_recv.c query_rx_phy_status */
static void _rtl92se_query_rxphystatus(struct ieee80211_hw *hw,
				       struct rtl_stats *pstats, u8 *pdesc,
				       struct rx_fwinfo *p_drvinfo,
				       bool packet_match_bssid,
				       bool packet_toself,
				       bool packet_beacon)
{
	struct rtl_priv *rtlpriv = rtl_priv(hw);
	struct phy_sts_cck_8192s_t *cck_buf;
	s8 rx_pwr_all = 0, rx_pwr[4];
	u8 rf_rx_num = 0, evm, pwdb_all;
	u8 i, max_spatial_stream;
	u32 rssi, total_rssi = 0;
	bool in_powersavemode = false;
	bool is_cck = pstats->is_cck;

	pstats->packet_matchbssid = packet_match_bssid;
	pstats->packet_toself = packet_toself;
	pstats->packet_beacon = packet_beacon;
	pstats->rx_mimo_signalquality[0] = -1;
	pstats->rx_mimo_signalquality[1] = -1;

	if (is_cck) {
		u8 report, cck_highpwr;
		cck_buf = (struct phy_sts_cck_8192s_t *)p_drvinfo;

		if (!in_powersavemode)
			cck_highpwr = (u8) rtl_get_bbreg(hw,
						RFPGA0_XA_HSSIPARAMETER2,
						0x200);
		else
			cck_highpwr = false;

		if (!cck_highpwr) {
			u8 cck_agc_rpt = cck_buf->cck_agc_rpt;
			report = cck_buf->cck_agc_rpt & 0xc0;
			report = report >> 6;
			switch (report) {
			case 0x3:
				rx_pwr_all = -40 - (cck_agc_rpt & 0x3e);
				break;
			case 0x2:
				rx_pwr_all = -20 - (cck_agc_rpt & 0x3e);
				break;
			case 0x1:
				rx_pwr_all = -2 - (cck_agc_rpt & 0x3e);
				break;
			case 0x0:
				rx_pwr_all = 14 - (cck_agc_rpt & 0x3e);
				break;
			}
		} else {
			u8 cck_agc_rpt = cck_buf->cck_agc_rpt;
			report = p_drvinfo->cfosho[0] & 0x60;
			report = report >> 5;
			switch (report) {
			case 0x3:
				rx_pwr_all = -40 - ((cck_agc_rpt & 0x1f) << 1);
				break;
			case 0x2:
				rx_pwr_all = -20 - ((cck_agc_rpt & 0x1f) << 1);
				break;
			case 0x1:
				rx_pwr_all = -2 - ((cck_agc_rpt & 0x1f) << 1);
				break;
			case 0x0:
				rx_pwr_all = 14 - ((cck_agc_rpt & 0x1f) << 1);
				break;
			}
		}

		pwdb_all = _rtl92s_query_rxpwrpercentage(rx_pwr_all);

		/* CCK gain is smaller than OFDM/MCS gain,  */
		/* so we add gain diff by experiences, the val is 6 */
		pwdb_all += 6;
		if (pwdb_all > 100)
			pwdb_all = 100;
		/* modify the offset to make the same gain index with OFDM. */
		if (pwdb_all > 34 && pwdb_all <= 42)
			pwdb_all -= 2;
		else if (pwdb_all > 26 && pwdb_all <= 34)
			pwdb_all -= 6;
		else if (pwdb_all > 14 && pwdb_all <= 26)
			pwdb_all -= 8;
		else if (pwdb_all > 4 && pwdb_all <= 14)
			pwdb_all -= 4;

		pstats->rx_pwdb_all = pwdb_all;
		pstats->recvsignalpower = rx_pwr_all;

		if (packet_match_bssid) {
			u8 sq;
			if (pstats->rx_pwdb_all > 40) {
				sq = 100;
			} else {
				sq = cck_buf->sq_rpt;
				if (sq > 64)
					sq = 0;
				else if (sq < 20)
					sq = 100;
				else
					sq = ((64 - sq) * 100) / 44;
			}

			pstats->signalquality = sq;
			pstats->rx_mimo_signalquality[0] = sq;
			pstats->rx_mimo_signalquality[1] = -1;
		}
	} else {
		rtlpriv->dm.rfpath_rxenable[0] =
		    rtlpriv->dm.rfpath_rxenable[1] = true;
		for (i = RF90_PATH_A; i < RF90_PATH_MAX; i++) {
			if (rtlpriv->dm.rfpath_rxenable[i])
				rf_rx_num++;

			rx_pwr[i] = ((p_drvinfo->gain_trsw[i] &
				    0x3f) * 2) - 110;
			rssi = _rtl92s_query_rxpwrpercentage(rx_pwr[i]);
			total_rssi += rssi;
			rtlpriv->stats.rx_snr_db[i] =
					 (long)(p_drvinfo->rxsnr[i] / 2);

			if (packet_match_bssid)
				pstats->rx_mimo_signalstrength[i] = (u8) rssi;
		}

		rx_pwr_all = ((p_drvinfo->pwdb_all >> 1) & 0x7f) - 110;
		pwdb_all = _rtl92s_query_rxpwrpercentage(rx_pwr_all);
		pstats->rx_pwdb_all = pwdb_all;
		pstats->rxpower = rx_pwr_all;
		pstats->recvsignalpower = rx_pwr_all;

		if (pstats->is_ht && pstats->rate >= DESC92_RATEMCS8 &&
		    pstats->rate <= DESC92_RATEMCS15)
			max_spatial_stream = 2;
		else
			max_spatial_stream = 1;

		for (i = 0; i < max_spatial_stream; i++) {
			evm = _rtl92s_evm_db_to_percentage(p_drvinfo->rxevm[i]);

			if (packet_match_bssid) {
				if (i == 0)
					pstats->signalquality = (u8)(evm &
								 0xff);
				pstats->rx_mimo_signalquality[i] =
							 (u8) (evm & 0xff);
			}
		}
	}

	if (is_cck)
		pstats->signalstrength = (u8)(_rtl92se_signal_scale_mapping(hw,
					 pwdb_all));
	else if (rf_rx_num != 0)
		pstats->signalstrength = (u8) (_rtl92se_signal_scale_mapping(hw,
				total_rssi /= rf_rx_num));
}

/* rtl8712_recv.c process_rssi */
static void _rtl92se_process_ui_rssi(struct ieee80211_hw *hw,
				     struct rtl_stats *pstats)
{
	struct rtl_priv *rtlpriv = rtl_priv(hw);
	struct rtl_phy *rtlphy = &(rtlpriv->phy);
	u8 rfpath;
	u32 last_rssi, tmpval;

	if (pstats->packet_toself || pstats->packet_beacon) {
		rtlpriv->stats.rssi_calculate_cnt++;

		if (rtlpriv->stats.ui_rssi.total_num++ >=
		    PHY_RSSI_SLID_WIN_MAX) {
			rtlpriv->stats.ui_rssi.total_num =
					 PHY_RSSI_SLID_WIN_MAX;
			last_rssi = rtlpriv->stats.ui_rssi.elements[
				rtlpriv->stats.ui_rssi.index];
			rtlpriv->stats.ui_rssi.total_val -= last_rssi;
		}

		rtlpriv->stats.ui_rssi.total_val += pstats->signalstrength;
		rtlpriv->stats.ui_rssi.elements[rtlpriv->stats.ui_rssi.index++]
			 = pstats->signalstrength;

		if (rtlpriv->stats.ui_rssi.index >= PHY_RSSI_SLID_WIN_MAX)
			rtlpriv->stats.ui_rssi.index = 0;

		tmpval = rtlpriv->stats.ui_rssi.total_val /
			rtlpriv->stats.ui_rssi.total_num;
		rtlpriv->stats.signal_strength = _rtl92se_translate_todbm(hw,
								(u8) tmpval);
		pstats->rssi = rtlpriv->stats.signal_strength;
	}

	if (!pstats->is_cck && pstats->packet_toself) {
		for (rfpath = RF90_PATH_A; rfpath < rtlphy->num_total_rfpath;
		     rfpath++) {
			if (rtlpriv->stats.rx_rssi_percentage[rfpath] == 0) {
				rtlpriv->stats.rx_rssi_percentage[rfpath] =
				    pstats->rx_mimo_signalstrength[rfpath];

			}

			if (pstats->rx_mimo_signalstrength[rfpath] >
			    rtlpriv->stats.rx_rssi_percentage[rfpath]) {
				rtlpriv->stats.rx_rssi_percentage[rfpath] =
				    ((rtlpriv->stats.rx_rssi_percentage[rfpath]
				    * (RX_SMOOTH_FACTOR - 1)) +
				    (pstats->rx_mimo_signalstrength[rfpath])) /
				    (RX_SMOOTH_FACTOR);

				rtlpriv->stats.rx_rssi_percentage[rfpath] =
				    rtlpriv->stats.rx_rssi_percentage[rfpath]
				    + 1;
			} else {
				rtlpriv->stats.rx_rssi_percentage[rfpath] =
				    ((rtlpriv->stats.rx_rssi_percentage[rfpath]
				    * (RX_SMOOTH_FACTOR - 1)) +
				    (pstats->rx_mimo_signalstrength[rfpath])) /
				    (RX_SMOOTH_FACTOR);
			}

		}
	}
}

static void _rtl92se_update_rxsignalstatistics(struct ieee80211_hw *hw,
					       struct rtl_stats *pstats)
{
	struct rtl_priv *rtlpriv = rtl_priv(hw);
	int weighting = 0;

	if (rtlpriv->stats.recv_signal_power == 0)
		rtlpriv->stats.recv_signal_power = pstats->recvsignalpower;

	if (pstats->recvsignalpower > rtlpriv->stats.recv_signal_power)
		weighting = 5;
	else if (pstats->recvsignalpower < rtlpriv->stats.recv_signal_power)
		weighting = (-5);

	rtlpriv->stats.recv_signal_power = (rtlpriv->stats.recv_signal_power * 5
					   + pstats->recvsignalpower +
					   weighting) / 6;
}

static void _rtl92se_process_pwdb(struct ieee80211_hw *hw,
				  struct rtl_stats *pstats)
{
	struct rtl_priv *rtlpriv = rtl_priv(hw);
	struct rtl_mac *mac = rtl_mac(rtl_priv(hw));
	long undec_sm_pwdb = 0;

	if (mac->opmode == NL80211_IFTYPE_ADHOC) {
		return;
	} else {
		undec_sm_pwdb =
		    rtlpriv->dm.undecorated_smoothed_pwdb;
	}

	if (pstats->packet_toself || pstats->packet_beacon) {
		if (undec_sm_pwdb < 0)
			undec_sm_pwdb = pstats->rx_pwdb_all;

		if (pstats->rx_pwdb_all > (u32) undec_sm_pwdb) {
			undec_sm_pwdb =
			    (((undec_sm_pwdb) *
			    (RX_SMOOTH_FACTOR - 1)) +
			    (pstats->rx_pwdb_all)) / (RX_SMOOTH_FACTOR);

			undec_sm_pwdb = undec_sm_pwdb + 1;
		} else {
			undec_sm_pwdb = (((undec_sm_pwdb) *
			      (RX_SMOOTH_FACTOR - 1)) + (pstats->rx_pwdb_all)) /
			      (RX_SMOOTH_FACTOR);
		}

		rtlpriv->dm.undecorated_smoothed_pwdb = undec_sm_pwdb;
		_rtl92se_update_rxsignalstatistics(hw, pstats);
	}
}

static void rtl_92s_process_streams(struct ieee80211_hw *hw,
				    struct rtl_stats *pstats)
{
	struct rtl_priv *rtlpriv = rtl_priv(hw);
	u32 stream;

	for (stream = 0; stream < 2; stream++) {
		if (pstats->rx_mimo_signalquality[stream] != -1) {
			if (rtlpriv->stats.rx_evm_percentage[stream] == 0) {
				rtlpriv->stats.rx_evm_percentage[stream] =
				    pstats->rx_mimo_signalquality[stream];
			}

			rtlpriv->stats.rx_evm_percentage[stream] =
			    ((rtlpriv->stats.rx_evm_percentage[stream] *
					(RX_SMOOTH_FACTOR - 1)) +
			     (pstats->rx_mimo_signalquality[stream] *
					1)) / (RX_SMOOTH_FACTOR);
		}
	}
}

/* rtl8712_recv.c process_link_qual */
static void _rtl92se_process_ui_link_quality(struct ieee80211_hw *hw,
					     struct rtl_stats *pstats)
{
	struct rtl_priv *rtlpriv = rtl_priv(hw);
	u32 last_evm = 0, tmpval;

	if (pstats->signalquality != 0) {
		if (pstats->packet_toself || pstats->packet_beacon) {

			if (rtlpriv->stats.ui_link_quality.total_num++ >=
			    PHY_LINKQUALITY_SLID_WIN_MAX) {
				rtlpriv->stats.ui_link_quality.total_num =
				    PHY_LINKQUALITY_SLID_WIN_MAX;
				last_evm =
				    rtlpriv->stats.ui_link_quality.elements[
				    rtlpriv->stats.ui_link_quality.index];
				rtlpriv->stats.ui_link_quality.total_val -=
				    last_evm;
			}

			rtlpriv->stats.ui_link_quality.total_val +=
			    pstats->signalquality;
			rtlpriv->stats.ui_link_quality.elements[
				rtlpriv->stats.ui_link_quality.index++] =
			    pstats->signalquality;

			if (rtlpriv->stats.ui_link_quality.index >=
			    PHY_LINKQUALITY_SLID_WIN_MAX)
				rtlpriv->stats.ui_link_quality.index = 0;

			tmpval = rtlpriv->stats.ui_link_quality.total_val /
			    rtlpriv->stats.ui_link_quality.total_num;
			rtlpriv->stats.signal_quality = tmpval;

			rtlpriv->stats.last_sigstrength_inpercent = tmpval;

			rtl_92s_process_streams(hw, pstats);

		}
	}
}

/* rtl8712_recv.c process_phy_info */
static void _rtl92se_process_phyinfo(struct ieee80211_hw *hw,
				     u8 *buffer,
				     struct rtl_stats *pcurrent_stats)
{

	if (!pcurrent_stats->packet_matchbssid &&
	    !pcurrent_stats->packet_beacon)
		return;

	_rtl92se_process_ui_rssi(hw, pcurrent_stats);
	_rtl92se_process_pwdb(hw, pcurrent_stats);
	_rtl92se_process_ui_link_quality(hw, pcurrent_stats);
}

static void _rtl92se_translate_rx_signal_stuff(struct ieee80211_hw *hw,
		struct sk_buff *skb, struct rtl_stats *pstats,
		u8 *pdesc, struct rx_fwinfo *p_drvinfo)
{
	struct rtl_mac *mac = rtl_mac(rtl_priv(hw));
	struct rtl_efuse *rtlefuse = rtl_efuse(rtl_priv(hw));

	struct ieee80211_hdr *hdr;
	u8 *tmp_buf;
	u8 *praddr;
	__le16 fc;
	u16 type, cfc;
	bool packet_matchbssid, packet_toself, packet_beacon;

	tmp_buf = skb->data + pstats->rx_drvinfo_size + pstats->rx_bufshift;

	hdr = (struct ieee80211_hdr *)tmp_buf;
	fc = hdr->frame_control;
	cfc = le16_to_cpu(fc);
	type = WLAN_FC_GET_TYPE(fc);
	praddr = hdr->addr1;

	packet_matchbssid = ((IEEE80211_FTYPE_CTL != type) &&
	     (!compare_ether_addr(mac->bssid, (cfc & IEEE80211_FCTL_TODS) ?
			hdr->addr1 : (cfc & IEEE80211_FCTL_FROMDS) ?
			hdr->addr2 : hdr->addr3)) && (!pstats->hwerror) &&
			(!pstats->crc) && (!pstats->icv));

	packet_toself = packet_matchbssid &&
	    (!compare_ether_addr(praddr, rtlefuse->dev_addr));

	if (ieee80211_is_beacon(fc))
		packet_beacon = true;

	_rtl92se_query_rxphystatus(hw, pstats, pdesc, p_drvinfo,
			packet_matchbssid, packet_toself, packet_beacon);
	_rtl92se_process_phyinfo(hw, tmp_buf, pstats);
}
#endif

/* r8712_signal_scale_mapping from rtl8712_recv.c */
static s32 rtl92s_signal_scale_mapping(s32 cur_sig)
{
	s32 ret_sig;

	if (cur_sig >= 51 && cur_sig <= 100)
		ret_sig = 100;
	else if (cur_sig >= 41 && cur_sig <= 50)
		ret_sig = 80 + ((cur_sig - 40) * 2);
	else if (cur_sig >= 31 && cur_sig <= 40)
		ret_sig = 66 + (cur_sig - 30);
	else if (cur_sig >= 21 && cur_sig <= 30)
		ret_sig = 54 + (cur_sig - 20);
	else if (cur_sig >= 10 && cur_sig <= 20)
		ret_sig = 42 + (((cur_sig - 10) * 2) / 3);
	else if (cur_sig >= 5 && cur_sig <= 9)
		ret_sig = 22 + (((cur_sig - 5) * 3) / 2);
	else if (cur_sig >= 1 && cur_sig <= 4)
		ret_sig = 6 + (((cur_sig - 1) * 3) / 2);
	else
		ret_sig = cur_sig;
	return ret_sig;
}

static void rtl92su_c2h_event(struct ieee80211_hw *hw, u8 *pdesc)
{
	struct rtl_priv *rtlpriv = rtl_priv(hw);
	u16 len;
	u8 evnum, seq;
	u8 *data;
	bool frag;

	len = (u16)SHIFT_AND_MASK_LE(pdesc, 0, 16);
	evnum = (u8)SHIFT_AND_MASK_LE(pdesc, 16, 8);
	seq = (u8)SHIFT_AND_MASK_LE(pdesc, 24, 7);
	frag = (bool)SHIFT_AND_MASK_LE(pdesc, 31, 1);
	data = pdesc + 8;

	//RT_TRACE(rtlpriv, COMP_INIT, DBG_TRACE, "C2H ID:%d len:%u\n", evnum, len);

	switch(evnum) {
	case 0x08:
		break;
	case 0x09:
		break;
	case 0x13: // FWDBG
		RT_TRACE(rtlpriv, COMP_FW, DBG_TRACE, "fwdbg: %s%s",
			 data, data[len - 2] == '\n' ? "" : "\n");
		break;

	default:
		RT_TRACE(rtlpriv, COMP_FW, DBG_WARNING, "unhandled C2H: %i\n",
			 evnum);
		break;
	}
}

bool rtl92su_rx_query_desc(struct ieee80211_hw *hw, struct rtl_stats *stats,
			   struct ieee80211_rx_status *rx_status, u8 *pdesc,
			   struct sk_buff *skb)
{
	struct rtl_priv *rtlpriv = rtl_priv(hw);
/*
	struct rx_fwinfo *p_drvinfo;
	u32 phystatus = (u32)GET_RX_STATUS_DESC_PHY_STATUS(pdesc);
*/
	struct ieee80211_hdr *hdr;
	bool first_ampdu = false;

	stats->length = (u16)GET_RX_STATUS_DESC_PKT_LEN(pdesc);
	stats->rx_drvinfo_size = (u8)GET_RX_STATUS_DESC_DRVINFO_SIZE(pdesc) * 8;
	stats->rx_bufshift = (u8)(GET_RX_STATUS_DESC_SHIFT(pdesc) & 0x03);
	stats->icv = (u16)GET_RX_STATUS_DESC_ICV(pdesc);
	stats->crc = (u16)GET_RX_STATUS_DESC_CRC32(pdesc);
	stats->hwerror = (u16)(stats->crc | stats->icv);
	stats->decrypted = !GET_RX_STATUS_DESC_SWDEC(pdesc);

	stats->rate = (u8)GET_RX_STATUS_DESC_RX_MCS(pdesc);
	stats->shortpreamble = (u16)GET_RX_STATUS_DESC_SPLCP(pdesc);
	stats->isampdu = (bool)(GET_RX_STATUS_DESC_PAGGR(pdesc) == 1);
	stats->isfirst_ampdu = (bool) ((GET_RX_STATUS_DESC_PAGGR(pdesc) == 1)
			       && (GET_RX_STATUS_DESC_FAGGR(pdesc) == 1));
	stats->timestamp_low = GET_RX_STATUS_DESC_TSFL(pdesc);
	stats->rx_is40Mhzpacket = (bool)GET_RX_STATUS_DESC_BW(pdesc);
	stats->is_ht = (bool)GET_RX_STATUS_DESC_RX_HT(pdesc);
	stats->is_cck = SE_RX_HAL_IS_CCK_RATE(pdesc);

	if (stats->hwerror)
		return false;

	if (GET_RX_STATUS_DESC_MACID(pdesc) == 0x1f &&
	    GET_RX_STATUS_DESC_TID(pdesc) == 0x0f) {
		/* C2H event */
		rtl92su_c2h_event(hw, pdesc + RX_STATUS_DESC_SIZE);
		return false;
	}

	rx_status->freq = hw->conf.channel->center_freq;
	rx_status->band = hw->conf.channel->band;

	hdr = (struct ieee80211_hdr *)(skb->data + stats->rx_drvinfo_size
	      + stats->rx_bufshift);
	RT_TRACE(rtlpriv, COMP_INIT, DBG_TRACE, "RX DATA fc[%02x] a1[%pM] a2[%pM] a3[%pM]\n",
		 hdr->frame_control, hdr->addr1, hdr->addr2, hdr->addr3);

	if (stats->crc)
		rx_status->flag |= RX_FLAG_FAILED_FCS_CRC;

	if (stats->rx_is40Mhzpacket)
		rx_status->flag |= RX_FLAG_40MHZ;

	if (stats->is_ht)
		rx_status->flag |= RX_FLAG_HT;

	rx_status->flag |= RX_FLAG_MACTIME_MPDU;

	/* hw will set stats->decrypted true, if it finds the
	 * frame is open data frame or mgmt frame,
	 * hw will not decrypt robust managment frame
	 * for IEEE80211w but still set stats->decrypted
	 * true, so here we should set it back to undecrypted
	 * for IEEE80211w frame, and mac80211 sw will help
	 * to decrypt it */
	if (rtlpriv->sec.use_sw_sec) {
		rx_status->flag &= ~RX_FLAG_DECRYPTED;
	} else if (!stats->decrypted) {
		if (ieee80211_is_robust_mgmt_frame(hdr) &&
		    ieee80211_has_protected(hdr->frame_control)) {
			rx_status->flag &= ~RX_FLAG_DECRYPTED;
		} else
			rx_status->flag |= RX_FLAG_DECRYPTED;
	}

	rx_status->rate_idx = rtlwifi_rate_mapping(hw,
			      stats->is_ht, stats->rate, first_ampdu);

	rx_status->mactime = stats->timestamp_low;
/*
	if (phystatus) {
		p_drvinfo = (struct rx_fwinfo *)(skb->data +
						 stats->rx_bufshift);
		// this call currently results in a BUG: scheduling while atomic
		// _rtl_rx_completed+0x81/0x210
		// _rtl_usb_rx_process_noagg+0xa8/0x220
		// rtl92su_rx_query_desc+0x782/0xa60
		// rtl92s_phy_query_bb_reg+0x63/0xe0
		// ...
		_rtl92se_translate_rx_signal_stuff(hw, skb, stats, pdesc,
						   p_drvinfo);
	}
*/

	/*rx_status->qual = stats->signal; */
	rx_status->signal = stats->rssi + 10;
	/*rx_status->noise = -stats->noise; */

	return true;
}

void  rtl8192su_rx_hdl(struct ieee80211_hw *hw, struct sk_buff * skb)
{
	struct ieee80211_rx_status *rx_status =
		 (struct ieee80211_rx_status *)IEEE80211_SKB_RXCB(skb);
	u8 *rxdesc;
	struct rtl_stats stats = {
		.signal = 0,
		.noise = -98,
		.rate = 0,
	};

	memset(rx_status, 0, sizeof(*rx_status));
	rxdesc = skb->data;
	if (rtl92su_rx_query_desc(hw, &stats, rx_status, rxdesc, skb)) {
		skb_pull(skb, (stats.rx_drvinfo_size + RTL_RX_DESC_SIZE));
		memcpy(IEEE80211_SKB_RXCB(skb), rx_status, sizeof(*rx_status));
		ieee80211_rx_irqsafe(hw, skb);
	}
}

/*
void rtl8192s_rx_segregate_hdl(
	struct ieee80211_hw *hw,
	struct sk_buff *skb,
	struct sk_buff_head *skb_list)
{
}
*/

static void _rtl_fill_usb_tx_desc(u8 *txdesc)
{
	SET_TX_DESC_OWN(txdesc, 1);
	SET_TX_DESC_LAST_SEG(txdesc, 1);
	SET_TX_DESC_FIRST_SEG(txdesc, 1);
}

void rtl92su_tx_fill_desc(struct ieee80211_hw *hw,
		struct ieee80211_hdr *hdr, u8 *pdesc_tx,
		struct ieee80211_tx_info *info,
		struct ieee80211_sta *sta, struct sk_buff *skb,
		u8 hw_queue, struct rtl_tcb_desc *ptcb_desc)
{
	struct rtl_priv *rtlpriv = rtl_priv(hw);
	struct rtl_mac *mac = rtl_mac(rtl_priv(hw));
	struct rtl_hal *rtlhal = rtl_hal(rtl_priv(hw));
	u8 *pdesc = (u8 *) pdesc_tx;
	u16 seq_number;
	__le16 fc = hdr->frame_control;
	u8 reserved_macid = 0;
	u8 fw_qsel = _rtl92se_map_hwqueue_to_fwqueue(skb, hw_queue);
	u8 bw_40 = 0;

	RT_TRACE(rtlpriv, COMP_INIT, DBG_TRACE, "TX DATA fc[%02x] a1[%pM] a2[%pM] a3[%pM]\n",
		 hdr->frame_control, hdr->addr1, hdr->addr2, hdr->addr3);

	if (mac->opmode == NL80211_IFTYPE_STATION) {
		bw_40 = mac->bw_40;
	} else if (mac->opmode == NL80211_IFTYPE_AP ||
		mac->opmode == NL80211_IFTYPE_ADHOC) {
		if (sta)
			bw_40 = sta->ht_cap.cap &
				    IEEE80211_HT_CAP_SUP_WIDTH_20_40;
	}

	/* 3 Fill necessary field in First Descriptor */
	/*DWORD 0*/
	SET_TX_DESC_LINIP(pdesc, 0);
	SET_TX_DESC_OFFSET(pdesc, 32);
	SET_TX_DESC_QUEUE_SEL(pdesc, fw_qsel);

	/* fill in packet size before adding pdesc header */
	SET_TX_DESC_PKT_SIZE(pdesc, (u16) skb->len - RTL_TX_HEADER_SIZE);

	seq_number = (le16_to_cpu(hdr->seq_ctrl) & IEEE80211_SCTL_SEQ) >> 4;

	rtl_get_tcb_desc(hw, info, sta, skb, ptcb_desc);

	if (rtlpriv->dm.useramask) {
		/* set pdesc macId */
		if (ptcb_desc->mac_id < 32) {
			SET_TX_DESC_MACID(pdesc, ptcb_desc->mac_id);
			reserved_macid |= ptcb_desc->mac_id;
		}
	}

	SET_TX_DESC_RSVD_MACID(pdesc, reserved_macid);

	SET_TX_DESC_TXHT(pdesc, ((ptcb_desc->hw_rate >=
			 DESC92_RATEMCS0) ? 1 : 0));

	if (rtlhal->version == VERSION_8192S_ACUT) {
		if (ptcb_desc->hw_rate == DESC92_RATE1M ||
			ptcb_desc->hw_rate  == DESC92_RATE2M ||
			ptcb_desc->hw_rate == DESC92_RATE5_5M ||
			ptcb_desc->hw_rate == DESC92_RATE11M) {
			ptcb_desc->hw_rate = DESC92_RATE12M;
		}
	}

	SET_TX_DESC_TX_RATE(pdesc, ptcb_desc->hw_rate);

	if (ptcb_desc->use_shortgi || ptcb_desc->use_shortpreamble)
		SET_TX_DESC_TX_SHORT(pdesc, 0);

	/* Aggregation related */
	if (info->flags & IEEE80211_TX_CTL_AMPDU)
		SET_TX_DESC_AGG_ENABLE(pdesc, 1);

	/* For AMPDU, we must insert SSN into TX_DESC */
	SET_TX_DESC_SEQ(pdesc, seq_number);

	/* Protection mode related */
	/* For 92S, if RTS/CTS are set, HW will execute RTS. */
	/* We choose only one protection mode to execute */
	SET_TX_DESC_RTS_ENABLE(pdesc, ((ptcb_desc->rts_enable &&
			!ptcb_desc->cts_enable) ? 1 : 0));
	SET_TX_DESC_CTS_ENABLE(pdesc, ((ptcb_desc->cts_enable) ?
			       1 : 0));
	SET_TX_DESC_RTS_RATE(pdesc, ptcb_desc->rts_rate);
	SET_TX_DESC_RTS_BANDWIDTH(pdesc, 0);
	SET_TX_DESC_RTS_SUB_CARRIER(pdesc, ptcb_desc->rts_sc);
	SET_TX_DESC_RTS_SHORT(pdesc, ((ptcb_desc->rts_rate <=
	       DESC92_RATE54M) ?
	       (ptcb_desc->rts_use_shortpreamble ? 1 : 0)
	       : (ptcb_desc->rts_use_shortgi ? 1 : 0)));


	/* Set Bandwidth and sub-channel settings. */
	if (bw_40) {
		if (ptcb_desc->packet_bw) {
			SET_TX_DESC_TX_BANDWIDTH(pdesc, 1);
			/* use duplicated mode */
			SET_TX_DESC_TX_SUB_CARRIER(pdesc, 0);
		} else {
			SET_TX_DESC_TX_BANDWIDTH(pdesc, 0);
			SET_TX_DESC_TX_SUB_CARRIER(pdesc,
					   mac->cur_40_prime_sc);
		}
	} else {
		SET_TX_DESC_TX_BANDWIDTH(pdesc, 0);
		SET_TX_DESC_TX_SUB_CARRIER(pdesc, 0);
	}

	/*DWORD 1*/
	SET_TX_DESC_RA_BRSR_ID(pdesc, ptcb_desc->ratr_index);

	/* Fill security related */
	if (info->control.hw_key) {
		struct ieee80211_key_conf *keyconf;

		keyconf = info->control.hw_key;
		switch (keyconf->cipher) {
		case WLAN_CIPHER_SUITE_WEP40:
		case WLAN_CIPHER_SUITE_WEP104:
			SET_TX_DESC_SEC_TYPE(pdesc, 0x1);
			break;
		case WLAN_CIPHER_SUITE_TKIP:
			SET_TX_DESC_SEC_TYPE(pdesc, 0x2);
			break;
		case WLAN_CIPHER_SUITE_CCMP:
			SET_TX_DESC_SEC_TYPE(pdesc, 0x3);
			break;
		default:
			SET_TX_DESC_SEC_TYPE(pdesc, 0x0);
			break;

		}
	}

	/* Set Packet ID */
	SET_TX_DESC_PACKET_ID(pdesc, 0);
	/* Alwasy enable all rate fallback range */
	SET_TX_DESC_DATA_RATE_FB_LIMIT(pdesc, 0x1F);
	/* Fix: I don't kown why hw use 6.5M to tx when set it */
	SET_TX_DESC_USER_RATE(pdesc,
			      ptcb_desc->use_driver_rate ? 1 : 0);

	/* Set NON_QOS bit. */
	if (!ieee80211_is_data_qos(fc))
		SET_TX_DESC_NON_QOS(pdesc, 1);

	if (is_multicast_ether_addr(ieee80211_get_DA(hdr))) {
		SET_TX_DESC_BMC(pdesc, 1);

		/*
		 * Note: In station mode, the station sends a
		 * copy of a broadcast data frame to the AP
		 * (of course, with a broadcast DA!)... This
		 * copy is sometimes a QoS-data frame and this
		 * seems to confuse the FW which seem so expect
		 * all broadcast data to be non-qos?!
		 */
		SET_TX_DESC_NON_QOS(pdesc, 1);
	}

	_rtl_fill_usb_tx_desc(pdesc);
}

void rtl92su_tx_fill_cmddesc(struct ieee80211_hw *hw, u8 *pdesc,
	bool firstseg, bool lastseg, struct sk_buff *skb)
{
	struct rtl_tcb_desc *tcb_desc = (struct rtl_tcb_desc *)(skb->cb);

	memset((void *)pdesc, 0, RTL_TX_HEADER_SIZE);

	/* This bit indicate this packet is used for FW download. */
	if (tcb_desc->cmd_or_init == DESC_PACKET_TYPE_INIT) {
		/* For firmware downlaod we only need to set LINIP */
		SET_TX_DESC_LINIP(pdesc, tcb_desc->last_inipkt);

		/* 92SE need not to set TX packet size when firmware download */
		SET_TX_DESC_PKT_SIZE(pdesc, (u16)(skb->len) -
				     RTL_TX_HEADER_SIZE);
	} else {
		/* H2C Command Desc format (Host TXCMD) */
		SET_TX_DESC_FIRST_SEG(pdesc, 1);
		SET_TX_DESC_LAST_SEG(pdesc, 1);
		SET_TX_DESC_OWN(pdesc, 1);

		SET_TX_DESC_OFFSET(pdesc, 0x20);

		/* Buffer size + command header */
		SET_TX_DESC_PKT_SIZE(pdesc, (u16)(skb->len) -
				     RTL_TX_HEADER_SIZE);

		/* Fixed queue of H2C command */
		SET_TX_DESC_QUEUE_SEL(pdesc, 0x13);
	}
}

void rtl8192s_tx_cleanup(struct ieee80211_hw *hw, struct sk_buff  *skb)
{
}

int rtl8192s_tx_post_hdl(struct ieee80211_hw *hw, struct urb *urb,
			 struct sk_buff *skb)
{
	return 0;
}

struct sk_buff *rtl8192s_tx_aggregate_hdl(struct ieee80211_hw *hw,
					   struct sk_buff_head *list)
{
	return skb_dequeue(list);
}
