#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "merry/merry.h"
#include "merry/common/actionmoni-client.h"
#include "merry/common/smp.h"
#include "worker.h"

static void on_master_exit_handler()
{
    LOGF(ALERT, "master exited");
}

static void master_main()
{
    //printf("master\n");
}

static void help()
{
    printf("--------------------------------------------------------------------------------\n"
           "This is %s/%s , Usage:\n"
           "\n"
           "    %s [options]\n"
           "\n"
           "Options:\n"
           "\n"
           "    --bind=127.0.0.1:%d\tserver bind. or --bind=%d for bind at 0.0.0.0:%d\n"
           "    --log=path[,level] \t\tlog file path, level=1-6\n"
           "    --daemon[=n]     \t\tdaemon mode(start n workers)\n"
           "  \n"
           "--------------------------------------------------------------------------------\n",
           program_name, "0.1", program_name, bind_port, bind_port, bind_port
          );
}

int main(int argc, const char **argv)
{
    return merry_start(argc, argv, help, master_main, on_master_exit_handler,
                       worker_main, 0);
}
