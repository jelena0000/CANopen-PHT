/*
 * CANopen main program file for CANopenNode on Linux.
 *
 * Ovdje je ubačen I2C kod za MS8607 (PHT Click) senzor
 * tako da se očitava temperatura i šalje preko CANopen-a.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sched.h>
#include <signal.h>
#include <errno.h>
#include <stdarg.h>
#include <syslog.h>
#include <time.h>
#include <sys/epoll.h>
#include <net/if.h>
#include <linux/reboot.h>
#include <sys/reboot.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>

#include "CANopen.h"
#include "OD.h"
#include "CO_error.h"
#include "CO_epoll_interface.h"
#include "CO_storageLinux.h"

#ifdef CO_USE_APPLICATION
#include "CO_application.h"
#endif

#if (CO_CONFIG_TRACE) & CO_CONFIG_TRACE_ENABLE
#include "CO_time_trace.h"
#endif

#ifndef MAIN_THREAD_INTERVAL_US
#define MAIN_THREAD_INTERVAL_US 100000
#endif
#ifndef TMR_THREAD_INTERVAL_US
#define TMR_THREAD_INTERVAL_US 1000
#endif

#ifndef NMT_CONTROL
#define NMT_CONTROL \
    CO_NMT_STARTUP_TO_OPERATIONAL | CO_NMT_ERR_ON_ERR_REG | CO_ERR_REG_GENERIC_ERR | CO_ERR_REG_COMMUNICATION
#endif
#ifndef FIRST_HB_TIME
#define FIRST_HB_TIME 500
#endif
#ifndef SDO_SRV_TIMEOUT_TIME
#define SDO_SRV_TIMEOUT_TIME 1000
#endif
#ifndef SDO_CLI_TIMEOUT_TIME
#define SDO_CLI_TIMEOUT_TIME 500
#endif
#ifndef SDO_CLI_BLOCK
#define SDO_CLI_BLOCK false
#endif
#ifndef OD_STATUS_BITS
#define OD_STATUS_BITS NULL
#endif
#ifndef GATEWAY_ENABLE
#define GATEWAY_ENABLE true
#endif
#ifndef TIME_STAMP_INTERVAL_MS
#define TIME_STAMP_INTERVAL_MS 10000
#endif
#ifndef CO_STORAGE_APPLICATION
#define CO_STORAGE_APPLICATION
#endif
#ifndef CO_STORAGE_AUTO_INTERVAL
#define CO_STORAGE_AUTO_INTERVAL 60000000
#endif

CO_t* CO = NULL;
static uint8_t CO_activeNodeId = CO_LSS_NODE_ID_ASSIGNMENT;

volatile sig_atomic_t CO_endProgram = 0;

/* ---------------- I2C MS8607 DEFINICIJE ---------------- */
#define MS8607_ADDR_PRESS_TEMP 0x76
#define RESET_CMD   0x1E
#define CONVERT_D2  0x50
#define ADC_READ    0x00
#define PROM_READ   0xA0

static int i2c_fd;
static uint16_t C[8];  // kalibracioni koeficijenti

/* I2C helper */
static int i2c_transfer(int fd, struct i2c_msg *msgs, int n) {
    struct i2c_rdwr_ioctl_data xfer = { .msgs = msgs, .nmsgs = n };
    return ioctl(fd, I2C_RDWR, &xfer);
}

static uint32_t read_adc(int fd, uint8_t cmd) {
    uint8_t c = cmd | 0x08; // OSR=4096
    struct i2c_msg m1 = { .addr=MS8607_ADDR_PRESS_TEMP, .flags=0, .len=1, .buf=&c };
    struct i2c_rdwr_ioctl_data x1 = { .msgs=&m1, .nmsgs=1 };
    ioctl(fd, I2C_RDWR, &x1);
    usleep(20000);

    uint8_t buf[3], rcmd=ADC_READ;
    struct i2c_msg msgs[2] = {
        { .addr=MS8607_ADDR_PRESS_TEMP, .flags=0, .len=1, .buf=&rcmd },
        { .addr=MS8607_ADDR_PRESS_TEMP, .flags=I2C_M_RD, .len=3, .buf=buf }
    };
    i2c_transfer(fd, msgs, 2);
    return (buf[0]<<16)|(buf[1]<<8)|buf[2];
}

static float read_temperature() {
    uint32_t D2 = read_adc(i2c_fd, CONVERT_D2);
    int32_t dT   = D2 - (uint32_t)C[5]*256;
    int32_t TEMP = 2000 + ((int64_t)dT * C[6]) / 8388608;
    return TEMP / 100.0; // °C
}

/* ------------------------------------------------------- */

static void sigHandler(int sig) {
    (void)sig;
    CO_endProgram = 1;
}

void log_printf(int priority, const char* format, ...) {
    va_list ap;
    va_start(ap, format);
    vsyslog(priority, format, ap);
    va_end(ap);
}

#ifndef CO_SINGLE_THREAD
CO_epoll_t epRT;
static void* rt_thread(void* arg);
#endif

/* ---------- NOVO: DEFINICIJE ZA PROSJEK TEMPERATURE ---------- */
#define AVG_INTERVAL_SEC 3
static float temp_sum = 0;
static int temp_count = 0;
static time_t last_avg_time = 0;

int main(int argc, char* argv[]) {
    int programExit = EXIT_SUCCESS;
    CO_epoll_t epMain;
#ifndef CO_SINGLE_THREAD
    pthread_t rt_thread_id;
#endif
    CO_NMT_reset_cmd_t reset = CO_RESET_NOT;
    CO_ReturnError_t err;
    CO_CANptrSocketCan_t CANptr = {0};
    bool_t firstRun = true;

    char* CANdevice = NULL;

    /* configure system log */
    setlogmask(LOG_UPTO(LOG_DEBUG));
    openlog(argv[0], LOG_PID | LOG_PERROR, LOG_USER);

    if (argc < 2 || strcmp(argv[1], "--help") == 0) {
        printf("Usage: %s <CAN device name>\n", argv[0]);
        exit(EXIT_SUCCESS);
    }

    CANdevice = argv[1];
    CANptr.can_ifindex = if_nametoindex(CANdevice);
    if (CANptr.can_ifindex == 0) {
        printf("No CAN device: %s\n", CANdevice);
        exit(EXIT_FAILURE);
    }

    err = CO_epoll_create(&epMain, MAIN_THREAD_INTERVAL_US);
    if (err != CO_ERROR_NO) {
        printf("CO_epoll_create failed\n");
        exit(EXIT_FAILURE);
    }
#ifdef CO_SINGLE_THREAD
    CANptr.epoll_fd = epMain.epoll_fd;
#else
    err = CO_epoll_create(&epRT, TMR_THREAD_INTERVAL_US);
    if (err != CO_ERROR_NO) {
        printf("CO_epoll_create(RT) failed\n");
        exit(EXIT_FAILURE);
    }
    CANptr.epoll_fd = epRT.epoll_fd;
#endif

    uint32_t heapMemoryUsed = 0;
    CO = CO_new(NULL, &heapMemoryUsed);
    if (CO == NULL) {
        printf("CO_new failed\n");
        exit(EXIT_FAILURE);
    }

    signal(SIGINT, sigHandler);
    signal(SIGTERM, sigHandler);

    /* ---------- INIT I2C SENZORA ---------- */
    i2c_fd = open("/dev/i2c-1", O_RDWR);
    if (i2c_fd < 0) { perror("open i2c"); exit(1); }

    // Reset senzora
    uint8_t rst = RESET_CMD;
    struct i2c_msg rm = { .addr=MS8607_ADDR_PRESS_TEMP, .flags=0, .len=1, .buf=&rst };
    i2c_transfer(i2c_fd, &rm, 1);
    usleep(10000);

    // Učitaj kalibraciju
    for (int i=0; i<8; i++) {
        uint8_t reg = PROM_READ + (i<<1);
        uint8_t buf[2];
        struct i2c_msg msgs[2] = {
            { .addr=MS8607_ADDR_PRESS_TEMP, .flags=0, .len=1, .buf=&reg },
            { .addr=MS8607_ADDR_PRESS_TEMP, .flags=I2C_M_RD, .len=2, .buf=buf }
        };
        i2c_transfer(i2c_fd, msgs, 2);
        C[i] = (buf[0]<<8)|buf[1];
    }
    /* -------------------------------------- */

    while (reset != CO_RESET_APP && reset != CO_RESET_QUIT && CO_endProgram == 0) {
        if (!firstRun) {
            CO_LOCK_OD(CO->CANmodule);
            CO->CANmodule->CANnormal = false;
            CO_UNLOCK_OD(CO->CANmodule);
        }

        CO_CANsetConfigurationMode((void*)&CANptr);
        CO_CANmodule_disable(CO->CANmodule);

        err = CO_CANinit(CO, (void*)&CANptr, 0);
        if (err != CO_ERROR_NO) { printf("CO_CANinit failed\n"); exit(EXIT_FAILURE); }

        err = CO_CANopenInit(CO, NULL, NULL, OD, OD_STATUS_BITS, NMT_CONTROL,
                             FIRST_HB_TIME, SDO_SRV_TIMEOUT_TIME,
                             SDO_CLI_TIMEOUT_TIME, SDO_CLI_BLOCK,
                             1, NULL);
        if (err != CO_ERROR_NO) { printf("CO_CANopenInit failed\n"); exit(EXIT_FAILURE); }

        CO_epoll_initCANopenMain(&epMain, CO);

        if (firstRun) {
            firstRun = false;
#ifndef CO_SINGLE_THREAD
            if (pthread_create(&rt_thread_id, NULL, rt_thread, NULL) != 0) {
                printf("pthread_create failed\n");
                exit(EXIT_FAILURE);
            }
#endif
        }

        err = CO_CANopenInitPDO(CO, CO->em, OD, 1, NULL);
        if (err != CO_ERROR_NO) { printf("CO_CANopenInitPDO failed\n"); exit(EXIT_FAILURE); }

        CO_CANsetNormalMode(CO->CANmodule);

        reset = CO_RESET_NOT;
        printf("CANopenNode running...\n");

        while (reset == CO_RESET_NOT && CO_endProgram == 0) {
            CO_epoll_wait(&epMain);
#ifdef CO_SINGLE_THREAD
            CO_epoll_processRT(&epMain, CO, false);
#endif
            CO_epoll_processMain(&epMain, CO, GATEWAY_ENABLE, &reset);
            CO_epoll_processLast(&epMain);

            /* ---- ČITANJE SENZORA ---- */
            float temp = read_temperature();
            OD_RAM.x2000_temperature = (int16_t) temp;

            /* ---- AKUMULACIJA ZA PROSJEK ---- */
            temp_sum += temp;
            temp_count++;

            time_t now = time(NULL);
            if (now - last_avg_time >= AVG_INTERVAL_SEC) {
                float avrg = temp_count ? temp_sum / temp_count : 0.0;
                printf("Prosjecna temperatura (%d sec) = %.2f C\n", AVG_INTERVAL_SEC, avrg);
                fflush(stdout);

                /* Reset za sljedeći interval */
                temp_sum = 0;
                temp_count = 0;
                last_avg_time = now;
            }
        }
    }

    CO_endProgram = 1;
#ifndef CO_SINGLE_THREAD
    pthread_join(rt_thread_id, NULL);
    CO_epoll_close(&epRT);
#endif
    CO_epoll_close(&epMain);
    CO_delete(CO);

    close(i2c_fd);

    printf("CANopenNode finished\n");
    exit(programExit);
}

#ifndef CO_SINGLE_THREAD
static void* rt_thread(void* arg) {
    (void)arg;
    while (CO_endProgram == 0) {
        CO_epoll_wait(&epRT);
        CO_epoll_processRT(&epRT, CO, true);
        CO_epoll_processLast(&epRT);
    }
    return NULL;
}
#endif

