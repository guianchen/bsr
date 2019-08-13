﻿#include "drbd_kref_debug.h"

#ifdef _WIN32
// not support, windows drbd current version 1.0
#else
struct kref_debug_class kref_class_resource = {
	"resource", {
		[1] = "kthread",
		[2] = "drbd_adm_prepare()/drbd_adm_finish()",
		[3] = "struct drbd_connection",
		[4] = "struct drbd_device",
		[5] = "struct drbd_state_change",
		[6] = "drbd_adm_dump_connections()",
		[7] = "drbd_adm_dump_devices()",
		[8] = "free",
		[9] = "drbd_adm_dump_peer_devices()",
	}
};

struct kref_debug_class kref_class_connection = {
	"connection", {
		[1] = "kthread",
		[2] = "drbd_adm_prepare()/drbd_adm_finish()",
		[3] = "struct drbd_peer_device",
		[4] = "conn_try_outdate_peer_async()",
		[5] = "remember_state_change()forget_state_change()",
		[6] = "change_cluster_wide_state()",
		[7] = "struct drbd_state_change",
		[8] = "target_connection/change_cluster_wide_state()",
		[9] = "resource->twopc_parent",
		[10] = "free",
		[11] = "connect_timer",
		[12] = "receive_peer_dagtag()",
		[13] = "for_each_conneciton_ref()",
		[14] = "w_update_peers",
		[15] = "for_each_peer_device_ref()",
		[16] = "queue_twopc",
	}
};

struct kref_debug_class kref_class_device = {
	"device", {
		[1] = "struct drbd_peer_device / free",
		[2] = "struct drbd_state_change",
		[3] = "open / release",
		[4] = "drbd_adm_prepare()/drbd_adm_finish()",
		[5] = "w_update_peers",
		[6] = "drbd_request",
		[7] = "flush_after_epoch",
		[8] = "send_acks_wf",
		[9] = "open()",
	}
};


#endif