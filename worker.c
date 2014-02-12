#include "merry/merry.h"
#include "worker.h"

int worker_n = 0;
static char buf_4096[4096] = {0};

static void on_exit_handler()
{
    LOGF(ALERT, "worker %d exited", worker_n);
}

static void close_client(epdata_t *epd)
{
    se_delete(epd->se_ptr);
    delete_timeout(epd->timeout_ptr);

    close(epd->fd);

    smp_free(epd);
}

static void timeout_handle(void *ptr)
{
    epdata_t *epd = ptr;
    close_client(epd);
}

static int client_be_read(se_ptr_t *ptr)
{
    epdata_t *epd = ptr->data;
    int n = 0;

    while((n = recv(epd->fd, buf_4096, 4095, 0)) > 0) {
        buf_4096[n] = '\0';
        //printf("%s", buf_4096);
        printf("%d\n", n);
    }

    if(n == 0 || (n < 0 && NOAGAIN)) {
        close_client(epd);
    }

    return 1;
}

static int client_be_write(se_ptr_t *ptr)
{
    printf("client_be_write\n");
    epdata_t *epd = ptr->data;
    int n = 0;

    while((n = send(epd->fd, epd->out_buf + epd->out_buf_sended,
                    epd->out_buf_len - epd->out_buf_sended, MSG_DONTWAIT)) > 0) {
        epd->out_buf_sended += n;
    }

    printf("send %d\n", n);

    if(epd->out_buf_sended == epd->out_buf_len) {
        se_be_read(epd->se_ptr, client_be_read);

    } else if(n < 0 && NOAGAIN)  {
        close_client(epd);
    }

    return 1;
}


void be_connect(void *data, int fd)
{
    printf("connected %s %d\n", (char *)data, fd);
    epdata_t *epd = NULL;
    epd = smp_malloc(sizeof(epdata_t));

    if(!epd) {
        close(fd);
        return;
    }

    epd->fd = fd;
    epd->stime = now;

    epd->se_ptr = se_add(loop_fd, fd, epd);
    epd->timeout_ptr = add_timeout(epd, 10, timeout_handle);

    epd->out_buf = smp_malloc(1024);
    epd->out_buf_len = sprintf(epd->out_buf,
                               "GET / HTTP/1.1\r\nUser-Agent: curl/7.24.0\r\nHost: www.163.com\r\nAccept: */*\r\n\r\n");
    epd->out_buf_sended = 0;

    se_be_read(epd->se_ptr, client_be_write);
    client_be_write(epd->se_ptr);
}

void be_dns_query(void *data, struct sockaddr_in addr)
{
    printf("%s %s\n", (char *)data, inet_ntoa(addr.sin_addr));
    char *aa = "www.163.com";
    printf("%d\n", se_connect(loop_fd, aa, 80, 3, be_connect, aa));
}

int a = 0;
static int other_simple_jobs()
{
    if(a++ < 1) {
        char *aa = "www.163.com";
        printf("%d\n", se_dns_query(loop_fd, aa, 10, be_dns_query, aa));
    }

    return 1; // return 0 will be exit the worker
}

static int be_read(se_ptr_t *ptr)
{
    epdata_t *epd = ptr->data;

    int n = recv(epd->fd, &buf_4096, 4096, 0);

    if(n > 0) {
        update_timeout(epd->timeout_ptr, 10);

        if(network_raw_send(epd->fd, (const char *)&buf_4096, n)) {
            if(buf_4096[n - 1] == '\n') {
                buf_4096[n - 1] = '\0';
            }

            LOGF(INFO, "%s", buf_4096);

        } else {
            LOGF(ERR, "send error!\n");

            se_delete(epd->se_ptr);

            if(epd->fd > -1) {
                close(epd->fd);
                epd->fd = -1;
            }

            free(epd);
        }

    } else if(n == 0 || (n < 0 && NOAGAIN)) { // close connection
        se_delete(epd->se_ptr);

        if(epd->fd > -1) {
            close(epd->fd);
            epd->fd = -1;
        }

        free(epd);

    } else {
        LOGF(ERR, "read error!\n");
    }

    return 1;
}

static void be_accept(int client_fd, struct in_addr client_addr)
{
    if(!set_nonblocking(client_fd, 1)) {
        close(client_fd);
        return;
    }

    epdata_t *epd = NULL;
    epd = smp_malloc(sizeof(epdata_t));

    if(!epd) {
        close(client_fd);
        return;
    }

    epd->fd = client_fd;
    epd->client_addr = client_addr;
    epd->stime = now;

    epd->se_ptr = se_add(loop_fd, client_fd, epd);
    epd->timeout_ptr = add_timeout(epd, 10, timeout_handle);

    se_be_read(epd->se_ptr, be_read);
}

void worker_main(int _worker_n)
{
    worker_n = _worker_n;
    attach_on_exit(on_exit_handler);
    set_process_user(/*user*/ NULL, /*group*/ NULL);

    /// 进入 loop 处理循环
    loop_fd = se_create(4096);
    se_accept(loop_fd, server_fd, be_accept);

    se_loop(loop_fd, 10, other_simple_jobs); // loop

    exit(0);
}
