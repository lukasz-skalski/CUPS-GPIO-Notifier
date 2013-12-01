GCC=gcc
CFLAGS=-Wall $(shell pkg-config --cflags gio-2.0)
LIBS=$(shell pkg-config --libs gio-2.0)

default: cups_gpio_notifier

cups_gpio_notifier:
	$(GCC) cups_gpio_notifier.c -o cups_gpio_notifier $(LIBS) $(CFLAGS)

clean:
	-rm -f cups_gpio_notifier
	-rm -f *~

install:
	cp cups_gpio_notifier /usr/local/bin/
	chmod 755 /usr/local/bin/cups_gpio_notifier
	cp cups_gpio_notifier_sysv /etc/init.d/
	chmod 755 /etc/init.d/cups_gpio_notifier_sysv
	insserv cups_gpio_notifier_sysv

