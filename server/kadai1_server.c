#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define SERVER_PORT 5001 /* 着信側の待ち受けポート番号 */
#define MAXPKTLEN   1004 /* 最大パケット長(バイト)     */

int main(int argc, char **argv)
{
    int sockfd;                           /*  ソケット設定に  */
    struct sockaddr_in servaddr, cliaddr; /*  必要な変数    　*/
    socklen_t len;
    int port_number;

    char recvpkt[MAXPKTLEN];      /* 受信パケット全体を格納する配列変数        */
    char recvdata[MAXPKTLEN - 4]; /* 受信パケットのデータ部を格納する配列変数   */
    char sendpkt[4];              /* ACK パケット全体を格納する4バイトの配列変数 */
    int seq_num = 0;              /* 受信パケットの順序番号を格納する変数       */
    int num_bytes_pkt = 0;        /* 受信パケット長                             */
    int num_bytes_data = 0;       /* 受信パケットのデータ部の長さ               */
    int total_num_bytes_data = 0;

    FILE *fp;
    char filename[50];
    int file_num = 1;
    /* UDPのソケットを設定 */

    port_number = SERVER_PORT;
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port_number);
    len = sizeof(cliaddr);

    while (bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
        servaddr.sin_port = htons(++port_number);

    if (SERVER_PORT != port_number)
        printf("PORT NUMBER : %d\n", port_number);

    /* 受信パケットのデータ部を書き出すファイルをオープン */

    sprintf(filename, "test%d.jpg", file_num);
    fp = fopen(filename, "wb");

    if (fp == NULL) {
        printf("File open error: %s\n", filename);
        return -1;
    }

    /* 相手からパケットを１つ受信して全体をrecvpktに格納 */

    /* 受信パケット長は num_bytes_pktに格納される       */
    while (1) {
        num_bytes_pkt = recvfrom(sockfd, recvpkt, MAXPKTLEN, 0,
                                 (struct sockaddr *)&cliaddr, &len);

        /* 受信パケット長 - 4 が パケットのデータ部の長さ */
        num_bytes_data = num_bytes_pkt - 4;
        total_num_bytes_data += num_bytes_data;

        /* 受信パケットの先頭4バイト(ヘッダ部)をseq_numにコピー */
        memcpy((char *)&seq_num, recvpkt, 4);

        /* 受信パケットの先頭から5バイト目以降末尾まで(データ部)を  */
        /* recvdataにコピー                                         */
        memcpy(recvdata, recvpkt + 4, num_bytes_data);
        printf("Receive : %d bytes, seq_num=%d\n", num_bytes_data, seq_num);

        /* sendpkt(長さ4バイト)にseq_num(長さ4バイト)の内容をコピー */
        memcpy(sendpkt, (char *)&seq_num, 4);
        /* sendpktの内容をACKパケットとして相手に送信 */
        sendto(sockfd, sendpkt, 4, 0,
               (struct sockaddr *)&cliaddr,
               sizeof(cliaddr));
        printf("ACK with seq_num %d is sent : \n", seq_num);

        /* recvdata の内容を ファイルに書き出し */
        fwrite(recvdata, num_bytes_data, 1, fp);
        if (seq_num == -1) {
            fclose(fp);
            file_num++;
            sprintf(filename, "test%d.jpg", file_num);
            fp = fopen(filename, "wb");
            if (fp == NULL) {
                printf("File open error: %s\n", filename);
                return -1;
            }
        }
        if (seq_num == 0) {
            break;
        }
    }
    fclose(fp);
    close(sockfd);

    printf("Total amount of received data : \
				%d bytes\n",
           total_num_bytes_data);

    return 0;
}
