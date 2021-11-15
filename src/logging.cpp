#include <systemd/sd-journal.h>

void log(const char* fmt, ...)
{
    // to direct the cout use printf( ...)
    // to direct to cerr use printf( cerr, ...)
    //
    va_list args;
    va_start(args, fmt);
    sd_journal_send(fmt, args, NULL);
    va_end(args);
}
