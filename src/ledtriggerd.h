//
//  ledtriggerd.h
//  A simple daemon for LED management
//
//  (C) 2017 Thibaut VARENE
//  License: GPLv2 - http://www.gnu.org/licenses/gpl-2.0.html
//

#ifndef ledtriggerd_h
#define ledtriggerd_h

#define SYS_LEDS_PATH	"/sys/class/leds/"
#define SYS_LED_TRIGGER	"trigger"

#define dbg(format, ...)	do { printf("[%s:%d] (%s()) " format "\n", __FILE__, __LINE__, __func__, ## __VA_ARGS__); fflush(stdout); } while(0)

enum {
	STATE_KEEP = 0,
	STATE_ON,
	STATE_OFF,
	STATE_MAX,
};

struct led {
	const char * name;
	const char * sysfsname;
	unsigned int defstate;
	struct led * next;
};

struct trigger_params {
	char * fname;
	const char * fvalue;
	struct trigger_params * next;
};

struct led_trigger {
	const struct led * led;
	const char * trigger;
	const struct trigger_params * params;
	struct led_trigger * next;
};

struct led_combo {
	const char * name;
	const struct led_trigger * ledtrigs;
	struct led_combo * next;
};

struct ubus_event {
	char * namespace;
	char * blob_attr;
};

struct trigger {
	const char * name;
	char * ubus_event;
	struct led_combo * ledcombo;
	struct trigger * next;
};

int new_led(const char * name, const char * sysfsname, const int defstate);
int new_led_combo(const char * name, const struct led_trigger * ltrigger);
const struct led * led_find_by_name(const char * const name);
const struct led_combo * led_combo_find_by_name(const char * const name);

#endif /* ledtriggerd_h */