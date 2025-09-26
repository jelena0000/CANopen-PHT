#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>

#define MS8607_ADDR_PRESS_TEMP 0x76
#define MS8607_ADDR_HUM        0x40

// Commands
#define RESET_CMD   0x1E
#define CONVERT_D1  0x40 // Pressure
#define CONVERT_D2  0x50 // Temperature
#define ADC_READ    0x00
#define PROM_READ   0xA0
#define HUM_RESET   0xFE
#define HUM_MEASURE 0xE5

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

int main() {
    int fd = open("/dev/i2c-1", O_RDWR);
    if (fd < 0) { perror("open"); return 1; }

    // Reset MS8607
    uint8_t rst = RESET_CMD;
    struct i2c_msg rm = { .addr=MS8607_ADDR_PRESS_TEMP, .flags=0, .len=1, .buf=&rst };
    i2c_transfer(fd, &rm, 1);
    usleep(10000);

    // Read calibration PROM (8 words)
    uint16_t C[8];
    int i;
    for (i=0; i<8; i++) {
        uint8_t reg = PROM_READ + (i<<1);
        uint8_t buf[2];
        struct i2c_msg msgs[2] = {
            { .addr=MS8607_ADDR_PRESS_TEMP, .flags=0, .len=1, .buf=&reg },
            { .addr=MS8607_ADDR_PRESS_TEMP, .flags=I2C_M_RD, .len=2, .buf=buf }
        };
        i2c_transfer(fd, msgs, 2);
        C[i] = (buf[0]<<8)|buf[1];
    }

    // Reset humidity sensor
    uint8_t h_rst = HUM_RESET;
    struct i2c_msg hm = { .addr=MS8607_ADDR_HUM, .flags=0, .len=1, .buf=&h_rst };
    i2c_transfer(fd, &hm, 1);
    usleep(15000);

    while (1) {
        // Pressure and temperature
            int count = 0;
    float sum_temp = 0, sum_pres = 0, sum_hum = 0;

    while (1) {
        // Pressure and temperature
        uint32_t D1 = read_adc(fd, CONVERT_D1);
        uint32_t D2 = read_adc(fd, CONVERT_D2);

        int32_t dT   = D2 - (uint32_t)C[5]*256;
        int32_t TEMP = 2000 + ((int64_t)dT * C[6]) / 8388608;
        int64_t OFF  = (int64_t)C[2]*131072 + ((int64_t)C[4]*dT)/64;
        int64_t SENS = (int64_t)C[1]*65536 + ((int64_t)C[3]*dT)/128;
        int32_t P    = ((D1*SENS)/2097152 - OFF)/32768;

        float temp_c = TEMP / 100.0;
        float pres_mbar = P / 100.0;

        // Humidity
        uint8_t cmd = HUM_MEASURE;
        uint8_t hbuf[3];
        struct i2c_msg hmsgs[2] = {
            { .addr=MS8607_ADDR_HUM, .flags=0, .len=1, .buf=&cmd },
            { .addr=MS8607_ADDR_HUM, .flags=I2C_M_RD, .len=3, .buf=hbuf }
        };
        i2c_transfer(fd, hmsgs, 2);

        uint16_t raw_hum = (hbuf[0]<<8)|hbuf[1];
        raw_hum &= 0xFFFC;
        float hum = -6 + 125.0*raw_hum/65536.0;

        // Saberi merenja
        sum_temp += temp_c;
        sum_pres += pres_mbar;
        sum_hum  += hum;
        count++;

        // Kad se skupi 5 uzoraka (5 sekundi)
        if (count == 5) {
            printf("Avg Temp: %.2f C | Avg Pressure: %.2f mbar | Avg Humidity: %.2f %%\n",
                   sum_temp/5.0, sum_pres/5.0, sum_hum/5.0);

            // Reset za sledeći ciklus
            count = 0;
            sum_temp = sum_pres = sum_hum = 0;
        }

        sleep(1);  // i dalje meri svake sekunde
    }

    }

    close(fd);
    return 0;
}

