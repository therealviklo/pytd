from ctypes import *

class PytdException(Exception):
	def __init__(self, msg):
		self.msg = msg

pytd_dll = CDLL(".\\pytd.dll")
pytd_dll.pytd_init.argtypes = []
pytd_dll.pytd_init.restype = c_bool
pytd_dll.pytd_uninit.argtypes = []
pytd_dll.pytd_uninit.restype = None
pytd_dll.pytd_getlasterr.argtypes = []
pytd_dll.pytd_getlasterr.restype = c_wchar_p
pytd_dll.pytd_clearerr.argtypes = []
pytd_dll.pytd_clearerr.restype = None
pytd_dll.pytd_getwidth.argtypes = []
pytd_dll.pytd_getwidth.restype = c_short
pytd_dll.pytd_getheight.argtypes = []
pytd_dll.pytd_getheight.restype = c_short
pytd_dll.pytd_swap.argtypes = []
pytd_dll.pytd_swap.restype = c_bool
pytd_dll.pytd_setchar.argtypes = [c_short, c_short, c_wchar]
pytd_dll.pytd_setchar.restype = None
pytd_dll.pytd_setclr.argtypes = [c_short, c_short, c_ushort]
pytd_dll.pytd_setclr.restype = None
pytd_dll.pytd_setcharclr.argtypes = [c_short, c_short, c_wchar, c_ushort]
pytd_dll.pytd_setcharclr.restype = None
pytd_dll.pytd_clear.argtypes = [c_wchar, c_ushort]
pytd_dll.pytd_clear.restype = None
pytd_dll.pytd_keydown.argtypes = [c_int]
pytd_dll.pytd_keydown.restype = c_bool

def throw_last_err():
	raise PytdException(pytd_dll.pytd_getlasterr())

def init():
	if not pytd_dll.pytd_init():
		throw_last_err()

def uninit():
	pytd_dll.pytd_uninit()

def getwidth():
	return pytd_dll.pytd_getwidth()

def getheight():
	return pytd_dll.pytd_getheight()

def swap():
	if not pytd_dll.pytd_swap():
		throw_last_err()

def setchar(x, y, c):
	pytd_dll.pytd_setchar(c_short(x), c_short(y), c_wchar(c))

def setclr(x, y, clr):
	pytd_dll.pytd_setchar(c_short(x), c_short(y), c_ushort(clr))

def setcharclr(x, y, c, clr):
	pytd_dll.pytd_setchar(c_short(x), c_short(y), c_wchar(c), c_ushort(clr))

def clear(c = ' ', clr = 15):
	pytd_dll.pytd_clear(c_wchar(c), c_ushort(clr))

def keydown(key):
	return pytd_dll.pytd_keydown(c_int(key))