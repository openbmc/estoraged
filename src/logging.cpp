#include <systemd/sd-journal.h>

void log(const char* fmt, ...)
{
    // to direct the cout use printf( ...)
    // to direct to cerr use printf( cerr, ...)
    sd_journal_send(fmt);
}
