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
    epd->timeout_ptr = add_timeout(epd, 1000, timeout_handle);

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
    printf("connect %d\n", se_connect(loop_fd, aa, 80, 3000, be_connect, aa));
}

/* libeio test */
static int eio_test_step = 0;
void eio_test();

int res_cb(eio_req *req)
{
    eio_test();
    printf("res_cb(%d|%s) = %d\n", req->type, req->data ? req->data : "?", EIO_RESULT(req));

    if(req->result < 0) {
        abort();
    }

    return 0;
}

int readdir_cb(eio_req *req)
{
    eio_test();
    char *buf = (char *)EIO_BUF(req);

    printf("readdir_cb = %d\n", EIO_RESULT(req));

    if(EIO_RESULT(req) < 0) {
        return 0;
    }

    while(EIO_RESULT(req)--) {
        printf("readdir = <%s>\n", buf);
        buf += strlen(buf) + 1;
    }

    return 0;
}

int stat_cb(eio_req *req)
{
    eio_test();
    struct stat *buf = EIO_STAT_BUF(req);

    if(req->type == EIO_FSTAT) {
        printf("fstat_cb = %d\n", EIO_RESULT(req));

    } else {
        printf("stat_cb(%s) = %d\n", EIO_PATH(req), EIO_RESULT(req));
    }

    if(!EIO_RESULT(req)) {
        printf("stat size %d perm 0%o\n", buf->st_size, buf->st_mode & 0777);
    }

    return 0;
}

int read_cb(eio_req *req)
{
    eio_test();
    unsigned char *buf = (unsigned char *)EIO_BUF(req);

    printf("read_cb = %d (%02x%02x%02x%02x %02x%02x%02x%02x)\n",
           EIO_RESULT(req),
           buf [0], buf [1], buf [2], buf [3],
           buf [4], buf [5], buf [6], buf [7]);

    return 0;
}

int last_fd;

int open_cb(eio_req *req)
{
    eio_test();
    printf("open_cb = %d\n", EIO_RESULT(req));

    last_fd = EIO_RESULT(req);

    return 0;
}

void eio_test()
{
    /* avoid relative paths yourself(!) */
    printf("step: %d\n", eio_test_step);
    int s = eio_test_step++;

    switch(s) {
        case 0:
            eio_mkdir("/tmp/eio-test-dir", 0777, 0, res_cb, "mkdir");
            break;

        case 1: {
            eio_nop(0, res_cb, "nop");
        };

        break;

        case 2:
            eio_stat("/tmp/eio-test-dir", 0, stat_cb, "stat");
            break;

        case 3:
            eio_lstat("/tmp/eio-test-dir", 0, stat_cb, "stat");
            break;

        case 4:
            eio_open("/tmp/eio-test-dir/eio-test-file", O_RDWR | O_CREAT, 0777, 0, open_cb, "open");
            break;

        case 5:
            eio_symlink("test", "/tmp/eio-test-dir/eio-symlink", 0, res_cb, "symlink");
            break;

        case 6:
            eio_mknod("/tmp/eio-test-dir/eio-fifo", S_IFIFO, 0, 0, res_cb, "mknod");
            break;

        case 7:
            eio_utime("/tmp/eio-test-dir", 12345.678, 23456.789, 0, res_cb, "utime");
            break;

        case 8:
            eio_futime(last_fd, 92345.678, 93456.789, 0, res_cb, "futime");
            break;

        case 9:
            eio_chown("/tmp/eio-test-dir", getuid(), getgid(), 0, res_cb, "chown");
            break;

        case 10:
            eio_fchown(last_fd, getuid(), getgid(), 0, res_cb, "fchown");
            break;

        case 11:
            eio_fchmod(last_fd, 0723, 0, res_cb, "fchmod");
            break;

        case 12:
            eio_readdir("/tmp/eio-test-dir", 0, 0, readdir_cb, "readdir");
            break;

        case 13:
            eio_readdir("/nonexistant", 0, 0, readdir_cb, "readdir");
            break;

        case 14:
            eio_fstat(last_fd, 0, stat_cb, "stat");
            break;

        case 15: {
            eio_write(last_fd, "test\nfail\n", 10, 4, 0, res_cb, "write");
        };

        break;

        case 16:
            eio_read(last_fd, 0, 8, 0, EIO_PRI_DEFAULT, read_cb, "read");
            break;

        case 17:
            eio_readlink("/tmp/eio-test-dir/eio-symlink", 0, res_cb, "readlink");
            break;

        case 18:
            eio_dup2(1, 2, EIO_PRI_DEFAULT, res_cb, "dup");
            break;  // dup stdout to stderr

        case 19:
            eio_chmod("/tmp/eio-test-dir", 0765, 0, res_cb, "chmod");
            break;

        case 20:
            eio_ftruncate(last_fd, 9, 0, res_cb, "ftruncate");
            break;

        case 21:
            eio_fdatasync(last_fd, 0, res_cb, "fdatasync");
            break;

        case 22:
            eio_fsync(last_fd, 0, res_cb, "fsync");
            break;

        case 23:
            eio_sync(0, res_cb, "sync");
            break;

        case 24:
            eio_busy(0.5, 0, res_cb, "busy");
            break;

        case 25:
            eio_sendfile(1, last_fd, 4, 5, 0, res_cb, "sendfile");
            break;  // write "test\n" to stdout

        case 26:
            eio_fstat(last_fd, 0, stat_cb, "stat");
            break;

        case 27:
            eio_truncate("/tmp/eio-test-dir/eio-test-file", 6, 0, res_cb, "truncate");
            break;

        case 28:
            eio_readahead(last_fd, 0, 64, 0, res_cb, "readahead");
            break;

        case 29:
            eio_close(last_fd, 0, res_cb, "close");
            break;

        case 30:
            eio_link("/tmp/eio-test-dir/eio-test-file", "/tmp/eio-test-dir/eio-test-file-2", 0, res_cb, "link");
            break;

        case 31:
            eio_rename("/tmp/eio-test-dir/eio-test-file", "/tmp/eio-test-dir/eio-test-file-renamed", 0, res_cb, "rename");
            break;

        case 32:
            eio_unlink("/tmp/eio-test-dir/eio-fifo", 0, res_cb, "unlink");
            break;

        case 33:
            eio_unlink("/tmp/eio-test-dir/eio-symlink", 0, res_cb, "unlink");
            break;

        case 34:
            eio_unlink("/tmp/eio-test-dir/eio-test-file-2", 0, res_cb, "unlink");
            break;

        case 35:
            eio_unlink("/tmp/eio-test-dir/eio-test-file-renamed", 0, res_cb, "unlink");
            break;

        case 36:
            eio_rmdir("/tmp/eio-test-dir", 0, res_cb, "rmdir");
            break;
    }
}
/* end */
int a = 0;
static int other_simple_jobs()
{
    if(a++ < 1) {
        char *aa = "www.163.com";

        printf("dns query %d\n", se_dns_query(loop_fd, aa, 3000, be_dns_query, aa));

        do {
            eio_test();
        } while(0);
    }

    return 1; // return 0 will be exit the worker
}

static int be_read(se_ptr_t *ptr)
{
    epdata_t *epd = ptr->data;

    int n = recv(epd->fd, &buf_4096, 4096, 0);

    if(n > 0) {
        update_timeout(epd->timeout_ptr, 1000);

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
    epd->timeout_ptr = add_timeout(epd, 1000, timeout_handle);

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
