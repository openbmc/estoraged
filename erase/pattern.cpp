

pattern::write()
{
  unsigned long driveSize;
  if (findSizeOfBlockDevice(m_devname, driveSize) == 0){
    sd_journal_send("MESSAGE=%s", "eStorageD erase pattern unable to find size");
    return -1;
  }
  int fd = open(m_devname, O_RDONLY, O_NDELAY);
  if (fd < 0){
    sd_journal_send("ESSAGE=%s", "eStorageD erase pattern unable to find size");
  }
  // static seed defines a fixed prng sequnce so it can be verified later,
  // and validated for entropy
  unsigned long currentIndex = 0;
  unsigned long seed = 0x6a656272;
  std::minstd_rand0 generator(seed);
  while (currentIndex - 4 < driveSize){
    fputc(generator(), fd);
    currentIndex = currentIndex + 4;
  }
  while (currentIndex < driveSize){
    fputc(static_cast<uint8>generator(), fd);
    currentIndex++;
  }

}

pattern::randblock()
