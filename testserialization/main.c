/*
 * main.c
 *
 *  Created on: Nov 13, 2017
 *      Author: davec
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <netdb.h> /* getprotobyname */
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "common/comm_connectivity.h"
#include "common/comm_channel.h"
#include "common/messages/messages.h"
#define TEST_SIZE 1024


int main(int argc, char **argv)
{

	free(plc_top_alloc(100));

    	plcConn *send = plcConnect(8000);

	char *value1 = (char *)malloc(TEST_SIZE);
		int i;
		for (i=0; i< TEST_SIZE-1;i++){
			value1[i]= 'A' + random() % 26;
		}
		value1[TEST_SIZE-1]='\0';
    // this is the child
	plcMsgCallreq *req = (plcMsgCallreq *)malloc(sizeof( plcMsgCallreq));
	req->msgtype = MT_CALLREQ;
	req->proc.name="foobar";
	req->proc.src = "function definition";
	req->nargs = 3;
	req->args = (plcArgument *)malloc(3*sizeof(plcArgument));

	req->args[0].name = "arg1";
	req->args[0].type.type=PLC_DATA_TEXT;
	req->args[0].data.value=value1;

	req->args[1].name="arg2";
	req->args[1].type.type=PLC_DATA_TEXT;
	req->args[1].data.value="hello";

	req->args[2].name="arg3";
	req->args[2].type.type = PLC_DATA_TEXT;
	req->args[2].data.value="12.5";
    plcontainer_channel_send(send, (plcMessage *)req);
    return 0;
}
