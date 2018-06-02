/*
 * Aasys Sresta
 * GPIO fun kernel arch
 * Instructions:
 *      add 'gpiofun' under 'link' in 'pi2' file
 *      build using - mk 'CONF=pi2'
 */

#include "u.h"
#include "../port/lib.h"
#include "../port/error.h"
#include "mem.h"
#include "dat.h"
#include "fns.h"
#include "io.h"

static long gpiofun_read(Chan*, void*, long, vlong);
static long gpiofun_write(Chan*, void*, long, vlong);

static void led_blink(void *);
static void switch_scan(void *);

static int led_state = 0;    // state of led = OFF, ON, BLINK
static int scan_state = 0;  // state of switch scan = OFF, ON
static int switch_state = 0; // last recored state of switch = OFF, ON

#define SWITCH_PIN 27
#define LED_PIN 22

#define OFF 0
#define ON 1
#define LED_BLINK 2

/////////////////////

// gpio pin, read/write utility functions adapted from "gpio.c"

#define GPIOREGS_	(VIRTIO+0x200000)

/* GPIO regs */
enum {
	Set0_	= 0x1c>>2,
	Clr0_	= 0x28>>2,
	Lev0_	= 0x34>>2
};

void
gpio_set(uint pin, int set)
{
	u32int *gp;
	int v;

	gp = (u32int*)GPIOREGS_;
	v = set? Set0_ : Clr0_;
	gp[v + pin/32] = 1 << (pin % 32);
}

int
gpio_read(uint pin)
{
	u32int *gp;

	gp = (u32int*)GPIOREGS_;
	return (gp[Lev0_ + pin/32] & (1 << (pin % 32))) != 0;
}

////////////////////

void
gpiofunlink(void) {
	addarchfile("gpiofun", 0664, gpiofun_read, gpiofun_write);
}


//return state of LED and SCAN
static long gpiofun_read(Chan *, void *a, long n, vlong offset) {	
	char str[128];
	char * led_str;
	char * scan_str;
	
	if (scan_state == OFF) {
		scan_str = "OFF";
	} else {
		scan_str = "ON";
	}

	if (led_state == OFF)  {
		led_str = "OFF";
	} else if (led_state == ON) {
		led_str = "ON";
	} else {
		led_str = "BLINK";
	}

	snprint(str, sizeof str, "SWITCH SCAN: %s | LED: %s\n", scan_str, led_str);	

	return readstr(offset, a, n, str);
}

//react to commands - start, stop, on, off, blink
//ignore if commanded state is current state
static long gpiofun_write(Chan *, void * a, long , vlong) {
	if (strncmp(a, "start", 5) == 0 && scan_state == OFF)  {
		scan_state = ON;
		kproc("gpiofun_scan", switch_scan, nil);
	} else if (strncmp(a, "stop", 4) == 0 && scan_state == ON)  {
		scan_state = OFF;
	} else if (strncmp(a, "on", 2) == 0 && led_state != ON) {
		led_state = ON;
		gpio_set(LED_PIN, ON);
	} else if (strncmp(a, "off", 3) == 0 && led_state != OFF) {
		led_state = OFF;
		gpio_set(LED_PIN, OFF);
	} else if (strncmp(a, "blink", 5) == 0 && led_state != LED_BLINK) {
		led_state = LED_BLINK;
		kproc("gpiofun_blink", led_blink, nil);
	}
	return 0;
}

// blink led at 200ms rate
static void led_blink(void *) {
	int blink_state = OFF;
	while (led_state == LED_BLINK) {
		gpio_set(LED_PIN, blink_state);
		blink_state = (blink_state == OFF) ? ON : OFF;

		tsleep(&up->sleep, return0, 0, 200);
	}
}

static void switch_scan(void *) {
	while (scan_state == ON) {
		int new_switch_state = gpio_read(SWITCH_PIN);
		
		if (new_switch_state == ON && switch_state == OFF) {
			switch_state = ON;
			if (led_state == OFF) {
				led_state = LED_BLINK;
				kproc("gpiofun_blink", led_blink, nil);
			} else if (led_state == ON) {
				led_state = OFF;
				gpio_set(LED_PIN, OFF);
			} else {
				led_state = ON;
				gpio_set(LED_PIN, ON);
			}		
		} else if (new_switch_state == OFF && switch_state == ON) {
			switch_state = OFF;
		}

		tsleep(&up->sleep, return0, 0, 100);
	}
}
