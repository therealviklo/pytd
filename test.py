import pytd
from time import sleep

pytd.init()
w = pytd.getwidth()
h = pytd.getheight()
x = 0
y = 0
while True:
	pytd.clear(' ', 15)
	if pytd.keydown(87):
		y = (y - 1) % h
	if pytd.keydown(65):
		x = (x - 1) % w
	if pytd.keydown(83):
		y = (y + 1) % h
	if pytd.keydown(68):
		x = (x + 1) % w
	if pytd.keydown(0x1b):
		break
	pytd.setchar(x, y, 'x')
	pytd.swap()
	sleep(1 / 60)
pytd.uninit()