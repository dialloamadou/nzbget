/*
 *  This file is part of nzbget
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007-2015 Andrey Prygunkov <hugbug@users.sourceforge.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * $Revision$
 * $Date$
 *
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef WIN32
#include "win32.h"
#endif

#ifndef DISABLE_CURSES

#ifdef HAVE_NCURSES_H
#include <ncurses.h>
#endif
#ifdef HAVE_NCURSES_NCURSES_H
#include <ncurses/ncurses.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifndef WIN32
#include <unistd.h>
#endif

#include "nzbget.h"
#include "NCursesFrontend.h"
#include "Options.h"
#include "Util.h"

#ifdef HAVE_CURSES_H
// curses.h header must be included last to avoid problems on Solaris
// (and possibly other systems, that uses curses.h (not ncurses.h)
#include <curses.h>
// "#undef erase" is neccessary on Solaris
#undef erase
#endif

#ifndef WIN32
// curses.h on Solaris declares "clear()" via DEFINE. That causes problems, because
// it also affects calls to deque's method "clear()", producing compiler errors.
// We use function "curses_clear()" to call macro "clear" of curses, then
// undefine macro "clear".
void curses_clear()
{
    clear();
}
#undef clear
#endif

extern void ExitProc();

static const int NCURSES_COLORPAIR_TEXT			= 1;
static const int NCURSES_COLORPAIR_INFO			= 2;
static const int NCURSES_COLORPAIR_WARNING		= 3;
static const int NCURSES_COLORPAIR_ERROR		= 4;
static const int NCURSES_COLORPAIR_DEBUG		= 5;
static const int NCURSES_COLORPAIR_DETAIL		= 6;
static const int NCURSES_COLORPAIR_STATUS		= 7;
static const int NCURSES_COLORPAIR_KEYBAR		= 8;
static const int NCURSES_COLORPAIR_INFOLINE		= 9;
static const int NCURSES_COLORPAIR_TEXTHIGHL	= 10;
static const int NCURSES_COLORPAIR_CURSOR		= 11;
static const int NCURSES_COLORPAIR_HINT			= 12;

static const int MAX_SCREEN_WIDTH				= 512;

#ifdef WIN32
static const int COLOR_BLACK	= 0;
static const int COLOR_BLUE		= FOREGROUND_BLUE;
static const int COLOR_RED		= FOREGROUND_RED;
static const int COLOR_GREEN	= FOREGROUND_GREEN;
static const int COLOR_WHITE	= FOREGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN;
static const int COLOR_MAGENTA	= FOREGROUND_RED | FOREGROUND_BLUE;
static const int COLOR_CYAN		= FOREGROUND_BLUE | FOREGROUND_GREEN;
static const int COLOR_YELLOW	= FOREGROUND_RED | FOREGROUND_GREEN;

static const int READKEY_EMPTY = 0;

#define KEY_DOWN VK_DOWN
#define KEY_UP VK_UP
#define KEY_PPAGE VK_PRIOR
#define KEY_NPAGE VK_NEXT
#define KEY_END VK_END
#define KEY_HOME VK_HOME
#define KEY_BACKSPACE VK_BACK

#else

static const int READKEY_EMPTY = ERR;

#endif

NCursesFrontend::NCursesFrontend()
{
    m_screenHeight = 0;
    m_screenWidth = 0;
    m_inputNumberIndex = 0;
    m_inputMode = normal;
    m_summary = true;
    m_fileList = true;
    m_neededLogEntries = 0;
	m_queueWinTop = 0;
	m_queueWinHeight = 0;
	m_queueWinClientHeight = 0;
	m_messagesWinTop = 0;
	m_messagesWinHeight = 0;
	m_messagesWinClientHeight = 0;
    m_selectedQueueEntry = 0;
	m_queueScrollOffset = 0;
	m_showNzbname = g_pOptions->GetCursesNZBName();
	m_showTimestamp = g_pOptions->GetCursesTime();
	m_groupFiles = g_pOptions->GetCursesGroup();
	m_queueWindowPercentage = 50;
	m_dataUpdatePos = 0;
    m_updateNextTime = false;
	m_lastEditEntry = -1;
	m_lastPausePars = false;
	m_hint = NULL;

    // Setup curses
#ifdef WIN32
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	m_screenBuffer = NULL;
	m_oldScreenBuffer = NULL;
	m_colorAttr.clear();

	CONSOLE_CURSOR_INFO ConsoleCursorInfo;
	GetConsoleCursorInfo(hConsole, &ConsoleCursorInfo);
	ConsoleCursorInfo.bVisible = false;
	SetConsoleCursorInfo(hConsole, &ConsoleCursorInfo);
	if (IsRemoteMode())
	{
		SetConsoleTitle("NZBGet - remote mode");
	}
	else
	{
		SetConsoleTitle("NZBGet");
	}

	m_useColor = true;
#else
    m_window = initscr();
    if (m_window == NULL)
    {
        printf("ERROR: m_pWindow == NULL\n");
        exit(-1);
    }
	keypad(stdscr, true);
    nodelay((WINDOW*)m_window, true);
    noecho();
    curs_set(0);
    m_useColor = has_colors();
#endif

    if (m_useColor)
    {
#ifndef WIN32
        start_color();
#endif
        init_pair(0,							COLOR_WHITE,	COLOR_BLUE);
        init_pair(NCURSES_COLORPAIR_TEXT,		COLOR_WHITE,	COLOR_BLACK);
        init_pair(NCURSES_COLORPAIR_INFO,		COLOR_GREEN,	COLOR_BLACK);
        init_pair(NCURSES_COLORPAIR_WARNING,	COLOR_MAGENTA,	COLOR_BLACK);
        init_pair(NCURSES_COLORPAIR_ERROR,		COLOR_RED,		COLOR_BLACK);
        init_pair(NCURSES_COLORPAIR_DEBUG,		COLOR_WHITE,	COLOR_BLACK);
        init_pair(NCURSES_COLORPAIR_DETAIL,		COLOR_GREEN,	COLOR_BLACK);
        init_pair(NCURSES_COLORPAIR_STATUS,		COLOR_BLUE,		COLOR_WHITE);
        init_pair(NCURSES_COLORPAIR_KEYBAR,		COLOR_WHITE,	COLOR_BLUE);
        init_pair(NCURSES_COLORPAIR_INFOLINE,	COLOR_WHITE,	COLOR_BLUE);
        init_pair(NCURSES_COLORPAIR_TEXTHIGHL,	COLOR_BLACK,	COLOR_CYAN);
        init_pair(NCURSES_COLORPAIR_CURSOR,		COLOR_BLACK,	COLOR_YELLOW);
        init_pair(NCURSES_COLORPAIR_HINT,		COLOR_WHITE,	COLOR_RED);
    }
}

NCursesFrontend::~NCursesFrontend()
{
#ifdef WIN32
	free(m_screenBuffer);
	free(m_oldScreenBuffer);

	m_colorAttr.clear();

	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_CURSOR_INFO ConsoleCursorInfo;
	GetConsoleCursorInfo(hConsole, &ConsoleCursorInfo);
	ConsoleCursorInfo.bVisible = true;
	SetConsoleCursorInfo(hConsole, &ConsoleCursorInfo);
#else
    keypad(stdscr, false);
    echo();
    curs_set(1);
    endwin();
#endif
    printf("\n");
	SetHint(NULL);
}

void NCursesFrontend::Run()
{
    debug("Entering NCursesFrontend-loop");

	m_dataUpdatePos = 0;

    while (!IsStopped())
    {
        // The data (queue and log) is updated each m_iUpdateInterval msec,
        // but the window is updated more often for better reaction on user's input

		bool updateNow = false;
		int key = ReadConsoleKey();

		if (key != READKEY_EMPTY)
		{
			// Update now and next if a key is pressed.
			updateNow = true;
			m_updateNextTime = true;
		} 
		else if (m_updateNextTime)
		{
			// Update due to key being pressed during previous call.
			updateNow = true;
			m_updateNextTime = false;
		} 
		else if (m_dataUpdatePos <= 0) 
		{
			updateNow = true;
			m_updateNextTime = false;
		}

		if (updateNow) 
		{
			Update(key);
		}

		if (m_dataUpdatePos <= 0)
		{
			m_dataUpdatePos = m_updateInterval;
		}

		usleep(10 * 1000);
		m_dataUpdatePos -= 10;
    }

    FreeData();
	
    debug("Exiting NCursesFrontend-loop");
}

void NCursesFrontend::NeedUpdateData()
{
	m_dataUpdatePos = 10;
	m_updateNextTime = true;
}

void NCursesFrontend::Update(int key)
{
    // Figure out how big the screen is
	CalcWindowSizes();

    if (m_dataUpdatePos <= 0)
    {
        FreeData();
		m_neededLogEntries = m_messagesWinClientHeight;
        if (!PrepareData())
        {
            return;
        }

		// recalculate frame sizes
		CalcWindowSizes();
    }

	if (m_inputMode == editQueue)
	{
		int queueSize = CalcQueueSize();
		if (queueSize == 0)
		{
			m_selectedQueueEntry = 0;
			m_inputMode = normal;
		}
	}

    //------------------------------------------
    // Print Current NZBInfoList
    //------------------------------------------
	if (m_queueWinHeight > 0)
	{
		PrintQueue();
	}

    //------------------------------------------
    // Print Messages
    //------------------------------------------
	if (m_messagesWinHeight > 0)
	{
		PrintMessages();
	}

    PrintStatus();

	PrintKeyInputBar();

    UpdateInput(key);

	RefreshScreen();
}

void NCursesFrontend::CalcWindowSizes()
{
    int nrRows, nrColumns;
#ifdef WIN32
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO BufInfo;
	GetConsoleScreenBufferInfo(hConsole, &BufInfo);
	nrRows = BufInfo.srWindow.Bottom - BufInfo.srWindow.Top + 1;
	nrColumns = BufInfo.srWindow.Right - BufInfo.srWindow.Left + 1;
#else
    getmaxyx(stdscr, nrRows, nrColumns);
#endif
    if (nrRows != m_screenHeight || nrColumns != m_screenWidth)
    {
#ifdef WIN32
		m_screenBufferSize = nrRows * nrColumns * sizeof(CHAR_INFO);
		m_screenBuffer = (CHAR_INFO*)realloc(m_screenBuffer, m_screenBufferSize);
		memset(m_screenBuffer, 0, m_screenBufferSize);
		m_oldScreenBuffer = (CHAR_INFO*)realloc(m_oldScreenBuffer, m_screenBufferSize);
		memset(m_oldScreenBuffer, 0, m_screenBufferSize);
#else
        curses_clear();
#endif
        m_screenHeight = nrRows;
        m_screenWidth = nrColumns;
    }

    int queueSize = CalcQueueSize();
    
	m_queueWinTop = 0;
	m_queueWinHeight = (m_screenHeight - 2) * m_queueWindowPercentage / 100;
	if (m_queueWinHeight - 1 > queueSize)
	{
		m_queueWinHeight = queueSize > 0 ? queueSize + 1 : 1 + 1;
	}
	m_queueWinClientHeight = m_queueWinHeight - 1;
	if (m_queueWinClientHeight < 0)
	{
		m_queueWinClientHeight = 0;
	}

	m_messagesWinTop = m_queueWinTop + m_queueWinHeight;
	m_messagesWinHeight = m_screenHeight - m_queueWinHeight - 2;
	m_messagesWinClientHeight = m_messagesWinHeight - 1;
	if (m_messagesWinClientHeight < 0)
	{
		m_messagesWinClientHeight = 0;
	}
}

int NCursesFrontend::CalcQueueSize()
{
	int queueSize = 0;
	DownloadQueue* downloadQueue = LockQueue();
	if (m_groupFiles)
	{
		queueSize = downloadQueue->GetQueue()->size();
	}
	else
	{
		for (NZBList::iterator it = downloadQueue->GetQueue()->begin(); it != downloadQueue->GetQueue()->end(); it++)
		{
			NZBInfo* nzbInfo = *it;
			queueSize += nzbInfo->GetFileList()->size();
		}
	}
	UnlockQueue();
	return queueSize;
}

void NCursesFrontend::PlotLine(const char * string, int row, int pos, int colorPair)
{
    char buffer[MAX_SCREEN_WIDTH];
    snprintf(buffer, sizeof(buffer), "%-*s", m_screenWidth, string);
	buffer[MAX_SCREEN_WIDTH - 1] = '\0';
	int len = strlen(buffer);
	if (len > m_screenWidth - pos && m_screenWidth - pos < MAX_SCREEN_WIDTH)
	{
		buffer[m_screenWidth - pos] = '\0';
	}

	PlotText(buffer, row, pos, colorPair, false);
}

void NCursesFrontend::PlotText(const char * string, int row, int pos, int colorPair, bool blink)
{
#ifdef WIN32
	int bufPos = row * m_screenWidth + pos;
	int len = strlen(string);
	for (int i = 0; i < len; i++)
	{
		char c = string[i];
		CharToOemBuff(&c, &c, 1);
		m_screenBuffer[bufPos + i].Char.AsciiChar = c;
		m_screenBuffer[bufPos + i].Attributes = m_colorAttr[colorPair];
	}
#else
	if( m_useColor ) 
	{
		attron(COLOR_PAIR(colorPair));
		if (blink)
		{
			attron(A_BLINK);
		}
	}
    mvaddstr(row, pos, (char*)string);
	if( m_useColor ) 
	{
		attroff(COLOR_PAIR(colorPair));
		if (blink)
		{
			attroff(A_BLINK);
		}
	}
#endif
}

void NCursesFrontend::RefreshScreen()
{
#ifdef WIN32
	bool bufChanged = memcmp(m_screenBuffer, m_oldScreenBuffer, m_screenBufferSize);
	if (bufChanged)
	{
		COORD BufSize;
		BufSize.X = m_screenWidth;
		BufSize.Y = m_screenHeight;

		COORD BufCoord;
		BufCoord.X = 0;
		BufCoord.Y = 0;

		HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
		CONSOLE_SCREEN_BUFFER_INFO BufInfo;
		GetConsoleScreenBufferInfo(hConsole, &BufInfo);
		WriteConsoleOutput(hConsole, m_screenBuffer, BufSize, BufCoord, &BufInfo.srWindow);

		BufInfo.dwCursorPosition.X = BufInfo.srWindow.Right;
		BufInfo.dwCursorPosition.Y = BufInfo.srWindow.Bottom;
		SetConsoleCursorPosition(hConsole, BufInfo.dwCursorPosition);

		memcpy(m_oldScreenBuffer, m_screenBuffer, m_screenBufferSize);
	}
#else
    // Cursor placement
    wmove((WINDOW*)m_window, m_screenHeight, m_screenWidth);

    // NCurses refresh
    refresh();
#endif
}

#ifdef WIN32
void NCursesFrontend::init_pair(int colorNumber, WORD wForeColor, WORD wBackColor)
{
	m_colorAttr.resize(colorNumber + 1);
	m_colorAttr[colorNumber] = wForeColor | (wBackColor << 4);
}
#endif

void NCursesFrontend::PrintMessages()
{
	int lineNr = m_messagesWinTop;

	char buffer[MAX_SCREEN_WIDTH];
	snprintf(buffer, sizeof(buffer), "%s Messages", m_useColor ? "" : "*** ");
	buffer[MAX_SCREEN_WIDTH - 1] = '\0';
    PlotLine(buffer, lineNr++, 0, NCURSES_COLORPAIR_INFOLINE);
	
    int line = lineNr + m_messagesWinClientHeight - 1;
	int linesToPrint = m_messagesWinClientHeight;

    MessageList* messages = LockMessages();
    
	// print messages from bottom
	for (int i = (int)messages->size() - 1; i >= 0 && linesToPrint > 0; i--)
    {
        int printedLines = PrintMessage((*messages)[i], line, linesToPrint);
		line -= printedLines;
		linesToPrint -= printedLines;
    }

	if (linesToPrint > 0)
	{
		// too few messages, print them again from top
    	line = lineNr + m_messagesWinClientHeight - 1;
		while (linesToPrint-- > 0)
		{
			PlotLine("", line--, 0, NCURSES_COLORPAIR_TEXT);
		}
		int linesToPrint2 = m_messagesWinClientHeight;
		for (int i = (int)messages->size() - 1; i >= 0 && linesToPrint2 > 0; i--)
		{
			int printedLines = PrintMessage((*messages)[i], line, linesToPrint2);
			line -= printedLines;
			linesToPrint2 -= printedLines;
		}
	}
	
    UnlockMessages();
}

int NCursesFrontend::PrintMessage(Message* Msg, int row, int maxLines)
{
    const char* messageType[] = { "INFO    ", "WARNING ", "ERROR   ", "DEBUG   ", "DETAIL  "};
    const int messageTypeColor[] = { NCURSES_COLORPAIR_INFO, NCURSES_COLORPAIR_WARNING,
    	NCURSES_COLORPAIR_ERROR, NCURSES_COLORPAIR_DEBUG, NCURSES_COLORPAIR_DETAIL };

	char* text = (char*)Msg->GetText();

	if (m_showTimestamp)
	{
		int len = strlen(text) + 50;
		text = (char*)malloc(len);

		time_t rawtime = Msg->GetTime();
		rawtime += g_pOptions->GetTimeCorrection();

		char time[50];
#ifdef HAVE_CTIME_R_3
		ctime_r(&rawtime, time, 50);
#else
		ctime_r(&rawtime, time);
#endif
		time[50-1] = '\0';
		time[strlen(time) - 1] = '\0'; // trim LF

		snprintf(text, len, "%s - %s", time, Msg->GetText());
		text[len - 1] = '\0';
	}
	else
	{
		text = strdup(text);
	}

	// replace some special characters with spaces
	for (char* p = text; *p; p++)
	{
		if (*p == '\n' || *p == '\r' || *p == '\b')
		{
			*p = ' ';
		}
	}

	int len = strlen(text);
	int winWidth = m_screenWidth - 8;
	int msgLines = len / winWidth;
	if (len % winWidth > 0)
	{
		msgLines++;
	}
	
	int lines = 0;
	for (int i = msgLines - 1; i >= 0 && lines < maxLines; i--)
	{
		int r = row - msgLines + i + 1;
		PlotLine(text + winWidth * i, r, 8, NCURSES_COLORPAIR_TEXT);
		if (i == 0)
		{
			PlotText(messageType[Msg->GetKind()], r, 0, messageTypeColor[Msg->GetKind()], false);
		}
		else
		{
			PlotText("        ", r, 0, messageTypeColor[Msg->GetKind()], false);
		}
		lines++;
	}

	free(text);

	return lines;
}

void NCursesFrontend::PrintStatus()
{
    char tmp[MAX_SCREEN_WIDTH];
    int statusRow = m_screenHeight - 2;

    char timeString[100];
    timeString[0] = '\0';

	int currentDownloadSpeed = m_standBy ? 0 : m_currentDownloadSpeed;
    if (currentDownloadSpeed > 0 && !m_pauseDownload)
    {
        long long remain_sec = (long long)(m_remainingSize / currentDownloadSpeed);
		int h = (int)(remain_sec / 3600);
		int m = (int)((remain_sec % 3600) / 60);
		int s = (int)(remain_sec % 60);
		sprintf(timeString, " (~ %.2d:%.2d:%.2d)", h, m, s);
    }

    char downloadLimit[128];
    if (m_downloadLimit > 0)
    {
        sprintf(downloadLimit, ", Limit %i KB/s", m_downloadLimit / 1024);
    }
    else
    {
        downloadLimit[0] = 0;
    }

    char postStatus[128];
    if (m_postJobCount > 0)
    {
        sprintf(postStatus, ", %i post-job%s", m_postJobCount, m_postJobCount > 1 ? "s" : "");
    }
    else
    {
        postStatus[0] = 0;
    }

	char currentSpeedBuf[20];
	char averageSpeedBuf[20];
	char remainingSizeBuf[20];
	int averageSpeed = (int)(m_dnTimeSec > 0 ? m_allBytes / m_dnTimeSec : 0);

	snprintf(tmp, MAX_SCREEN_WIDTH, " %d threads, %s, %s remaining%s%s%s%s, Avg. %s",
		m_threadCount, Util::FormatSpeed(currentSpeedBuf, sizeof(currentSpeedBuf), currentDownloadSpeed),
		Util::FormatSize(remainingSizeBuf, sizeof(remainingSizeBuf), m_remainingSize),
		timeString, postStatus, m_pauseDownload ? (m_standBy ? ", Paused" : ", Pausing") : "",
		downloadLimit, Util::FormatSpeed(averageSpeedBuf, sizeof(averageSpeedBuf), averageSpeed));
	tmp[MAX_SCREEN_WIDTH - 1] = '\0';
    PlotLine(tmp, statusRow, 0, NCURSES_COLORPAIR_STATUS);
}

void NCursesFrontend::PrintKeyInputBar()
{
    int queueSize = CalcQueueSize();
	int inputBarRow = m_screenHeight - 1;
	
	if (m_hint)
	{
		time_t time = ::time(NULL);
		if (time - m_startHint < 5)
		{
			PlotLine(m_hint, inputBarRow, 0, NCURSES_COLORPAIR_HINT);
			return;
		}
		else
		{
			SetHint(NULL);
		}
	}

    switch (m_inputMode)
    {
    case normal:
		if (m_groupFiles)
		{
	        PlotLine("(Q)uit | (E)dit | (P)ause | (R)ate | (W)indow | (G)roup | (T)ime", inputBarRow, 0, NCURSES_COLORPAIR_KEYBAR);
		}
		else
		{
	        PlotLine("(Q)uit | (E)dit | (P)ause | (R)ate | (W)indow | (G)roup | (T)ime | n(Z)b", inputBarRow, 0, NCURSES_COLORPAIR_KEYBAR);
		}
        break;
    case editQueue:
    {
		const char* status = NULL;
		if (m_selectedQueueEntry > 0 && queueSize > 1 && m_selectedQueueEntry == queueSize - 1)
		{
			status = "(Q)uit | (E)xit | (P)ause | (D)elete | (U)p/(T)op";
		}
		else if (queueSize > 1 && m_selectedQueueEntry == 0)
		{
			status = "(Q)uit | (E)xit | (P)ause | (D)elete | dow(N)/(B)ottom";
		}
		else if (queueSize > 1)
		{
			status = "(Q)uit | (E)xit | (P)ause | (D)elete | (U)p/dow(N)/(T)op/(B)ottom";
		}
		else
		{
			status = "(Q)uit | (E)xit | (P)ause | (D)elete";
		}

		PlotLine(status, inputBarRow, 0, NCURSES_COLORPAIR_KEYBAR);
        break;
    }
    case downloadRate:
        char string[128];
        snprintf(string, 128, "Download rate: %i", m_inputValue);
		string[128-1] = '\0';
        PlotLine(string, inputBarRow, 0, NCURSES_COLORPAIR_KEYBAR);
        // Print the cursor
		PlotText(" ", inputBarRow, 15 + m_inputNumberIndex, NCURSES_COLORPAIR_CURSOR, true);
        break;
    }
}

void NCursesFrontend::SetHint(const char* hint)
{
	free(m_hint);
	m_hint = NULL;
	if (hint)
	{
		m_hint = strdup(hint);
		m_startHint = time(NULL);
	}
}

void NCursesFrontend::PrintQueue()
{
	if (m_groupFiles)
	{
		PrintGroupQueue();
	}
	else
	{
		PrintFileQueue();
	}
}

void NCursesFrontend::PrintFileQueue()
{
    DownloadQueue* downloadQueue = LockQueue();

	int lineNr = m_queueWinTop + 1;
	long long remaining = 0;
	long long paused = 0;
	int pausedFiles = 0;
	int fileNum = 0;

	for (NZBList::iterator it = downloadQueue->GetQueue()->begin(); it != downloadQueue->GetQueue()->end(); it++)
	{
		NZBInfo* nzbInfo = *it;
		for (FileList::iterator it2 = nzbInfo->GetFileList()->begin(); it2 != nzbInfo->GetFileList()->end(); it2++, fileNum++)
		{
			FileInfo* fileInfo = *it2;

			if (fileNum >= m_queueScrollOffset && fileNum < m_queueScrollOffset + m_queueWinHeight -1)
			{
				PrintFilename(fileInfo, lineNr++, fileNum == m_selectedQueueEntry);
			}

			if (fileInfo->GetPaused())
			{
				pausedFiles++;
				paused += fileInfo->GetRemainingSize();
			}
			remaining += fileInfo->GetRemainingSize();
		}
	}

	if (fileNum > 0)
	{
		char remainingBuf[20];
		char unpausedBuf[20];
		char buffer[MAX_SCREEN_WIDTH];
		snprintf(buffer, sizeof(buffer), " %sFiles for downloading - %i / %i files in queue - %s / %s", 
			m_useColor ? "" : "*** ", fileNum,
			fileNum - pausedFiles,
			Util::FormatSize(remainingBuf, sizeof(remainingBuf), remaining),
			Util::FormatSize(unpausedBuf, sizeof(unpausedBuf), remaining - paused));
		buffer[MAX_SCREEN_WIDTH - 1] = '\0';
		PrintTopHeader(buffer, m_queueWinTop, true);
    }
	else
	{
		lineNr--;
		char buffer[MAX_SCREEN_WIDTH];
		snprintf(buffer, sizeof(buffer), "%s Files for downloading", m_useColor ? "" : "*** ");
		buffer[MAX_SCREEN_WIDTH - 1] = '\0';
		PrintTopHeader(buffer, lineNr++, true);
        PlotLine("Ready to receive nzb-job", lineNr++, 0, NCURSES_COLORPAIR_TEXT);
	}

    UnlockQueue();
}

void NCursesFrontend::PrintFilename(FileInfo * fileInfo, int row, bool selected)
{
	int color = 0;
	const char* Brace1 = "[";
	const char* Brace2 = "]";
	if (m_inputMode == editQueue && selected)
	{
		color = NCURSES_COLORPAIR_TEXTHIGHL;
		if (!m_useColor)
		{
			Brace1 = "<";
			Brace2 = ">";
		}
	}
	else
	{
		color = NCURSES_COLORPAIR_TEXT;
	}

	const char* downloading = "";
	if (fileInfo->GetActiveDownloads() > 0)
	{
		downloading = " *";
	}

	char priority[100];
	priority[0] = '\0';
	if (fileInfo->GetNZBInfo()->GetPriority() != 0)
	{
		sprintf(priority, " [%+i]", fileInfo->GetNZBInfo()->GetPriority());
	}

	char completed[20];
	completed[0] = '\0';
	if (fileInfo->GetRemainingSize() < fileInfo->GetSize())
	{
		sprintf(completed, ", %i%%", (int)(100 - fileInfo->GetRemainingSize() * 100 / fileInfo->GetSize()));
	}

	char nzbNiceName[1024];
	if (m_showNzbname)
	{
		strncpy(nzbNiceName, fileInfo->GetNZBInfo()->GetName(), 1023);
		int len = strlen(nzbNiceName);
		nzbNiceName[len] = PATH_SEPARATOR;
		nzbNiceName[len + 1] = '\0';
	}
	else
	{
		nzbNiceName[0] = '\0';
	}

	char size[20];
	char buffer[MAX_SCREEN_WIDTH];
	snprintf(buffer, MAX_SCREEN_WIDTH, "%s%i%s%s%s %s%s (%s%s)%s", Brace1, fileInfo->GetID(),
		Brace2, priority, downloading, nzbNiceName, fileInfo->GetFilename(),
		Util::FormatSize(size, sizeof(size), fileInfo->GetSize()),
		completed, fileInfo->GetPaused() ? " (paused)" : "");
	buffer[MAX_SCREEN_WIDTH - 1] = '\0';

	PlotLine(buffer, row, 0, color);
}

void NCursesFrontend::PrintTopHeader(char* header, int lineNr, bool upTime)
{
    char buffer[MAX_SCREEN_WIDTH];
    snprintf(buffer, sizeof(buffer), "%-*s", m_screenWidth, header);
	buffer[MAX_SCREEN_WIDTH - 1] = '\0';
	int headerLen = strlen(header);
	int charsLeft = m_screenWidth - headerLen - 2;

	int time = upTime ? m_upTimeSec : m_dnTimeSec;
	int d = time / 3600 / 24;
	int h = (time % (3600 * 24)) / 3600;
	int m = (time % 3600) / 60;
	int s = time % 60;
	char timeStr[30];

	if (d == 0)
	{
		snprintf(timeStr, 30, "%.2d:%.2d:%.2d", h, m, s);
		if ((int)strlen(timeStr) > charsLeft)
		{
			snprintf(timeStr, 30, "%.2d:%.2d", h, m);
		}
	}
	else 
	{
		snprintf(timeStr, 30, "%i %s %.2d:%.2d:%.2d", d, (d == 1 ? "day" : "days"), h, m, s);
		if ((int)strlen(timeStr) > charsLeft)
		{
			snprintf(timeStr, 30, "%id %.2d:%.2d:%.2d", d, h, m, s);
		}
		if ((int)strlen(timeStr) > charsLeft)
		{
			snprintf(timeStr, 30, "%id %.2d:%.2d", d, h, m);
		}
	}

	timeStr[29] = '\0';
	const char* shortCap = upTime ? " Up " : "Dn ";
	const char* longCap = upTime ? " Uptime " : " Download-time ";

	int timeLen = strlen(timeStr);
	int shortCapLen = strlen(shortCap);
	int longCapLen = strlen(longCap);

	if (charsLeft - timeLen - longCapLen >= 0)
	{
		snprintf(buffer + m_screenWidth - timeLen - longCapLen, MAX_SCREEN_WIDTH - (m_screenWidth - timeLen - longCapLen), "%s%s", longCap, timeStr);
	}
	else if (charsLeft - timeLen - shortCapLen >= 0)
	{
		snprintf(buffer + m_screenWidth - timeLen - shortCapLen, MAX_SCREEN_WIDTH - (m_screenWidth - timeLen - shortCapLen), "%s%s", shortCap, timeStr);
	}
	else if (charsLeft - timeLen >= 0)
	{
		snprintf(buffer + m_screenWidth - timeLen, MAX_SCREEN_WIDTH - (m_screenWidth - timeLen), "%s", timeStr);
	}

    PlotLine(buffer, lineNr, 0, NCURSES_COLORPAIR_INFOLINE);
}

void NCursesFrontend::PrintGroupQueue()
{
	int lineNr = m_queueWinTop;

    DownloadQueue* downloadQueue = LockQueue();
	if (downloadQueue->GetQueue()->empty())
    {
		char buffer[MAX_SCREEN_WIDTH];
		snprintf(buffer, sizeof(buffer), "%s NZBs for downloading", m_useColor ? "" : "*** ");
		buffer[MAX_SCREEN_WIDTH - 1] = '\0';
		PrintTopHeader(buffer, lineNr++, false);
        PlotLine("Ready to receive nzb-job", lineNr++, 0, NCURSES_COLORPAIR_TEXT);
    }
    else
    {
		lineNr++;

		ResetColWidths();
		int calcLineNr = lineNr;
		int i = 0;
        for (NZBList::iterator it = downloadQueue->GetQueue()->begin(); it != downloadQueue->GetQueue()->end(); it++, i++)
        {
            NZBInfo* nzbInfo = *it;
			if (i >= m_queueScrollOffset && i < m_queueScrollOffset + m_queueWinHeight -1)
			{
				PrintGroupname(nzbInfo, calcLineNr++, false, true);
			}
        }

		long long remaining = 0;
		long long paused = 0;
		i = 0;
        for (NZBList::iterator it = downloadQueue->GetQueue()->begin(); it != downloadQueue->GetQueue()->end(); it++, i++)
        {
            NZBInfo* nzbInfo = *it;
			if (i >= m_queueScrollOffset && i < m_queueScrollOffset + m_queueWinHeight -1)
			{
				PrintGroupname(nzbInfo, lineNr++, i == m_selectedQueueEntry, false);
			}
			remaining += nzbInfo->GetRemainingSize();
			paused += nzbInfo->GetPausedSize();
        }
		
		char remainingBuf[20];
		Util::FormatSize(remainingBuf, sizeof(remainingBuf), remaining);

		char unpausedBuf[20];
		Util::FormatSize(unpausedBuf, sizeof(unpausedBuf), remaining - paused);
		
		char buffer[MAX_SCREEN_WIDTH];
		snprintf(buffer, sizeof(buffer), " %sNZBs for downloading - %i NZBs in queue - %s / %s", 
			m_useColor ? "" : "*** ", (int)downloadQueue->GetQueue()->size(), remainingBuf, unpausedBuf);
		buffer[MAX_SCREEN_WIDTH - 1] = '\0';
		PrintTopHeader(buffer, m_queueWinTop, false);
    }
    UnlockQueue();
}

void NCursesFrontend::ResetColWidths()
{
	m_colWidthFiles = 0;
	m_colWidthTotal = 0;
	m_colWidthLeft = 0;
}

void NCursesFrontend::PrintGroupname(NZBInfo* nzbInfo, int row, bool selected, bool calcColWidth)
{
	int color = NCURSES_COLORPAIR_TEXT;
	char chBrace1 = '[';
	char chBrace2 = ']';
	if (m_inputMode == editQueue && selected)
	{
		color = NCURSES_COLORPAIR_TEXTHIGHL;
		if (!m_useColor)
		{
			chBrace1 = '<';
			chBrace2 = '>';
		}
	}

	const char* downloading = "";
	if (nzbInfo->GetActiveDownloads() > 0)
	{
		downloading = " *";
	}

	long long unpausedRemainingSize = nzbInfo->GetRemainingSize() - nzbInfo->GetPausedSize();

	char remaining[20];
	Util::FormatSize(remaining, sizeof(remaining), unpausedRemainingSize);

	char priority[100];
	priority[0] = '\0';
	if (nzbInfo->GetPriority() != 0)
	{
		sprintf(priority, " [%+i]", nzbInfo->GetPriority());
	}

	char buffer[MAX_SCREEN_WIDTH];

	// Format:
	// [id - id] Name   Left-Files/Paused     Total      Left     Time
	// [1-2] Nzb-name             999/999 999.99 MB 999.99 MB 00:00:00

	int nameLen = 0;
	if (calcColWidth)
	{
		nameLen = m_screenWidth - 1 - 9 - 11 - 11 - 9;
	}
	else
	{
		nameLen = m_screenWidth - 1 - m_colWidthFiles - 2 - m_colWidthTotal - 2 - m_colWidthLeft - 2 - 9;
	}

	bool printFormatted = nameLen > 20;

	if (printFormatted)
	{
		char files[20];
		snprintf(files, 20, "%i/%i", (int)nzbInfo->GetFileList()->size(), nzbInfo->GetPausedFileCount());
		files[20-1] = '\0';
		
		char total[20];
		Util::FormatSize(total, sizeof(total), nzbInfo->GetSize());

		char nameWithIds[1024];
		snprintf(nameWithIds, 1024, "%c%i%c%s%s %s", chBrace1, nzbInfo->GetID(), chBrace2, 
			priority, downloading, nzbInfo->GetName());
		nameWithIds[nameLen] = '\0';

		char time[100];
		time[0] = '\0';
		int currentDownloadSpeed = m_standBy ? 0 : m_currentDownloadSpeed;
		if (nzbInfo->GetPausedSize() > 0 && unpausedRemainingSize == 0)
		{
			snprintf(time, 100, "[paused]");
			Util::FormatSize(remaining, sizeof(remaining), nzbInfo->GetRemainingSize());
		}
		else if (currentDownloadSpeed > 0 && !m_pauseDownload)
		{
			long long remain_sec = (long long)(unpausedRemainingSize / currentDownloadSpeed);
			int h = (int)(remain_sec / 3600);
			int m = (int)((remain_sec % 3600) / 60);
			int s = (int)(remain_sec % 60);
			if (h < 100)
			{
				snprintf(time, 100, "%.2d:%.2d:%.2d", h, m, s);
			}
			else
			{
				snprintf(time, 100, "99:99:99");
			}
		}

		if (calcColWidth)
		{
			int colWidthFiles = strlen(files);
			m_colWidthFiles = colWidthFiles > m_colWidthFiles ? colWidthFiles : m_colWidthFiles;

			int colWidthTotal = strlen(total);
			m_colWidthTotal = colWidthTotal > m_colWidthTotal ? colWidthTotal : m_colWidthTotal;

			int colWidthLeft = strlen(remaining);
			m_colWidthLeft = colWidthLeft > m_colWidthLeft ? colWidthLeft : m_colWidthLeft;
		}
		else
		{
			snprintf(buffer, MAX_SCREEN_WIDTH, "%-*s  %*s  %*s  %*s  %8s", nameLen, nameWithIds, m_colWidthFiles, files, m_colWidthTotal, total, m_colWidthLeft, remaining, time);
		}
	}
	else
	{
		snprintf(buffer, MAX_SCREEN_WIDTH, "%c%i%c%s %s", chBrace1, nzbInfo->GetID(), 
			chBrace2, downloading, nzbInfo->GetName());
	}

	buffer[MAX_SCREEN_WIDTH - 1] = '\0';

	if (!calcColWidth)
	{
		PlotLine(buffer, row, 0, color);
	}
}

bool NCursesFrontend::EditQueue(DownloadQueue::EEditAction action, int offset)
{
	int ID = 0;

	if (m_groupFiles)
	{
		DownloadQueue* downloadQueue = LockQueue();
		if (m_selectedQueueEntry >= 0 && m_selectedQueueEntry < (int)downloadQueue->GetQueue()->size())
		{
			NZBInfo* nzbInfo = downloadQueue->GetQueue()->at(m_selectedQueueEntry);
			ID = nzbInfo->GetID();
			if (action == DownloadQueue::eaFilePause)
			{
				if (nzbInfo->GetRemainingSize() == nzbInfo->GetPausedSize())
				{
					action = DownloadQueue::eaFileResume;
				}
				else if (nzbInfo->GetPausedSize() == 0 && (nzbInfo->GetRemainingParCount() > 0) &&
					!(m_lastPausePars && m_lastEditEntry == m_selectedQueueEntry))
				{
					action = DownloadQueue::eaFilePauseExtraPars;
					m_lastPausePars = true;
				}
				else
				{
					action = DownloadQueue::eaFilePause;
					m_lastPausePars = false;
				}
			}
		}
		UnlockQueue();

		// map file-edit-actions to group-edit-actions
		 DownloadQueue::EEditAction FileToGroupMap[] = {
			(DownloadQueue::EEditAction)0,
			DownloadQueue::eaGroupMoveOffset, 
			DownloadQueue::eaGroupMoveTop, 
			DownloadQueue::eaGroupMoveBottom, 
			DownloadQueue::eaGroupPause, 
			DownloadQueue::eaGroupResume, 
			DownloadQueue::eaGroupDelete,
			DownloadQueue::eaGroupPauseAllPars,
			DownloadQueue::eaGroupPauseExtraPars };
		action = FileToGroupMap[action];
	}
	else
	{
		DownloadQueue* downloadQueue = LockQueue();

		int fileNum = 0;
		for (NZBList::iterator it = downloadQueue->GetQueue()->begin(); it != downloadQueue->GetQueue()->end(); it++)
		{
			NZBInfo* nzbInfo = *it;
			for (FileList::iterator it2 = nzbInfo->GetFileList()->begin(); it2 != nzbInfo->GetFileList()->end(); it2++, fileNum++)
			{
				if (m_selectedQueueEntry == fileNum)
				{
					FileInfo* fileInfo = *it2;
					ID = fileInfo->GetID();
					if (action == DownloadQueue::eaFilePause)
					{
						action = !fileInfo->GetPaused() ? DownloadQueue::eaFilePause : DownloadQueue::eaFileResume;
					}
				}
			}
		}

		UnlockQueue();
	}

	m_lastEditEntry = m_selectedQueueEntry;

	NeedUpdateData();

	if (ID != 0)
	{
		return ServerEditQueue(action, offset, ID);
	}
	else
	{
		return false;
	}
}

void NCursesFrontend::SetCurrentQueueEntry(int entry)
{
    int queueSize = CalcQueueSize();

	if (entry < 0)
	{
		entry = 0;
	}
	else if (entry > queueSize - 1)
	{
		entry = queueSize - 1;
	}

	if (entry > m_queueScrollOffset + m_queueWinClientHeight ||
	   entry < m_queueScrollOffset - m_queueWinClientHeight)
	{
		m_queueScrollOffset = entry - m_queueWinClientHeight / 2;
	}
	else if (entry < m_queueScrollOffset)
	{
		m_queueScrollOffset -= m_queueWinClientHeight;
	}
	else if (entry >= m_queueScrollOffset + m_queueWinClientHeight)
	{
		m_queueScrollOffset += m_queueWinClientHeight;
	}
	
	if (m_queueScrollOffset > queueSize - m_queueWinClientHeight)
	{
		m_queueScrollOffset = queueSize - m_queueWinClientHeight;
	}
	if (m_queueScrollOffset < 0)
	{
		m_queueScrollOffset = 0;
	}
	
	m_selectedQueueEntry = entry;
}


/*
 * Process keystrokes starting with the initialKey, which must not be
 * READKEY_EMPTY but has alread been set via ReadConsoleKey.
 */
void NCursesFrontend::UpdateInput(int initialKey)
{
	int key = initialKey;
	while (key != READKEY_EMPTY)
    {
		int queueSize = CalcQueueSize();

		// Normal or edit queue mode
		if (m_inputMode == normal || m_inputMode == editQueue)
		{
			switch (key)
			{
			case 'q':
				// Key 'q' for quit
				ExitProc();
				break;
			case 'z':
				// show/hide NZBFilename
				m_showNzbname = !m_showNzbname;
				break;
			case 'w':
				// swicth window sizes
				if (m_queueWindowPercentage == 50)
				{
					m_queueWindowPercentage = 100;
				}
				else if (m_queueWindowPercentage == 100 && m_inputMode != editQueue)
				{
					m_queueWindowPercentage = 0;
				}
				else 
				{
					m_queueWindowPercentage = 50;
				}
				CalcWindowSizes();
				SetCurrentQueueEntry(m_selectedQueueEntry);
				break;
			case 'g':
				// group/ungroup files
				m_groupFiles = !m_groupFiles;
				SetCurrentQueueEntry(m_selectedQueueEntry);
				NeedUpdateData();
				break;
			}
		}
		
		// Normal mode
		if (m_inputMode == normal)
		{
			switch (key)
			{
			case 'p':
				// Key 'p' for pause
				if (!IsRemoteMode())
				{
					info(m_pauseDownload ? "Unpausing download" : "Pausing download");
				}
				ServerPauseUnpause(!m_pauseDownload);
				break;
			case 'e':
			case 10: // return
			case 13: // enter
				if (queueSize > 0)
				{
					m_inputMode = editQueue;
					if (m_queueWindowPercentage == 0)
					{
						m_queueWindowPercentage = 50;
					}
					return;
				}
				break;
			case 'r':
				// Download rate
				m_inputMode = downloadRate;
				m_inputNumberIndex = 0;
				m_inputValue = 0;
				return;
			case 't':
				// show/hide Timestamps
				m_showTimestamp = !m_showTimestamp;
				break;
			}
		}

		// Edit Queue mode
		if (m_inputMode == editQueue)
		{
			switch (key)
			{
			case 'e':
			case 10: // return
			case 13: // enter
				m_inputMode = normal;
				return;
			case KEY_DOWN:
				if (m_selectedQueueEntry < queueSize - 1)
				{
					SetCurrentQueueEntry(m_selectedQueueEntry + 1);
				}
				break;
			case KEY_UP:
				if (m_selectedQueueEntry > 0)
				{
					SetCurrentQueueEntry(m_selectedQueueEntry - 1);
				}
				break;
			case KEY_PPAGE:
				if (m_selectedQueueEntry > 0)
				{
					if (m_selectedQueueEntry == m_queueScrollOffset)
					{
						m_queueScrollOffset -= m_queueWinClientHeight;
						SetCurrentQueueEntry(m_selectedQueueEntry - m_queueWinClientHeight);
					}
					else
					{
						SetCurrentQueueEntry(m_queueScrollOffset);
					}
				}
				break;
			case KEY_NPAGE:
				if (m_selectedQueueEntry < queueSize - 1)
				{
					if (m_selectedQueueEntry == m_queueScrollOffset + m_queueWinClientHeight - 1)
					{
						m_queueScrollOffset += m_queueWinClientHeight;
						SetCurrentQueueEntry(m_selectedQueueEntry + m_queueWinClientHeight);
					}
					else
					{
						SetCurrentQueueEntry(m_queueScrollOffset + m_queueWinClientHeight - 1);
					}
				}
				break;
			case KEY_HOME:
				SetCurrentQueueEntry(0);
				break;
			case KEY_END:
				SetCurrentQueueEntry(queueSize > 0 ? queueSize - 1 : 0);
				break;
			case 'p':
				// Key 'p' for pause
				EditQueue(DownloadQueue::eaFilePause, 0);
				break;
			case 'd':
				SetHint(" Use Uppercase \"D\" for delete");
				break;
			case 'D':
				// Delete entry
				if (EditQueue(DownloadQueue::eaFileDelete, 0))
				{
					SetCurrentQueueEntry(m_selectedQueueEntry);
				}
				break;
			case 'u':
				if (EditQueue(DownloadQueue::eaFileMoveOffset, -1))
				{
					SetCurrentQueueEntry(m_selectedQueueEntry - 1);
				}
				break;
			case 'n':
				if (EditQueue(DownloadQueue::eaFileMoveOffset, +1))
				{
					SetCurrentQueueEntry(m_selectedQueueEntry + 1);
				}
				break;
			case 't':
				if (EditQueue(DownloadQueue::eaFileMoveTop, 0))
				{
					SetCurrentQueueEntry(0);
				}
				break;
			case 'b':
				if (EditQueue(DownloadQueue::eaFileMoveBottom, 0))
				{
					SetCurrentQueueEntry(queueSize > 0 ? queueSize - 1 : 0);
				}
				break;
			}
		}

		// Edit download rate input mode
		if (m_inputMode == downloadRate)
		{
			// Numbers
			if (m_inputNumberIndex < 5 && key >= '0' && key <= '9')
			{
				m_inputValue = (m_inputValue * 10) + (key - '0');
				m_inputNumberIndex++;
			}
			// Enter
			else if (key == 10 || key == 13)
			{
				ServerSetDownloadRate(m_inputValue * 1024);
				m_inputMode = normal;
				return;
			}
			// Escape
			else if (key == 27)
			{
				m_inputMode = normal;
				return;
			}
			// Backspace
			else if (m_inputNumberIndex > 0 && key == KEY_BACKSPACE)
			{
				int remain = m_inputValue % 10;

				m_inputValue = (m_inputValue - remain) / 10;
				m_inputNumberIndex--;
			}
		}

		key = ReadConsoleKey();
	}
}

int NCursesFrontend::ReadConsoleKey()
{
#ifdef WIN32
	HANDLE hConsole = GetStdHandle(STD_INPUT_HANDLE);
	DWORD NumberOfEvents;
	BOOL ok = GetNumberOfConsoleInputEvents(hConsole, &NumberOfEvents);
	if (ok && NumberOfEvents > 0)
	{
		while (NumberOfEvents--)
		{
			INPUT_RECORD InputRecord;
			DWORD NumberOfEventsRead;
			if (ReadConsoleInput(hConsole, &InputRecord, 1, &NumberOfEventsRead) && 
				NumberOfEventsRead > 0 &&
				InputRecord.EventType == KEY_EVENT &&
				InputRecord.Event.KeyEvent.bKeyDown)
			{
				char c = tolower(InputRecord.Event.KeyEvent.wVirtualKeyCode);
				if (bool(InputRecord.Event.KeyEvent.dwControlKeyState & CAPSLOCK_ON) ^
					bool(InputRecord.Event.KeyEvent.dwControlKeyState & SHIFT_PRESSED))
				{
					c = toupper(c);
				}
				return c;
			}
		}
	}
	return READKEY_EMPTY;
#else
	return getch();
#endif
}

#endif
