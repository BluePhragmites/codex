#include <string.h>

#include "test_helpers.h"

#include "mini_gnb_c/ue/ue_context_store.h"

void test_ue_context_store_promote_initializes_core_session(void) {
  mini_gnb_c_ue_context_store_t store;
  mini_gnb_c_ra_context_t ra_context;
  mini_gnb_c_rrc_setup_request_info_t request_info;
  mini_gnb_c_ue_context_t* ue_context = NULL;
  static const uint8_t contention_id48[6] = {0xa1u, 0xb2u, 0xc3u, 0xd4u, 0xe5u, 0xf6u};

  mini_gnb_c_ue_context_store_init(&store);
  memset(&ra_context, 0, sizeof(ra_context));
  memset(&request_info, 0, sizeof(request_info));

  ra_context.tc_rnti = 0x4601u;
  memcpy(request_info.contention_id48, contention_id48, sizeof(contention_id48));
  request_info.valid = true;

  ue_context = mini_gnb_c_ue_context_store_promote(&store, &ra_context, &request_info, 6);

  mini_gnb_c_require(ue_context != NULL, "expected UE context promotion");
  mini_gnb_c_require(store.count == 1U, "expected one stored UE context");
  mini_gnb_c_require(ue_context->tc_rnti == 0x4601u, "expected promoted TC-RNTI");
  mini_gnb_c_require(ue_context->c_rnti == 0x4601u, "expected promoted C-RNTI");
  mini_gnb_c_require(memcmp(ue_context->contention_id48, contention_id48, sizeof(contention_id48)) == 0,
                     "expected contention identity in promoted UE context");
  mini_gnb_c_require(ue_context->core_session.c_rnti == 0x4601u, "expected core session C-RNTI seed");
  mini_gnb_c_require(!ue_context->core_session.ran_ue_ngap_id_valid, "expected empty RAN UE NGAP ID");
  mini_gnb_c_require(!ue_context->core_session.amf_ue_ngap_id_valid, "expected empty AMF UE NGAP ID");
  mini_gnb_c_require(!ue_context->core_session.pdu_session_id_valid, "expected empty PDU session state");
  mini_gnb_c_require(!ue_context->core_session.upf_tunnel_valid, "expected empty UPF tunnel state");
  mini_gnb_c_require(!ue_context->core_session.qfi_valid, "expected empty QFI state");
  mini_gnb_c_require(!ue_context->core_session.ue_ipv4_valid, "expected empty UE IPv4 state");
  mini_gnb_c_require(ue_context->core_session.uplink_nas_count == 0u, "expected zero uplink NAS count");
  mini_gnb_c_require(ue_context->core_session.downlink_nas_count == 0u, "expected zero downlink NAS count");
}
