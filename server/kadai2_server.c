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
#define MAXFILES    100  /* 一度に要求できるファイルの最大数 */

int GetFileSize(FILE *fp);

int main(int argc, char **argv)
{
    int sockfd;
    struct sockaddr_in servaddr, cliaddr;
    socklen_t len;
    int port_number;
    struct timeval tv; /* 受信タイムアウトの待ち時間を設定する変数 */

    FILE *fp;      /* 読み込むファイルの識別子   */
    long filesize; /* ファイルサイズ             */

    int num_bytes_data = 0; /* 相手に送信するデータの長さ */
    int send_seq_num = 1;   /* パケットの順序番号         */
    int num_bytes_pkt = 0;  /* 相手に送信するパケットの長さ */

    char sendpkt[MAXPKTLEN];      /* パケット全体を格納する配列変数 */
    char senddata[MAXPKTLEN - 4]; /* パケットのデータ部を格納する配列変数 */

    char recvpkt[MAXPKTLEN]; /* 相手から受信するパケット全体を格納する配列変数 */
    int num_bytes_recv;      /* 相手から受信するパケットの長さ情報を格納する変数 */

    char *filenames[MAXFILES]; /* クライアントが要求したファイル名 */
    int n = 0, i;

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

    /* クライアントから要求パケット(空白区切りのファイル名)を受信 */
    /* 送り返す相手のアドレスは cliaddr に格納される              */
    num_bytes_recv = recvfrom(sockfd, recvpkt, MAXPKTLEN - 1, 0,
                              (struct sockaddr *)&cliaddr, &len);
    if (num_bytes_recv <= 0) {
        printf("Request receive error\n");
        return -1;
    }
    recvpkt[num_bytes_recv] = '\0';
    printf("Request : %s\n", recvpkt);

    /* 要求をファイル名ごとに分けて filenames に格納 */
    while (n < MAXFILES && NULL != (filenames[n] = strtok(n == 0 ? recvpkt : NULL, " ")))
        n++;

    if (n == 0) {
        printf("No file is requested\n");
        return -1;
    }

    /* 受信タイムアウト時間の設定(ACK待ちに使う) */
    tv.tv_sec = 0;       /* 秒単位 */
    tv.tv_usec = 750000; /* マイクロ秒単位 750ms*/
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    for (i = 0; i < n; i++) {
        /* 相手に送信する画像ファイルをオープン */
        if (NULL == (fp = fopen(filenames[i], "rb"))) {
            perror(filenames[i]);
            return -1;
        }

        /* ファイルサイズ (バイト数) を取得 */
        filesize = GetFileSize(fp);
        printf("%s: File size is %ld bytes\n", filenames[i], filesize);

        send_seq_num = 1;
        while (1) {
            /* 1000バイトより大きければ1000バイト，そうでなければ残り全部を送る */
            /* 最後のパケットは，最後のファイルなら0，それ以外なら-1とする     */
            if (filesize > 1000) {
                num_bytes_data = 1000;
            } else {
                num_bytes_data = filesize;
                send_seq_num = (i == n - 1) ? 0 : -1;
            }

            /* num_bytes_dataの長さのデータをファイルから読み込んで senddataに格納 */
            fread(senddata, num_bytes_data, 1, fp);

            /* sendpktの先頭4バイトに順序番号，5バイト目以降にデータをコピー */
            memcpy(sendpkt, (char *)&send_seq_num, 4);
            memcpy(sendpkt + 4, senddata, num_bytes_data);

            /* sendpktの先頭から num_bytes_pkt バイトをクライアントに送信 */
            num_bytes_pkt = num_bytes_data + 4;
            sendto(sockfd, sendpkt, num_bytes_pkt, 0,
                   (struct sockaddr *)&cliaddr,
                   sizeof(cliaddr));

            printf("Sending data of %d bytes, seq_num=%d....\n", num_bytes_data, send_seq_num);
            num_bytes_recv = recvfrom(sockfd, recvpkt, MAXPKTLEN, 0, NULL, NULL);

            if (num_bytes_recv > 0) /* num_bytes_recvが0より大きい場合，正常に受信している*/
                printf("ACK received.\n");
            else if (num_bytes_recv < 0) /* num_bytes_recvが負の場合，タイムアウト */
                printf("ACK timeout.\n");
            else /* num_bytes_recv = 0の場合はタイムアウト以外のエラー*/
                printf("ACK error.\n");

            if (send_seq_num <= 0) /* ファイルの最後のパケットを送り終えたら次へ */
                break;
            send_seq_num++;
            filesize -= num_bytes_data;
        }

        fclose(fp);
    }

    close(sockfd);
    return 0;
}

int GetFileSize(FILE *fp) /* 引数で与えるファイルのサイズを出力する関数 */
{
    int filesize;

    fseek(fp, 0L, SEEK_END);
    filesize = ftell(fp);
    rewind(fp);

    return filesize;
}
