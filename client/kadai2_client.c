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
    int sockfd;
    struct sockaddr_in servaddr;

    char reqpkt[MAXPKTLEN] = ""; /* 要求パケット(空白区切りのファイル名)   */
    char recvpkt[MAXPKTLEN];      /* 受信パケット全体を格納する配列変数        */
    char recvdata[MAXPKTLEN - 4]; /* 受信パケットのデータ部を格納する配列変数   */
    char sendpkt[4];              /* ACK パケット全体を格納する4バイトの配列変数 */
    int seq_num = 0;              /* 受信パケットの順序番号を格納する変数       */
    int num_bytes_pkt = 0;        /* 受信パケット長                             */
    int num_bytes_data = 0;       /* 受信パケットのデータ部の長さ               */
    int total_num_bytes_data = 0;

    FILE *fp;
    int file_num = 2; /* いま受信しているファイル名が入っている argv の番号 */
    int i;

    if (argc < 3) {
        printf("usage:  %s <IPaddress> <file1> (<file2> ...)\n", argv[0]);
        return -1;
    }

    /* コマンド実行時に引数として与えるIPアドレスを相手のIPアドレスとして設定 */
    memset(&servaddr, 0, sizeof(servaddr));
    inet_pton(AF_INET, argv[1], &servaddr.sin_addr);
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(SERVER_PORT);

    /* UDPのソケットを設定 */
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    if (sockfd < 0) {
        perror("Socket open error:\n");
        return -1;
    }

    /* argv[2] 以降のファイル名を空白で区切ってつなげ，要求パケットを作る */
    for (i = 2; i < argc; i++) {
        if (strlen(reqpkt) + strlen(argv[i]) + 1 >= MAXPKTLEN) {
            printf("Too many file names\n");
            return -1;
        }
        if (i > 2)
            strcat(reqpkt, " ");
        strcat(reqpkt, argv[i]);
    }

    /* 要求パケットをサーバに送信 */
    sendto(sockfd, reqpkt, strlen(reqpkt), 0,
           (struct sockaddr *)&servaddr,
           sizeof(servaddr));
    printf("Request : %s\n", reqpkt);

    /* 受信パケットのデータ部を書き出すファイルを，指定した名前でオープン */
    if (NULL == (fp = fopen(argv[file_num], "wb"))) {
        perror(argv[file_num]);
        return -1;
    }

    while (1) {
        /* 相手からパケットを1つ受信して全体をrecvpktに格納 */
        num_bytes_pkt = recvfrom(sockfd, recvpkt, MAXPKTLEN, 0, NULL, NULL);

        /* 受信パケット長 - 4 が パケットのデータ部の長さ */
        num_bytes_data = num_bytes_pkt - 4;
        total_num_bytes_data += num_bytes_data;

        /* 受信パケットの先頭4バイト(ヘッダ部)をseq_numに，残りをrecvdataにコピー */
        memcpy((char *)&seq_num, recvpkt, 4);
        memcpy(recvdata, recvpkt + 4, num_bytes_data);
        printf("Receive : %d bytes, seq_num=%d\n", num_bytes_data, seq_num);

        /* seq_numをそのままACKパケットとしてサーバに送信 */
        memcpy(sendpkt, (char *)&seq_num, 4);
        sendto(sockfd, sendpkt, 4, 0,
               (struct sockaddr *)&servaddr,
               sizeof(servaddr));
        printf("ACK with seq_num %d is sent : \n", seq_num);

        /* recvdata の内容を ファイルに書き出す */
        fwrite(recvdata, num_bytes_data, 1, fp);

        if (seq_num == 0) /* 最後のファイルの最後のパケットなら終了 */
            break;
        if (seq_num == -1) { /* ファイルの最後のパケットなら，次のファイルをオープン */
            fclose(fp);
            if (++file_num >= argc || NULL == (fp = fopen(argv[file_num], "wb"))) {
                printf("File open error\n");
                return -1;
            }
        }
    }
    fclose(fp);
    close(sockfd);

    printf("Total amount of received data : %d bytes\n", total_num_bytes_data);

    return 0;
}
