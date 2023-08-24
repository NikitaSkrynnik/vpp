/*
 *------------------------------------------------------------------
 * Copyright (c) 2023 Cisco and/or its affiliates.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *------------------------------------------------------------------
 */

#include <vlib/vlib.h>
#include <vlib/unix/unix.h>
#include <vlib/pci/pci.h>
#include <vnet/ethernet/ethernet.h>
#include <vnet/format_fns.h>
#include <vnet/ip/ip_types_api.h>

#include <vlibapi/api.h>
#include <vlibmemory/api.h>

#include <ping/ping.h>

/* define message IDs */
#include <ping/ping.api_enum.h>
#include <ping/ping.api_types.h>

#define REPLY_MSG_ID_BASE (pm->msg_id_base)
#include <vlibapi/api_helper_macros.h>

void
vl_api_my_ping_ping_t_handler (vl_api_my_ping_ping_t *mp)
{
  vlib_main_t *vm = vlib_get_main ();
  ping_main_t *pm = &ping_main;
  vl_api_my_ping_ping_reply_t *rmp;

  int rv;
  u32 n_requests = 0;
  u32 n_replies = 0;
  u16 icmp_id;
 
  ip_address_t ping_dst_addr = { 0 };
  ip_address_decode2(&mp->address, &ping_dst_addr);

  u32 rand_seed = random_default_seed();
  icmp_id = random_u32 (&rand_seed) & 0xffff;

  send_ip46_ping_result_t res = SEND_PING_OK;
  f64 sleep_interval = 5.0;

	res = send_ip4_ping(vm, 0, &ping_dst_addr.ip.ip4,
			       0, 0, icmp_id, 0,
			       0, 0);

	if (SEND_PING_OK == res)
	  n_requests += 1;
	else
    rv = 1;
   
	uword event_type, *event_data = 0;
	vlib_process_wait_for_event_or_clock(vm, sleep_interval);
	event_type = vlib_process_get_events(vm, &event_data);
  if (event_type == PING_RESPONSE_IP4)
      n_replies += 1;

  REPLY_MACRO2(VL_API_MY_PING_PING_REPLY, ({ rmp->reply_count = ntohl(n_replies); }));
}

/* set tup the API message handling tables */
#include <ping/ping.api.c>

clib_error_t *
ping_plugin_api_hookup(vlib_main_t *vm)
{
  ping_main_t *pm = &ping_main;

  /* ask for a correctly-sized block of API message decode slots */
  pm->msg_id_base = setup_message_id_table ();

  return 0;
}