#include <iostream>
#include <algorithm>
#include <vapi/vapi.h>
#include <vapi/vpe.api.vapi.hpp>
DEFINE_VAPI_MSG_IDS_VPE_API_JSON
#include <vapi/ping.api.vapi.hpp>
DEFINE_VAPI_MSG_IDS_PING_API_JSON


template <typename MyRequest>
static auto &
execute (vapi::Connection &con, MyRequest &req)
{
  // send the command to VPP
  auto err = req.execute ();
  if (VAPI_OK != err)
    throw std::runtime_error ("execute()");
  // active-wait for command result
  do
    {
      err = con.wait_for_response (req);
    }
  while (VAPI_EAGAIN == err);
  if (VAPI_OK != err)
    throw std::runtime_error ("wait_for_response()");
  // verify the reply error code
  auto &rmp = req.get_response ().get_payload ();
  if (0 != rmp.retval)
    throw std::runtime_error ("wrong return code");
  return rmp;
}


int
main ()
{
  // Connect to VPP: client name, API prefix, max outstanding request, response
  // queue size
  std::cout << "Connecting to VPP..." << std::endl;
  vapi::Connection con;

  auto err = con.connect("example_client", "vpp1", 32, 32);
  std::cout << err << std::endl;
  if (VAPI_OK != err)
    throw std::runtime_error("connection to VPP failed");

  vapi::My_ping_ping msg(con);

  auto &payload = msg.get_request().get_payload();
  payload.address.af = ADDRESS_IP4;
  const char ip[] = { 0x0a, 0x0a, 0x02, 0x02 }; // 10.10.2.2
  std::copy(ip, ip + 4, payload.address.un.ip4);

  payload.sw_if_index = 1;
  payload.repeat = 1;
  payload.interval = 5.0;

  auto reply = execute(con, msg);

  std::cout << "Request Count: " << reply.request_count << std::endl;
  std::cout << "Reply Count: " << reply.reply_count << std::endl;
  std::cout << "Iface Index: " << reply.if_index << std::endl;

  std::cout << "Event Type 1: " << reply.event_type1 << std::endl;
  std::cout << "Event Type 2: " << reply.event_type2 << std::endl;
  std::cout << "Event Type 3: " << reply.event_type3 << std::endl;
  std::cout << "Event Type 4: " << reply.event_type4 << std::endl;

  std::cout << "Ping Result 1:  " << reply.ping_res1 << std::endl;
  std::cout << "Ping Result 2:  " << reply.ping_res2 << std::endl;
  std::cout << "Ping Result 3:  " << reply.ping_res3 << std::endl;
  std::cout << "Ping Result 4:  " << reply.ping_res4 << std::endl;
 
  con.disconnect();
  std::cerr << "Success" << std::endl;
  return 0;
}