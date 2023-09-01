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

  uword curr_proc = vlib_current_process (vm);

  static u32 rand_seed = 0;
  if (PREDICT_FALSE (!rand_seed))
    rand_seed = random_default_seed ();

  u16 icmp_id = random_u32 (&rand_seed) & 0xffff;
  while (~0 != get_cli_process_id_by_icmp_id_mt (vm, icmp_id))
  {
    vlib_cli_output (vm, "ICMP ID collision at %d, incrementing", icmp_id);
    icmp_id++;
  }

  set_cli_process_id_by_icmp_id_mt (vm, icmp_id, curr_proc);


  int rv = 0;
  u32 n_requests = 0;
  u32 n_replies = 0;

  if (mp->sw_if_index != ~0)
    VALIDATE_SW_IF_INDEX (mp);

  u32 table_id = 0;
  ip_address_t dst_addr = { 0 };
  u32 sw_if_index = ntohl (mp->sw_if_index);
  f64 ping_interval = clib_net_to_host_f64(mp->interval);
  u32 ping_repeat = ntohl (mp->repeat);
  u32 data_len = PING_DEFAULT_DATA_LEN;
  u32 ping_burst = 1;
  u32 verbose = 0;
  ip_address_decode2(&mp->address, &dst_addr);

  int i;
  send_ip46_ping_result_t results[4];
  send_ip46_ping_result_t res = SEND_PING_OK;
  uword event_types[4];
  for (i = 1; i <= ping_repeat; i++)
  {
    f64 sleep_interval;
    f64 time_ping_sent = vlib_time_now (vm);

    vlib_log_notice(pm->log_class, "Sending ping...");
    res = send_ip4_ping(vm, table_id, &dst_addr.ip.ip4, sw_if_index, i, icmp_id, data_len, ping_burst, verbose);

    if (i <= 4) results[i-1] = res;
    if (SEND_PING_OK == res)
      n_requests += 1;
    
    while (
      (i <= ping_repeat) && 
      (sleep_interval = time_ping_sent + ping_interval - vlib_time_now(vm)) > 0.0)
    {
      uword event_type, *event_data = 0;
      vlib_process_wait_for_event_or_clock(vm, sleep_interval);
      event_type = vlib_process_get_events(vm, &event_data);

      vlib_log_notice(pm->log_class, "Got event type: %u", event_type);

      if (i <= 4) event_types[i-1] = event_type;
      
      if (event_type == ~0)
        break;
      
      if (event_type == PING_RESPONSE_IP4 || event_type == PING_RESPONSE_IP6)
        n_replies += vec_len(event_data);
      
      vec_free(event_data);
    }
  }

  BAD_SW_IF_INDEX_LABEL;

  clear_cli_process_id_by_icmp_id_mt(vm, icmp_id);

  REPLY_MACRO2(VL_API_MY_PING_PING_REPLY, (
    { 
      rmp->request_count = ntohl(n_requests); 
      rmp->reply_count = ntohl(n_replies); 
      rmp->if_index = ntohl(sw_if_index); 
      rmp->event_type1 = ntohl(event_types[0]);
      rmp->event_type2 = ntohl(event_types[1]);
      rmp->event_type3 = ntohl(event_types[2]);
      rmp->event_type4 = ntohl(event_types[3]);
      rmp->ping_res1 = ntohl(results[0]);
      rmp->ping_res2 = ntohl(results[1]);
      rmp->ping_res3 = ntohl(results[2]);
      rmp->ping_res4 = ntohl(results[3]);
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