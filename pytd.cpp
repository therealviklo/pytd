#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <atomic>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include <chrono>
#include <algorithm>

#define DLLEXP __declspec(dllexport)
#define CCALL __cdecl

using Clock = std::chrono::steady_clock;
using namespace std::literals;

extern "C"
{
	DLLEXP bool CCALL pytd_init(void) noexcept;
	DLLEXP void CCALL pytd_uninit(void) noexcept;
	DLLEXP const wchar_t* CCALL pytd_getlasterr(void) noexcept;
	DLLEXP void CCALL pytd_clearerr(void) noexcept;

	DLLEXP short CCALL pytd_getwidth(void) noexcept;
	DLLEXP short CCALL pytd_getheight(void) noexcept;

	DLLEXP bool CCALL pytd_swap(void) noexcept;
	DLLEXP void CCALL pytd_setchar(short x, short y, wchar_t c) noexcept;
	DLLEXP void CCALL pytd_setclr(short x, short y, unsigned short clr) noexcept;
	DLLEXP void CCALL pytd_setcharclr(short x, short y, wchar_t c, unsigned short clr) noexcept;
	DLLEXP void CCALL pytd_clear(wchar_t c, unsigned short clr) noexcept;

	DLLEXP bool CCALL pytd_keydown(int key) noexcept;
}

const wchar_t* err = L"";

DLLEXP const wchar_t* CCALL pytd_getlasterr(void) noexcept
{
	return err;
}

DLLEXP void CCALL pytd_clearerr(void) noexcept
{
	err = L"";
}

namespace
{
	void seterr(const wchar_t* msg) noexcept
	{
		err = msg;
	}
}

HANDLE scrbuf = INVALID_HANDLE_VALUE;
HANDLE oldbuf = INVALID_HANDLE_VALUE;
bool running = false;
std::thread thr;

COORD size{};
std::vector<CHAR_INFO> buf1;
std::vector<CHAR_INFO> buf2;
std::mutex secondBufActive_m;
std::atomic<bool> secondBufActive = false;
std::mutex running_m;
std::condition_variable cv;

namespace
{
	HANDLE setBufferAndGetOld(HANDLE newbuf) noexcept
	{
		const HANDLE oldbuf = GetStdHandle(STD_OUTPUT_HANDLE);
		if (oldbuf == INVALID_HANDLE_VALUE || oldbuf == nullptr)
		{
			return INVALID_HANDLE_VALUE;
		}

		if (!SetConsoleActiveScreenBuffer(
			newbuf
		))
		{
			return INVALID_HANDLE_VALUE;
		}

		return oldbuf;
	}
}

void threadloop() noexcept
{
	Clock::time_point nextFrame = Clock::now();
	std::unique_lock<std::mutex> ul(running_m);
	do
	{
		const std::lock_guard lg(secondBufActive_m);
		SMALL_RECT sr{0, 0, size.X, size.Y};
		WriteConsoleOutputW(
			scrbuf,
			secondBufActive.load(std::memory_order_relaxed) ? buf2.data() : buf1.data(),
			size,
			{0, 0},
			&sr
		);
	} while (nextFrame += 1s / 60, !cv.wait_until(ul, nextFrame, []{ return !running; }));
}

DLLEXP bool CCALL pytd_init(void) noexcept
{
	pytd_clearerr();

	if (scrbuf != INVALID_HANDLE_VALUE)
	{
		seterr(L"Already initialised");
		return false;
	}

	scrbuf = CreateConsoleScreenBuffer(
		GENERIC_WRITE | GENERIC_READ,
		FILE_SHARE_WRITE | FILE_SHARE_READ,
		nullptr,
		CONSOLE_TEXTMODE_BUFFER,
		nullptr
	);
	if (scrbuf == INVALID_HANDLE_VALUE)
	{
		seterr(L"Failed to create screen buffer");
		return false;
	}

	oldbuf = setBufferAndGetOld(scrbuf);
	if (oldbuf == INVALID_HANDLE_VALUE)
	{
		pytd_uninit();
		seterr(L"Failed to set active console screen buffer");
		return false;
	}

	CONSOLE_SCREEN_BUFFER_INFO csbi{};
	if (!GetConsoleScreenBufferInfo(scrbuf, &csbi))
	{
		pytd_uninit();
		seterr(L"Failed to get console screen buffer info");
		return false;
	}
	size = csbi.dwSize;

	buf1.clear();
	buf2.clear();
	try
	{
		buf1.resize(
			size.X * size.Y,
			CHAR_INFO{{L' '}, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY}
		);
		buf2.resize(
			size.X * size.Y,
			CHAR_INFO{{L' '}, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY}
		);
	}
	catch (...)
	{
		pytd_uninit();
		seterr(L"Failed to resize buffers");
		return false;
	}

	try
	{
		const std::lock_guard lg(secondBufActive_m);
		secondBufActive.store(false, std::memory_order_relaxed);
	}
	catch (...)
	{
		pytd_uninit();
		seterr(L"Failed to set active buffer");
		return false;
	}

	try
	{
		const std::lock_guard lg(running_m);
		running = true;
	}
	catch (...)
	{
		pytd_uninit();
		seterr(L"Failed to setup thread condition variable");
		return false;
	}

	try
	{
		std::thread tmp(&threadloop);
		thr.swap(tmp);
	}
	catch (...)
	{
		pytd_uninit();
		seterr(L"Failed to start thread");
		return false;
	}

	return true;
}

DLLEXP void CCALL pytd_uninit(void) noexcept
{
	if (thr.joinable())
	{
		try
		{
			const std::lock_guard lg(running_m);
			running = false;
			cv.notify_all();
		}
		catch (...)
		{
			running = false;
			cv.notify_all();
		}
		try
		{
			thr.join();
		}
		catch (...)
		{
			thr.detach();
		}
	}
	buf2.clear();
	buf1.clear();
	if (oldbuf != INVALID_HANDLE_VALUE)
	{
		SetConsoleActiveScreenBuffer(oldbuf);
		oldbuf = INVALID_HANDLE_VALUE;
	}
	if (scrbuf != INVALID_HANDLE_VALUE)
	{
		CloseHandle(scrbuf);
		scrbuf = INVALID_HANDLE_VALUE;
	}
}

DLLEXP short CCALL pytd_getwidth(void) noexcept
{
	return size.X;
}

DLLEXP short CCALL pytd_getheight(void) noexcept
{
	return size.Y;
}

DLLEXP bool CCALL pytd_swap(void) noexcept
{
	pytd_clearerr();

	try
	{
		const std::lock_guard lg(secondBufActive_m);
		secondBufActive.store(!secondBufActive.load(std::memory_order_relaxed), std::memory_order_relaxed);
	}
	catch (...)
	{
		seterr(L"Failed to lock active buffer mutex");
		return false;
	}

	return true;
}

DLLEXP void CCALL pytd_setchar(short x, short y, wchar_t c) noexcept
{
	if (x >= 0
		&& y >= 0
		&& x < size.X
		&& y < size.Y)
	{
		if (secondBufActive.load(std::memory_order_acquire))
		{
			buf1[x + y * size.X].Char.UnicodeChar = c;
		}
		else
		{
			buf2[x + y * size.X].Char.UnicodeChar = c;
		}
	}
}

DLLEXP void CCALL pytd_setclr(short x, short y, unsigned short clr) noexcept
{
	if (x >= 0
		&& y >= 0
		&& x < size.X
		&& y < size.Y)
	{
		if (secondBufActive.load(std::memory_order_acquire))
		{
			buf1[x + y * size.X].Attributes = clr;
		}
		else
		{
			buf2[x + y * size.X].Attributes = clr;
		}
	}
}

DLLEXP void CCALL pytd_setcharclr(short x, short y, wchar_t c, unsigned short clr) noexcept
{
	if (x >= 0
		&& y >= 0
		&& x < size.X
		&& y < size.Y)
	{
		if (secondBufActive.load(std::memory_order_acquire))
		{
			buf1[x + y * size.X].Char.UnicodeChar = c;
			buf1[x + y * size.X].Attributes = clr;
		}
		else
		{
			buf2[x + y * size.X].Char.UnicodeChar = c;
			buf2[x + y * size.X].Attributes = clr;
		}
	}
}

DLLEXP void CCALL pytd_clear(wchar_t c, unsigned short clr) noexcept
{
	if (secondBufActive.load(std::memory_order_acquire))
	{
		std::fill(buf1.begin(), buf1.end(), CHAR_INFO{{c}, clr});
	}
	else
	{
		std::fill(buf2.begin(), buf2.end(), CHAR_INFO{{c}, clr});
	}
}

DLLEXP bool CCALL pytd_keydown(int key) noexcept
{
	return GetKeyState(key) & 0b1'0000'0000;
}