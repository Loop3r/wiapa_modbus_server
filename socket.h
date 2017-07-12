//
// Created by champer on 27/04/17.
//

#ifndef MODBUS_SERVER_SOCKET_H
#define MODBUS_SERVER_SOCKET_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/time.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>

#include <modbus/modbus.h>
#include "get_config.h"

#define IOT_DAEMON_PORT     5222
#define MODBUS_SERVER_PORT   502
#define IOT_DAEMON_ADDR    "127.0.0.1"

#define CO       0x01
#define HUMITURE 0x02  //温湿度
#define DUST     0x03
#define METER    0x04  //电表


#define CO_PACKET_LEN       20
#define HUMITURE_PACKET_LEN 20
#define DUST_PACKET_LEN     20
#define METER_PACKET_LEN    37


#define NB_CONNECTION        5
#define WIAPA_RESP_LEN       100

#define REGISTER_WRITE_HEAD   ((buf[8]-1)*20)


void close_sigint(int dummy);
void *Modbus_Server(void *arg);
void *Wiapa_Client(void *arg);
int Parse_Wiapa_Resp(uint8_t *buf, int len);
uint8_t Get_Data_Type(uint8_t *data);
int Meter_hex2int(uint8_t hex);
void Meter_to_hex(uint8_t *buf);
float Meter_to_float(uint8_t *buf);


#endif //MODBUS_SERVER_SOCKET_H