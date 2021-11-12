#include "pattern.hpp"
#include "verifyGeometry.hpp"
#include <systemd/sd-journal.h>
#include <random>
//#include <sys/types.h>
//#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#define SEED 0x6a656272

pattern::pattern(std::string devName) : m_devName(devName)
  {}

int pattern::writePattern()
{
  if (this->openAndGetSize() !=0){
    return -1;
  }
  // static seed defines a fixed prng sequnce so it can be verified later,
  // and validated for entropy
  unsigned long currentIndex = 0;
  unsigned long seed = SEED;
  std::minstd_rand0 generator(seed);
  while (currentIndex - 4 < m_driveSize){
    const unsigned long rand = generator();
    if (write(m_fd, static_cast<const void*>(&rand), sizeof(long)) != sizeof(long)){
      sd_journal_send("MESSAGE=%s", "Estoraged erase pattern unable to write 4",
                          "REDFISH_MESSAGE_ID=%s","eStorageD.1.0.EraseFailure",
                          "REDFISH_MESSAGE_ARGS=%d", m_fd);
    return -1;
    }
    currentIndex = currentIndex + 4;
  }
  while (currentIndex < m_driveSize){
    const uint8_t rand = static_cast<uint8_t>(generator());
    if (write(m_fd, static_cast<const void*>(&rand),1 ) != 1){
      sd_journal_send("MESSAGE=%s", "Estoraged erase pattern unable to write 1",
                          "REDFISH_MESSAGE_ID=%s","eStorageD.1.0.EraseFailure",
                          "REDFISH_MESSAGE_ARGS=%d", m_fd);
    return -1;
    }
    currentIndex++;
  }
  close(m_fd);
  return 0;

}

int pattern::verifyPattern()
{
  if (openAndGetSize() !=0){
    return -1;
  }
  // static seed defines a fixed prng sequnce so it can be verified later,
  // and validated for entropy
  unsigned long currentIndex = 0;
  unsigned long seed = SEED;
  std::minstd_rand0 generator(seed);

  while (currentIndex - 4 < m_driveSize){
    //read 4 bytes

    unsigned long data_long;
    if (read(m_fd, static_cast<void*>(&data_long), sizeof(long) != 4)){
       sd_journal_send("MESSAGE=%s", "Estoraged erase pattern unable to read 4",
                         "REDFISH_MESSAGE_ID=%s","eStorageD.1.0.EraseFailure",
                          "REDFISH_MESSAGE_ARGS=%d", m_fd);
      return -1;
    }
    // compare 4 bytes
    unsigned long rand = generator();
    if (rand != data_long){
      sd_journal_send("MESSAGE=%s", "Estoraged erase pattern does not match",
                            "REDFISH_MESSAGE_ID=%s","eStorageD.1.0.EraseFailure",
                          "REDFISH_MESSAGE_ARGS=%d,%d", rand, data_long);
      return -1;
    }
    currentIndex = currentIndex + 4;
  }

  while (currentIndex < m_driveSize){

    uint8_t readData;
    if (read(m_fd, static_cast<void*>(&readData), 1) == 1){
       sd_journal_send("MESSAGE=%s", "Estoraged erase pattern unable to read 1",
                        "REDFISH_MESSAGE_ID=%s","eStorageD.1.0.EraseFailure",
                          "REDFISH_MESSAGE_ARGS=%d", m_fd);
    }
    uint8_t randData = static_cast<uint8_t>(generator());
    if (randData != readData)
    {
      sd_journal_send("MESSAGE=%s", "Estoraged erase pattern does not match",
                            "REDFISH_MESSAGE_ID=%s","eStorageD.1.0.EraseFailure",
                          "REDFISH_MESSAGE_ARGS=%d,%d", randData, readData);
      return -1;
    }
    currentIndex++;
  }
  return 0;
}

//helper
int pattern::openAndGetSize(){
  int rc = findSizeOfBlockDevice(m_devName, m_driveSize);
  if (rc  <= 0){
    sd_journal_send("MESSAGE=%s", "eStorageD erase pattern unable to find size"
                           "REDFISH_MESSAGE_ID=%s","eStorageD.1.0.EraseFailure",
                          "REDFISH_MESSAGE_ARGS=%d", rc);
    return -1;
  }
  m_fd = open(m_devName.c_str(), O_RDONLY, O_NDELAY);
  if (m_fd < 0){
    sd_journal_send("ESSAGE=%s", "eStorageD erase pattern unable to find size"
                          "REDFISH_MESSAGE_ID=%s","eStorageD.1.0.EraseFailure",
                          "REDFISH_MESSAGE_ARGS=%d", m_fd);
    return -1;
  }
  return 0;
}
