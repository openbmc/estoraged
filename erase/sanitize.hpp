#pragma once

#include <functional>
#include <systemd/sd-journal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <linux/mmc/ioctl.h>
#include <chrono>
#include <thread>
#include <stdarg.h>

#define MMC_SWITCH 6
#define MMC_SWITCH_MODE_WRITE_BYTE 0x03
#define EXT_CSD_SANITIZE_START 165
#define EXT_CSD_CMD_SET_NORMAL (1<<0)

#define MMC_RSP_PRESENT  (1 << 0)
#define MMC_RSP_CRC      (1 << 2)
#define MMC_RSP_BUSY     (1 << 3)
#define MMC_RSP_OPCODE   (1 << 4)

#define MMC_CMD_AC       (0 << 5)

#define MMC_RSP_SPI_S1   (1 << 7)
#define MMC_RSP_SPI_BUSY (1 << 10)

#define MMC_RSP_SPI_R1 (MMC_RSP_SPI_S1)
#define MMC_RSP_SPI_R1B (MMC_RSP_SPI_S1|MMC_RSP_SPI_BUSY)

#define MMC_RSP_R1B (MMC_RSP_PRESENT|MMC_RSP_CRC|MMC_RSP_OPCODE|MMC_RSP_BUSY)

class sanitize{
  public:
    sanitize(std::string devPath, unsigned int delaySec, std::function<int (int, long unsigned, struct mmc_ioc_cmd)> inIOctl);
  };

// ioctl uses a variadic; wrapper_ioctl is easy to pass a std::function
int wrapper_ioctl(int fd, unsigned long request, struct mmc_ioc_cmd data);
