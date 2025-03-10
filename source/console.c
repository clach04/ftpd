#include "console.h"
#include <arpa/inet.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _3DS
#include <3ds.h>
#define CONSOLE_WIDTH 50
#define CONSOLE_HEIGHT 30
#elif defined(_NDS)
#include <nds.h>
#define CONSOLE_WIDTH 32
#define CONSOLE_HEIGHT 24
#elif defined(__SWITCH__)
#include <switch.h>
#define CONSOLE_WIDTH 80
#define CONSOLE_HEIGHT 45
#endif

#if defined(_3DS) || defined(_NDS) || defined(__SWITCH__)
static PrintConsole status_console;
static PrintConsole main_console;
static PrintConsole warning_console;
#endif
#if ENABLE_LOGGING
static bool disable_logging = false;
#endif


#if defined(_3DS)
/*! initialize console subsystem */
void
console_init(void)
{
  consoleInit(GFX_TOP, &status_console);
  consoleSetWindow(&status_console, 0, 0, 50, 1);

  consoleInit(GFX_TOP, &main_console);
  consoleSetWindow(&main_console, 0, 1, 50, 29);

  consoleSelect(&main_console);
}
#elif defined(_NDS)
/*! initialize console subsystem */
void
console_init(void)
{
  powerOff(PM_BACKLIGHT_BOTTOM);
  powerOff(PM_ARM9_DIRECT | POWER_2D_B);

  videoSetMode(MODE_0_2D);
  vramSetBankA(VRAM_A_MAIN_BG_0x06000000);

  consoleInit(&main_console, 0, BgType_Text4bpp, BgSize_T_256x256, 22, 3, true, true);
  status_console = main_console;
  warning_console = main_console;

  consoleSetWindow(&status_console, 0, 0, 32, 2);
  consoleSetWindow(&main_console, 0, 2, 32, 21);
  consoleSetWindow(&warning_console, 0, 23, 32, 1);

  consoleSelect(&warning_console);
  console_print(RED "This is an unofficial fork!");
  consoleSelect(&main_console);
}
#endif

#if defined(_3DS)
/*! print tcp tables */
static void
print_tcp_table(void)
{
  static SOCU_TCPTableEntry tcp_entries[32];
  socklen_t                 optlen;
  size_t                    i;
  int                       rc, lines = 0;

#ifdef ENABLE_LOGGING
  disable_logging = true;
#endif

  consoleSelect(&tcp_console);
  console_print("\x1b[0;0H\x1b[K");
  optlen = sizeof(tcp_entries);
  rc = SOCU_GetNetworkOpt(SOL_CONFIG, NETOPT_TCP_TABLE, tcp_entries, &optlen);
  if(rc != 0 && errno != ENODEV)
    console_print(RED "tcp table: %d %s\n\x1b[J\n" RESET, errno, strerror(errno));
  else if(rc == 0)
  {
    for(i = 0; lines < CONSOLE_HEIGHT && i < optlen / sizeof(SOCU_TCPTableEntry); ++i)
    {
      SOCU_TCPTableEntry *entry  = &tcp_entries[i];
      struct sockaddr_in *local  = (struct sockaddr_in*)&entry->local;
      struct sockaddr_in *remote = (struct sockaddr_in*)&entry->remote;

      console_print(GREEN "%stcp[%zu]: ", i == 0 ? "" : "\n", i);
      switch(entry->state)
      {
        case TCP_STATE_CLOSED:
          console_print("CLOSED\x1b[K");
          local = remote = NULL;
          break;

        case TCP_STATE_LISTEN:
          console_print("LISTEN\x1b[K");
          remote = NULL;
          break;

        case TCP_STATE_ESTABLISHED:
          console_print("ESTABLISHED\x1b[K");
          break;

        case TCP_STATE_FINWAIT1:
          console_print("FINWAIT1\x1b[K");
          break;

        case TCP_STATE_FINWAIT2:
          console_print("FINWAIT2\x1b[K");
          break;

        case TCP_STATE_CLOSE_WAIT:
          console_print("CLOSE_WAIT\x1b[K");
          break;

        case TCP_STATE_LAST_ACK:
          console_print("LAST_ACK\x1b[K");
          break;

        case TCP_STATE_TIME_WAIT:
          console_print("TIME_WAIT\x1b[K");
          break;

        default:
          console_print("State %lu\x1b[K", entry->state);
          break;
      }

      ++lines;

      if(local && (lines++ < CONSOLE_HEIGHT))
        console_print("\n Local %s:%u\x1b[K", inet_ntoa(local->sin_addr),
                                 ntohs(local->sin_port));

      if(remote && (lines++ < CONSOLE_HEIGHT))
        console_print("\n Peer  %s:%u\x1b[K", inet_ntoa(remote->sin_addr),
                                  ntohs(remote->sin_port));
    }

    console_print(RESET "\x1b[J");
  }
  else
    console_print("\x1b[2J");

  consoleSelect(&main_console);

#ifdef ENABLE_LOGGING
  disable_logging = false;
#endif
}
#endif

#if defined(__SWITCH__)
/*! initialize console subsystem */
void
console_init(void)
{
  consoleInit(&status_console);
  consoleSetWindow(&status_console, 0, 0, CONSOLE_WIDTH, 1);

  consoleInit( &main_console);
  consoleSetWindow(&main_console, 0, 1, CONSOLE_WIDTH, CONSOLE_HEIGHT-1);

  consoleSelect(&main_console);
}
#endif

#if defined(_3DS) || defined(__SWITCH__) || defined(_NDS)


/*! set status bar contents
 *
 *  @param[in] fmt format string
 *  @param[in] ... format arguments
 */
void
console_set_status(const char *fmt, ...)
{
  va_list ap;

  consoleSelect(&status_console);
  va_start(ap, fmt);
  vprintf(fmt, ap);
#ifdef ENABLE_LOGGING
  vfprintf(stderr, fmt, ap);
#endif
  va_end(ap);
  consoleSelect(&main_console);
}

/*! add text to the console
 *
 *  @param[in] fmt format string
 *  @param[in] ... format arguments
 */
void
console_print(const char *fmt, ...)
{
  va_list ap;

  va_start(ap, fmt);
  vprintf(fmt, ap);
#ifdef ENABLE_LOGGING
  if(!disable_logging)
    vfprintf(stderr, fmt, ap);
#endif
  va_end(ap);
}

/*! print debug message
 *
 *  @param[in] fmt format string
 *  @param[in] ... format arguments
 */
void
debug_print(const char *fmt, ...)
{
#ifdef ENABLE_LOGGING
  va_list ap;

  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
#endif
}

/*! draw console to screen */
void
console_render(void)
{
  /* print tcp table */
#ifdef _3DS
  print_tcp_table();
#endif
  /* flush framebuffer */
#ifdef _3DS
  gfxFlushBuffers();
  gspWaitForVBlank();
  gfxSwapBuffers();
#endif
#ifdef __SWITCH__
  consoleUpdate(NULL);
#endif
}


#else

/* this is a lot easier when you have a real console */

void
console_init(void)
{
}

void
console_set_status(const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  vprintf(fmt, ap);
  va_end(ap);
  fputc('\n', stdout);
}

void
console_print(const char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  vprintf(fmt, ap);
  va_end(ap);
}

void
debug_print(const char *fmt, ...)
{
#ifdef ENABLE_LOGGING
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
#endif
}

void console_render(void)
{
}
#endif

