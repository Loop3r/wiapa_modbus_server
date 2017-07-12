//
// Created by champer on 27/04/17.
//

#include "socket.h"
#include "modbus_data.h"
#include "get_config.h"

int server_socket = -1;
int Wiapa_Client_SocketFd = -1;

uint16_t DEVICE_NUM = 0;
uint16_t TIMEOUT = 0;
uint16_t DELAY = 0;

uint8_t Meter_Data[4];

void close_sigint(int dummy)
{
    if (server_socket != -1) {
        close(server_socket);
    }
    modbus_free(ctx);
    modbus_mapping_free(mb_mapping);

    exit(dummy);
}


void *Modbus_Server(void *arg)
{
    uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];
    int master_socket;
    int rc;
    fd_set refset;
    fd_set rdset;
    /* Maximum file descriptor number */
    int fdmax;

    ctx = modbus_new_tcp(INADDR_ANY, MODBUS_SERVER_PORT);

    mb_mapping = modbus_mapping_new_start_address(
            UT_BITS_ADDRESS, UT_BITS_NB,
            UT_INPUT_BITS_ADDRESS, UT_INPUT_BITS_NB,
            UT_REGISTERS_ADDRESS, UT_REGISTERS_NB_MAX,
            UT_INPUT_REGISTERS_ADDRESS, UT_INPUT_REGISTERS_NB
    );
    if (mb_mapping == NULL) {
        fprintf(stderr, "Failed to allocate the mapping: %s\n",
                modbus_strerror(errno));
        modbus_free(ctx);
        exit(EXIT_FAILURE);
    }

    /* Initialize values of INPUT REGISTERS */
    for (int i=0; i < UT_INPUT_REGISTERS_NB; i++) {
        mb_mapping->tab_input_registers[i] = UT_INPUT_REGISTERS_TAB[i];
    }

    server_socket = modbus_tcp_listen(ctx, NB_CONNECTION);
    if (server_socket == -1) {
        fprintf(stderr, "Unable to listen TCP connection\n");
        modbus_free(ctx);
        exit(EXIT_FAILURE);
    }

    signal(SIGINT, close_sigint);

    /* Clear the reference set of socket */
    FD_ZERO(&refset);
    /* Add the server socket */
    FD_SET(server_socket, &refset);

    /* Keep track of the max file descriptor */
    fdmax = server_socket;

    for (;;) {
        rdset = refset;
        if (select(fdmax+1, &rdset, NULL, NULL, NULL) == -1) {
            perror("Server select() failure.");
            close_sigint(1);
        }

        /* Run through the existing connections looking for data to be
         * read */
        for (master_socket = 0; master_socket <= fdmax; master_socket++) {

            if (!FD_ISSET(master_socket, &rdset)) {
                continue;
            }

            if (master_socket == server_socket) {
                /* A client is asking a new connection */
                socklen_t addrlen;
                struct sockaddr_in clientaddr;
                int newfd;

                /* Handle new connections */
                addrlen = sizeof(clientaddr);
                memset(&clientaddr, 0, sizeof(clientaddr));
                newfd = accept(server_socket, (struct sockaddr *)&clientaddr, &addrlen);
                if (newfd == -1) {
                    perror("Server accept() error");
                } else {
                    FD_SET(newfd, &refset);

                    if (newfd > fdmax) {
                        /* Keep track of the maximum */
                        fdmax = newfd;
                    }
                    printf("New connection from %s:%d on socket %d\n",
                           inet_ntoa(clientaddr.sin_addr), clientaddr.sin_port, newfd);
                }
            } else {
                modbus_set_socket(ctx, master_socket);
                rc = modbus_receive(ctx, query);
                if (rc > 0) {
                    modbus_reply(ctx, query, rc, mb_mapping);
                } else if (rc == -1) {
                    /* This example server in ended on connection closing or
                     * any errors. */
                    printf("Connection closed on socket %d\n", master_socket);
                    close(master_socket);

                    /* Remove from reference set */
                    FD_CLR(master_socket, &refset);

                    if (master_socket == fdmax) {
                        fdmax--;
                    }
                }
            }
        }
    }
}

void *Wiapa_Client(void *arg) {

    struct sockaddr_in clientAddr;
    int nread;
    uint8_t Wiapa_Resp[WIAPA_RESP_LEN];
    fd_set rset;

    clientAddr.sin_family = AF_INET;
    clientAddr.sin_port = htons(IOT_DAEMON_PORT);
    clientAddr.sin_addr.s_addr = inet_addr(IOT_DAEMON_ADDR);
    Wiapa_Client_SocketFd = socket(AF_INET, SOCK_STREAM, 0);
    //setsockopt(Wiapa_Client_SocketFd,SOL_SOCKET,SO_RCVTIMEO,(const char*)&timeout,sizeof(timeout));

    if (Wiapa_Client_SocketFd < 0) {
        perror("wiapa client socket");
        exit(EXIT_FAILURE);
    }
    if (connect(Wiapa_Client_SocketFd, (struct sockaddr *) &clientAddr, sizeof(clientAddr)) < 0) {
        perror("wiapa client connect");
        exit(EXIT_FAILURE);
    }

    FD_ZERO(&rset);
    while(1){

        FD_SET(Wiapa_Client_SocketFd, &rset);
        //int recvd = recv(Wiapa_Client_SocketFd, IPv6_Resp, IPV6_RESP_LEN, 0);
        if (-1 == select(Wiapa_Client_SocketFd + 1, &rset, NULL, NULL, NULL)){
            perror("select");
        }
        else{
            if (FD_ISSET(Wiapa_Client_SocketFd, &rset)){
                nread = read(Wiapa_Client_SocketFd, Wiapa_Resp, WIAPA_RESP_LEN);
                if (-1 == nread) {
                    perror("read");
                } else {
                    Parse_Wiapa_Resp(Wiapa_Resp, nread);
                }
            }
            //usleep((useconds_t)(DELAY*1000));
        }
    }
}


int Parse_Wiapa_Resp(uint8_t *buf, int len)
{
    if(buf[0] == 0xA1 && buf[1] == 0xA2)
    {
        if(Get_Data_Type(buf) == CO && len == CO_PACKET_LEN) {
            printf("get node%x CO data:%d\n", buf[7]<<8|buf[8], buf[18]<<8|buf[19]);
            UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD] = (uint16_t)buf[7]<<8|buf[8];        //addr
            UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD + 1] = (uint16_t)buf[12];               //type
            UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD + 2] = (uint16_t)buf[18]<<8|buf[19];    //data
        }
        if(Get_Data_Type(buf) == HUMITURE && len == HUMITURE_PACKET_LEN) {
            printf("get node%x moisture data:%d temp data:%d,\n", buf[7]<<8|buf[8], buf[16]<<8|buf[17], buf[18]<<8|buf[19]);
            UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD] = (uint16_t)buf[7]<<8|buf[8];        //addr
            UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD + 1] = (uint16_t)buf[12];               //type
            UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD + 2] = (uint16_t)buf[16]<<8|buf[17];    //data
            UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD + 3] = (uint16_t)buf[18]<<8|buf[19];
        }
        if(Get_Data_Type(buf) == DUST && len == DUST_PACKET_LEN) {
            printf("get node%x DUST data:%d\n", buf[7]<<8|buf[8], buf[18]<<8|buf[19]);
            UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD] = (uint16_t)buf[7]<<8|buf[8];        //addr
            UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD + 1] = (uint16_t)buf[12];               //type
            UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD + 2] = (uint16_t)buf[18]<<8|buf[19];    //data
        }
        if(Get_Data_Type(buf) == METER && len == METER_PACKET_LEN){
            float tempf = Meter_to_float(buf);
            Meter_to_hex(buf);
            printf("get node%x METER data:%.2f\n", buf[7]<<8|buf[8], tempf);
            UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD] = (uint16_t)buf[7]<<8|buf[8];        //addr
            UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD + 1] = (uint16_t)buf[12];               //type
            UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD + 2] = (uint16_t)Meter_Data[0]<<8|Meter_Data[1];    //data
            UT_INPUT_REGISTERS_TAB[REGISTER_WRITE_HEAD + 3] = (uint16_t)Meter_Data[2]<<8|Meter_Data[3];
        }
    }
    for (int i=0; i < UT_INPUT_REGISTERS_NB; i++) {
        mb_mapping->tab_input_registers[i] = UT_INPUT_REGISTERS_TAB[i];
    }
    return 0;
}

uint8_t Get_Data_Type(uint8_t *data)
{
    return data[12];
}

int Meter_hex2int(uint8_t hex)
{
    char *temps;
    int tempi;
    temps = (char *)malloc(sizeof(char *));
    sprintf(temps, "%x", hex);
    tempi = (uint16_t)atoi(temps);
    return tempi;
}

float Meter_to_float(uint8_t *buf)
{
    uint16_t integer;
    float decimal;
    integer = Meter_hex2int(buf[34]-0x33)*10000 + Meter_hex2int(buf[33]-0x33)*100 + Meter_hex2int(buf[32]-0x33);
    decimal = (float)Meter_hex2int(buf[31]-0x33) * 0.01 + (float)integer;
    return decimal;
}

void Meter_to_hex(uint8_t *buf)
{
    float temp;
    uint8_t * ret;
    //ret = (unsigned char *)malloc(sizeof(unsigned char *));
    temp = Meter_to_float(buf);
    ret = (uint8_t *)&temp;
    for(int i=0; i<4; i++){
       Meter_Data[i] = ret[i];
    }
}



