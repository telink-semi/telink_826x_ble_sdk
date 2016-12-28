#pragma  once

//#include "../hci/hci_include.h"



// define the l2cap CID for BLE
#define L2CAP_CID_NULL                   0x0000
#define L2CAP_CID_ATTR_PROTOCOL          0x0004
#define L2CAP_CID_SIG_CHANNEL            0x0005
#define L2CAP_CID_SMP                    0x0006
#define L2CAP_CID_GENERIC                0x0007
#define L2CAP_CID_DYNAMIC                0x0040

#define L2CAP_CID_DYNAMIC                0x0040

#define L2CAP_HEADER_LENGTH              0x0004
#define L2CAP_MTU_SIZE                   23

#define L2CAP_CMD_REJECT                 0x01
#define L2CAP_CMD_DISC_CONN_REQ          0x06
#define L2CAP_CMD_DISC_CONN_RESP         0x07
#define L2CAP_CMD_CONN_UPD_PARA_REQ      0x12
#define L2CAP_CMD_CONN_UPD_PARA_RESP     0x13
#define L2CAP_CMD_CONN_REQ               0x14
#define L2CAP_CMD_CONN_RESP              0x15
#define L2CAP_CMD_FLOW_CTRL_CRED         0x16


#define L2CAP_SIGNAL_MSG_TYPE            0x01
#define L2CAP_DATA_MSG_TYPE              0x02

/** 
 * Command Reject: Reason Codes
 */
  // Command not understood
#define L2CAP_REJECT_CMD_NOT_UNDERSTOOD  0x0000

  // Signaling MTU exceeded
#define L2CAP_REJECT_SIGNAL_MTU_EXCEED   0x0001

  // Invalid CID in request
#define L2CAP_REJECT_INVALID_CID         0x0002

// Response Timeout expired
#define L2CAP_RTX_TIMEOUT_MS             2000

#define NEXT_SIG_ID()                    ( ++l2capId == 0 ? l2capId = 1 : l2capId )


#define L2CAP_PKT_HANDLER_SIZE           6
 
 
// l2cap handler type
#define L2CAP_CMD_PKT_HANDLER            0
#define L2CAP_USER_CB_HANDLER            1

// l2cap pb flag type
#define L2CAP_FRIST_PKT_H2C              0x00
#define L2CAP_CONTINUING_PKT             0x01
#define L2CAP_FIRST_PKT_C2H              0x02



#define L2CAP_CONNECTION_PARAMETER_ACCEPTED        0x0000
#define L2CAP_CONNECTION_PARAMETER_REJECTED        0x0001



/******************************* User Interface  ************************************/
//Slave
void		bls_l2cap_requestConnParamUpdate (u16 min_interval, u16 max_interval, u16 latency, u16 timeout);

void		blc_l2cap_register_handler (void *p);
void 		blc_l2cap_reg_att_cli_hander(void *p);
int 		blc_l2cap_packet_receive (u8 * p);
int 		blc_l2cap_send_data (u16 cid, u8 *p, int n);
int 		bls_hci_sendACLData2Host (u8 *p);


//Master
