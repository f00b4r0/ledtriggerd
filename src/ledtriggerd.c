//
//  ledtriggerd.c
//  A simple daemon for LED management
//
//  (C) 2017 Thibaut VARENE
//  License: GPLv2 - http://www.gnu.org/licenses/gpl-2.0.html
//


#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>

#include "ledtriggerd.h"
#include "config.h"

const struct trigger_params tparams_none_on = {
	.fname = "brightness",
	.fvalue = "255",
	.next = NULL,
};

const struct trigger_params tparams_none_off = {
	.fname = "brightness",
	.fvalue = "0",
	.next = NULL,
};

static struct led * Led_head = NULL;
static struct led_combo * Lcombo_head = NULL;
static struct trigger * Trigger_head = NULL;

/**
 * Find a LED combination by name.
 * @param name target combination name
 * @return led_combo struct if found, NULL otherwise
 */
const struct led_combo * led_combo_find_by_name(const char * const name)
{
	const struct led_combo * combo;

	for (combo = Lcombo_head; combo; combo = combo->next) {
		if (!strcmp(combo->name, name))
			break;
	}

	return (combo);
}

/**
 * Find a LED by name.
 * @param name target LED name
 * @return led struct if found, NULL otherwise
 */
const struct led * led_find_by_name(const char * const name)
{
	const struct led * led;

	for (led = Led_head; led; led = led->next) {
		if (!strcmp(led->name, name))
			break;
	}

	return (led);
}



/**
 * Apply a list of LED triggers.
 * @param led_triggers the list of triggers to apply
 * @return exec status
 */
static int apply_led_triggers(const struct led_trigger * const led_triggers)
{
	const struct led_trigger * ledtrig;
	const struct trigger_params * tparams;
	FILE * fp = NULL;

	for (ledtrig = led_triggers; ledtrig; ledtrig = ledtrig->next) {
		dbg("trigger %s for led %s", ledtrig->trigger, ledtrig->led->name);

		if (chdir(SYS_LEDS_PATH))
			goto error;

		if (chdir(ledtrig->led->sysfsname))
			goto error;

		fp = fopen(SYS_LED_TRIGGER, "w");
		if (!fp)
			goto error;

		if (fprintf(fp, "%s", ledtrig->trigger) != strlen(ledtrig->trigger)) {
			syslog(LOG_ERR, "failed setting trigger \"%s\" for led \"%s\": %s\n",
			       ledtrig->trigger, ledtrig->led->name, strerror(errno));
			goto error;
		}

		fclose(fp);

		for (tparams = ledtrig->params; tparams; tparams = tparams->next) {
			fp = fopen(tparams->fname, "w");
			if (!fp) {
				syslog(LOG_ERR, "Cannot open \"%s\" for led \"%s\"\n",
				       tparams->fname, ledtrig->led->name);
				goto error;
			}

			if (fprintf(fp, "%s", tparams->fvalue) != strlen(tparams->fvalue)) {
				syslog(LOG_ERR, "Cannot set \"%s\" to \"%s\" for led \"%s\"\n",
				       tparams->fname, tparams->fvalue, ledtrig->led->name);
				goto error;
			}

			fclose(fp);
		}
	}

	return (0);

error:
	if (fp)
		fclose(fp);
	return (-1);
}

// event matcher - params: current event, list of triggers
static int match_event(void * event, const struct trigger * const triggers)
{
	const struct trigger * trigger;

	for (trigger = triggers; trigger; trigger = trigger->next) {
		// match ubus event appropriately
		if (1 /* match found */) {
			apply_led_triggers(trigger->ledcombo->ledtrigs);
			//syslog match?
		}
	}
}

/**
 * Create and initialize a new LED.
 * @param name unique name for this LED
 * @param sysfsname target led sysfs link name under SYS_LEDS_PATH
 * @param defstate initialization target state
 * @return exec status.
 */
int new_led(const char * name, const char * sysfsname, const int defstate)
{
	static struct led_trigger ltrigger;
	struct led * newled = NULL;

	if (!name || !sysfsname || (defstate >= STATE_MAX)) {
		// XXX input sanitization should be handled by caller
		goto error;
	}

	if (led_find_by_name(name)) {
		syslog(LOG_ERR, "Duplicate led entry: \"%s\"\n", name);
		goto error;
	}

	if (chdir(SYS_LEDS_PATH))
		goto error;

	if (chdir(sysfsname)) {
		syslog(LOG_ERR, "Cannot access LED \"%s\" at \"%s\"\n", name, sysfsname);
		goto error;
	}

	newled = calloc(1, sizeof(*newled));
	if (!newled)
		goto error;

	newled->name = name;
	newled->sysfsname = sysfsname;
	newled->defstate = defstate;

	if (STATE_KEEP != defstate) {
		ltrigger.led = newled;
		ltrigger.trigger = "none";
		ltrigger.params = (STATE_ON == defstate) ? &tparams_none_on : &tparams_none_off;
		ltrigger.next = NULL;

		if (apply_led_triggers(&ltrigger))
			goto error;
	}

	newled->next = Led_head;
	Led_head = newled;

	return (0);

error:
	free(newled);
	return (-1);
}

/**
 * Create and initialize a new LED combination.
 * @param name unique name for this combination
 * @param ltrigger list of triggers for this combination
 * @return exec status.
 */
int new_led_combo(const char * name, const struct led_trigger * ltrigger)
{
	struct led_combo * newlcombo;
	struct trigger_params * tparams;
	int ret = -1;

	if (!name || !ltrigger)
		goto error;

	newlcombo = calloc(1, sizeof(*newlcombo));
	if (!newlcombo)
		goto error;

	newlcombo->name = name;
	newlcombo->ledtrigs = ltrigger;

	newlcombo->next = Lcombo_head;
	Lcombo_head = newlcombo;

	ret = 0;

error:
	return (ret);
}


/**
 * Init the daemon.
 */
static int ledtriggerd_init(void)
{
	int ret = 0;

	if (chdir(SYS_LEDS_PATH)) {
		syslog(LOG_CRIT, "sysfs path not accessible!\n");
		ret = 1;
		goto abort;
	}

	// parse config
	ret = config_init();
	if (ret)
		goto abort;

	// setup event listener

abort:
	return ret;
}

// cleanup

/**
 * Delete a trigger.
 * @param trigger target trigger to delete
 */
static void del_trigger(struct trigger * trigger)
{
}

/**
 * Delete a LED combination.
 * @param combo target led_combo to delete
 */
static void del_led_combo(struct led_combo * combo)
{
	struct led_trigger * ltrigger, * ltriggern;
	struct trigger_params * tparams, * tparamsn;

	// clean the led_trigger list
	ltrigger = combo->ledtrigs;
	while (ltrigger) {
		ltriggern = ltrigger->next;

		// clean the parameter list
		tparams = ltrigger->params;
		while (tparams) {
			tparamsn = tparams->next;
			free(tparams->fname);	// fvalue is an offset of fname
			free(tparams);
			tparams = tparamsn;
		}

		free(ltrigger);
		ltrigger = ltriggern;
	}
}

/**
 * Delete a LED.
 * @param led target led to delete
 * @todo exit led state?
 */
static void del_led(struct led * led)
{
}

/**
 * Cleanup before exiting.
 * Frees all allocated memory.
 */
static void cleanup(void)
{
	struct trigger * trigger, * triggern;
	struct led_combo * combo, * combon;
	struct led * led, * ledn;

	// clean triggers
	trigger = Trigger_head;
	while (trigger) {
		triggern = trigger->next;
		del_trigger(trigger);
		free(trigger);
		trigger = triggern;
	}

	// clean combos
	combo = Lcombo_head;
	while (combo) {
		combon = combo->next;
		del_led_combo(combo);
		free(combo);
		combo = combon;
	}

	// clean leds
	led = Led_head;
	while (led) {
		ledn = led->next;
		del_led(led);
		free(led);
		led = ledn;
	}
}

static void ledtriggerd_exit(void)
{
	config_exit();
	cleanup();
}

int main(void)
{
	int ret;

	// init
	ret = ledtriggerd_init();
	if (ret) {
		dbg("failed init\n");
		goto exit;
	}

	// daemonize


	if (1) {	// match event
		ret = apply_led_triggers(Lcombo_head->ledtrigs);
	}

	ledtriggerd_exit();

exit:
	return (ret);
}
