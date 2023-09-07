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
vl_api_ping_t_handler (vl_api_ping_t *mp)
{
  vlib_main_t *vm = vlib_get_main ();
  ping_main_t *pm = &ping_main;
  vl_api_ping_reply_t *rmp;

  uword curr_proc = vlib_current_process (vm);

  static u32 rand_seed = 0;

  if (PREDICT_FALSE (!rand_seed))
    rand_seed = random_default_seed ();

  u16 icmp_id = random_u32 (&rand_seed) & 0xffff;

  while (~0 != get_cli_process_id_by_icmp_id_mt (vm, icmp_id))
    icmp_id++;

  set_cli_process_id_by_icmp_id_mt (vm, icmp_id, curr_proc);

  int rv = 0;
  u32 n_requests = 0;
  u32 n_replies = 0;

  u32 table_id = 0;
  ip_address_t dst_addr = { 0 };
  u32 sw_if_index = ~0;
  f64 ping_timeout = clib_net_to_host_f64(mp->timeout);
  u32 data_len = PING_DEFAULT_DATA_LEN;
  u32 ping_burst = 1;
  u32 verbose = 0;
  ip_address_decode2(&mp->address, &dst_addr);

  send_ip46_ping_result_t res = SEND_PING_OK;
  f64 sleep_interval;
  f64 time_ping_sent = vlib_time_now(vm);

  vlib_log_notice(pm->log_class, "Sending ping...");

  if (dst_addr.version == AF_IP4)
    res = send_ip4_ping(vm, table_id, &dst_addr.ip.ip4, sw_if_index, 1, icmp_id, data_len, ping_burst, verbose);
  else
    res = send_ip6_ping(vm, table_id, &dst_addr.ip.ip6, sw_if_index, 1, icmp_id, data_len, ping_burst, verbose);

  if (SEND_PING_OK == res)
    n_requests = 1;
  
  while ((sleep_interval = time_ping_sent + ping_timeout - vlib_time_now(vm)) > 0.0)
  {
    vlib_process_wait_for_event_or_clock(vm, sleep_interval);
    uword event_type = vlib_process_get_events(vm, 0);

    vlib_log_notice(pm->log_class, "Got event type: %u", event_type);
    
    if (event_type == PING_RESPONSE_IP4 || event_type == PING_RESPONSE_IP6)
    {
      n_replies = 1;
      break;
    }
  }

  clear_cli_process_id_by_icmp_id_mt(vm, icmp_id);

  REPLY_MACRO2(VL_API_PING_REPLY, ({ 
      rmp->reply_count = ntohl(n_replies); 
    }));
      
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