#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netlink/msg.h>
#include <netlink/attr.h>

#include "genl_lib.h"
#define LED_PATTERN_CMD "LED"
#define CS_PIN_CMD "CHS"
#define ECHO_PIN_CMD "ECH"
#define TRIG_PIN_CMD "TRG"
#define DISTANCE_CMD "DST"
#define DELAY_CMD "DLY"

struct led_data{
uint8_t led[8]; // 8 bytes (64 bits) for 64 leds
}*pattern;

static char *message = NULL;
static int distance = 10;
static int delay = 100;

static int print_krnl_msg(struct nl_msg *msg, void *arg)
{
    
    struct nlattr *attr[genl_attr_max+1];
    printf("entered print_kernel_msg function \n");
    genlmsg_parse(nlmsg_hdr(msg), 0, attr, genl_attr_max,genl_policy);
    if(attr[GENL_INT_ATTR])
    {
        distance = nla_get_u32(attr[GENL_INT_ATTR]);
        printf("distance = %d \n ",distance);
    }
    return 0;

}
static int send_msg_to_kernel(struct nl_sock *sock, int number)
{
	struct nl_msg* msg;
	int family_id, err = 0;
    int num = number;
   

    printf("entered send_msg_to_kernel \n");

	family_id = genl_ctrl_resolve(sock,genl_family_name);
    msg = nlmsg_alloc();
    genlmsg_put(msg, NL_AUTO_PID, NL_AUTO_SEQ, family_id, 0, NLM_F_REQUEST, GENL_TEST_C_MSG, 0);

    if(num == 1)
    {
        printf("message type is string \n");
        
        nla_put(msg, GENL_STRING_ATTR,sizeof(char)*20,message);
        err = nl_send_auto(sock, msg);
        printf("message sent \n");
        return 0;
    }
    else if(num == 2)
    {
        printf("message type is custom structure \n");
        nla_put(msg,GENL_STRUCT_ATTR,sizeof(struct led_data),(const void *)pattern);
        err = nl_send_auto(sock, msg);
        printf("message sent \n");
        return 0;
    }
    
    return 0;

}


static void config_nlsock(struct nl_sock **nlsockptr)
{
    int family_id, grp_id;
    *nlsockptr = nl_socket_alloc();
    printf("line 1 \n");
    nl_socket_disable_seq_check(*nlsockptr);
    printf("line 2 \n");
	nl_socket_disable_auto_ack(*nlsockptr);
    printf("line 3 \n");
    genl_connect(*nlsockptr);
    printf("line 4 \n");
    family_id = genl_ctrl_resolve(*nlsockptr,genl_family_name);
    printf("line 5 \n");
    grp_id = genl_ctrl_resolve_grp(*nlsockptr,genl_family_name,genl_mcgrp_name[0]);
    printf("line 6 \n");
    nl_socket_add_membership(*nlsockptr, grp_id);
    printf("line 7 \n");
}

int main()
{
    int ret,iter;
    printf("entered int main \n");
    struct nl_sock *nlsock = NULL;
	struct nl_cb *callback = NULL;
    config_nlsock(&nlsock);
    printf("configure nlsock done \n");
    unsigned int value[24] = {0x00,0x00,0x1C,0x3E,0x3E,0x3E,0x3E,0x1C,0x1C,0x3E,0x3E,0x3E,0x3E,0x1C,0x00,0x00,0x00,0x1C,0x3E,0x3E,0x3E,0x3E,0x1C,0x00};

    callback = nl_cb_alloc(NL_CB_DEFAULT);
	nl_cb_set(callback, NL_CB_VALID, NL_CB_CUSTOM, print_krnl_msg, NULL);
    printf("callback function set \n");

    message = (char *)malloc(sizeof(char)*20);
    pattern = (struct led_data *)malloc(sizeof(struct led_data));


    sprintf(message,"%s,%04x",CS_PIN_CMD,0x0A);

    send_msg_to_kernel(nlsock,1);

    sprintf(message,"%s,%04x",TRIG_PIN_CMD,0x02);

    send_msg_to_kernel(nlsock,1);


    sprintf(message,"%s,%04x",ECHO_PIN_CMD,0x01);

    send_msg_to_kernel(nlsock,1);


    sprintf(message,"%s,%04x",DELAY_CMD,0x64);

    send_msg_to_kernel(nlsock,1);

    for(iter=0;iter<=7;iter++) {
        pattern->led[iter] = value[iter];
    }    
    send_msg_to_kernel(nlsock,2);

    for(iter=0;iter<=7;iter++) {
        pattern->led[iter] = value[iter+8];
    }
    send_msg_to_kernel(nlsock,2);

    for(iter=0;iter<=7;iter++) {
        pattern->led[iter] = value[iter+16];
    }
    send_msg_to_kernel(nlsock,2);
    int distanceTmp = 5;

    while(1) {
        sprintf(message,"%s,%04x",DISTANCE_CMD,0x00);
        send_msg_to_kernel(nlsock,1);
        ret = nl_recvmsgs(nlsock, callback);
        sleep(3);
        delay = (1000/distance); 
        printf("delay = %d \n",delay);
        sprintf(message,"%s,%04x",DELAY_CMD,delay);
        send_msg_to_kernel(nlsock,1);
    }

    return 0;
}