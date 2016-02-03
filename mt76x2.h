/*
 * Copyright (C) 2014 Felix Fietkau <nbd@openwrt.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __MT76x2_H
#define __MT76x2_H

#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/spinlock.h>
#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/bitops.h>
#include <linux/kfifo.h>

#define MT7662_FIRMWARE		"mt7662.bin"
#define MT7662_ROM_PATCH	"mt7662_rom_patch.bin"
#define MT7662_EEPROM_SIZE	512

#define MT_RX_HEADROOM		32

#define MT_MAX_CHAINS		2

#define MT_CALIBRATE_INTERVAL	HZ

#include "mt76.h"
#include "mt76x2_regs.h"
#include "mt76x2_mac.h"

struct mt76x2_mcu {
	struct mutex mutex;

	wait_queue_head_t wait;
	struct sk_buff_head res_q;

	u32 msg_seq;
};

struct mt76x2_rx_freq_cal {
	s8 high_gain[MT_MAX_CHAINS];
	s8 rssi_offset[MT_MAX_CHAINS];
	s8 lna_gain;
	u32 mcu_gain;
};

struct mt76x2_calibration {
	struct mt76x2_rx_freq_cal rx;

	u8 agc_gain_init[MT_MAX_CHAINS];
	int avg_rssi[MT_MAX_CHAINS];
	int avg_rssi_all;

	s8 low_gain;

	u8 temp;

	bool init_cal_done;
	bool tssi_cal_done;
	bool tssi_comp_pending;
	bool dpd_cal_done;
	bool channel_cal_done;
};

struct mt76x2_rate_power {
	union {
		struct {
			s8 cck[4];
			s8 ofdm[8];
			s8 ht[16];
			s8 vht[10];
		};
		s8 all[38];
	};
};

struct mt76x2_dev {
	struct mt76_dev mt76; /* must be first */

	struct mac_address macaddr_list[8];

	struct mutex mutex;

	const u16 *beacon_offsets;
	unsigned long wcid_mask[256 / BITS_PER_LONG];

	struct cfg80211_chan_def chandef;
	int txpower_conf;
	int txpower_cur;

	u8 txdone_seq;
	DECLARE_KFIFO_PTR(txstatus_fifo, struct mt76x2_tx_status);

	struct mt76x2_mcu mcu;
	struct sk_buff *rx_head;

	struct tasklet_struct tx_tasklet;
	struct tasklet_struct pre_tbtt_tasklet;
	struct delayed_work cal_work;
	struct delayed_work mac_work;

	u32 aggr_stats[32];

	struct mt76_wcid __rcu *wcid[254 - 8];

	spinlock_t irq_lock;
	u32 irqmask;

	struct sk_buff *beacons[8];
	u8 beacon_mask;
	u8 beacon_data_mask;

	u32 rev;
	u32 rxfilter;

	u16 chainmask;

	struct mt76x2_calibration cal;
	struct debugfs_blob_wrapper otp;

	s8 target_power;
	s8 target_power_delta[2];
	struct mt76x2_rate_power rate_power;

	u8 coverage_class;
	u8 slottime;
};

struct mt76x2_vif {
	u8 idx;

	struct mt76_wcid group_wcid;
};

struct mt76x2_sta {
	struct mt76_wcid wcid; /* must be first */

	struct mt76x2_tx_status status;
	int n_frames;
};

static inline bool is_mt7612(struct mt76x2_dev *dev)
{
	return (dev->rev >> 16) == 0x7612;
}

void mt76x2_set_irq_mask(struct mt76x2_dev *dev, u32 clear, u32 set);

static inline void mt76x2_irq_enable(struct mt76x2_dev *dev, u32 mask)
{
	mt76x2_set_irq_mask(dev, 0, mask);
}

static inline void mt76x2_irq_disable(struct mt76x2_dev *dev, u32 mask)
{
	mt76x2_set_irq_mask(dev, mask, 0);
}

extern const struct ieee80211_ops mt76x2_ops;

struct mt76x2_dev *mt76x2_alloc_device(struct device *pdev);
int mt76x2_register_device(struct mt76x2_dev *dev);
void mt76x2_init_debugfs(struct mt76x2_dev *dev);

irqreturn_t mt76x2_irq_handler(int irq, void *dev_instance);
void mt76x2_phy_power_on(struct mt76x2_dev *dev);
int mt76x2_init_hardware(struct mt76x2_dev *dev);
void mt76x2_stop_hardware(struct mt76x2_dev *dev);
int mt76x2_eeprom_init(struct mt76x2_dev *dev);
int mt76x2_apply_calibration_data(struct mt76x2_dev *dev, int channel);
void mt76x2_set_tx_ackto(struct mt76x2_dev *dev);

int mt76x2_phy_start(struct mt76x2_dev *dev);
int mt76x2_set_channel(struct mt76x2_dev *dev, struct cfg80211_chan_def *chandef);
int mt76x2_phy_set_channel(struct mt76x2_dev *dev,
			 struct cfg80211_chan_def *chandef);
int mt76x2_phy_get_rssi(struct mt76x2_dev *dev, s8 rssi, int chain);
void mt76x2_phy_calibrate(struct work_struct *work);
void mt76x2_phy_set_txpower(struct mt76x2_dev *dev);

int mt76x2_mcu_init(struct mt76x2_dev *dev);
int mt76x2_mcu_set_channel(struct mt76x2_dev *dev, u8 channel, u8 bw, u8 bw_index,
			 bool scan);
int mt76x2_mcu_set_radio_state(struct mt76x2_dev *dev, bool on);
int mt76x2_mcu_load_cr(struct mt76x2_dev *dev, u8 type, u8 temp_level, u8 channel);
int mt76x2_mcu_cleanup(struct mt76x2_dev *dev);

int mt76x2_dma_init(struct mt76x2_dev *dev);
void mt76x2_dma_cleanup(struct mt76x2_dev *dev);

void mt76x2_cleanup(struct mt76x2_dev *dev);
void mt76x2_rx(struct mt76x2_dev *dev, struct sk_buff *skb);

int mt76x2_tx_queue_skb(struct mt76_dev *cdev, struct mt76_queue *q,
			struct sk_buff *skb, struct mt76_txwi_cache *t,
			struct mt76_wcid *wcid, struct ieee80211_sta *sta);
int mt76x2_tx_queue_mcu(struct mt76x2_dev *dev, enum mt76_txq_id qid,
			struct sk_buff *skb, int cmd, int seq);
void mt76x2_tx(struct ieee80211_hw *hw, struct ieee80211_tx_control *control,
	     struct sk_buff *skb);
void mt76x2_tx_complete(struct mt76x2_dev *dev, struct sk_buff *skb);

void mt76x2_pre_tbtt_tasklet(unsigned long data);

void mt76x2_txq_init(struct mt76x2_dev *dev, struct ieee80211_txq *txq);


void mt76x2_rx_poll_complete(struct mt76_dev *dev, enum mt76_rxq_id q);
void mt76x2_queue_rx_skb(struct mt76_dev *mdev, enum mt76_rxq_id q,
			 struct sk_buff *skb);

#endif
