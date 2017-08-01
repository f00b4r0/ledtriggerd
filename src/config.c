//
//  config.c
//  ledtriggerd
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

#include <uci.h>

#include "config.h"
#include "ledtriggerd.h"

static struct uci_context * Uci_ctx;
static struct uci_package * Uci_pkg;

static void config_init_leds(void)
{
	struct uci_element * e;
	struct uci_section * s;
	const char * sysfsname, * name, * option;
	int defstate;

	dbg("init leds");

	uci_foreach_element(&Uci_pkg->sections, e) {
		s = uci_to_section(e);

		if (!s->type)
			continue;

		if (strcmp(s->type, "led") != 0)
			continue;

		name = s->e.name;
		if (!name)
			continue;	// error

		sysfsname = uci_lookup_option_string(Uci_ctx, s, "sysfs");
		if (!sysfsname)
			continue;	// error

		option = uci_lookup_option_string(Uci_ctx, s, "default");
		if (!option)
			defstate = STATE_KEEP;
		else {
			if (!strcmp(option, "KEEP"))
				defstate = STATE_KEEP;
			else if (!strcmp(option, "ON"))
				defstate = STATE_ON;
			else if (!strcmp(option, "OFF"))
				defstate = STATE_OFF;
			else
				defstate = STATE_MAX;
		}

		if (defstate >= STATE_MAX)
			continue;	// error

		dbg("name: %s, sysfs: %s, state: %d", name, sysfsname, defstate);

		if (new_led(name, sysfsname, defstate))
			dbg("error\n");
	}

	dbg("done.\n");
}

static struct led_trigger * config_parse_led_triggers(const char * name)
{
	struct uci_element * e, * list;
	struct uci_section * s;
	struct uci_option * o;
	struct trigger_params * tparams;
	struct led_trigger * ltrigger = NULL;
	const struct led * led;
	const char * option, * trigger, * sname, * fname, * fvalue;
	char * tmp;

	dbg("parsing led_trigger %s", name);

	uci_foreach_element(&Uci_pkg->sections, e) {
		s = uci_to_section(e);

		if (!s->type)
			continue;

		if (strcmp(s->type, "led_trigger") != 0)
			continue;

		sname = s->e.name;
		if (!sname)
			continue;	// error

		if (strcmp(sname, name) != 0)
			continue;

		dbg("found.");

		option = uci_lookup_option_string(Uci_ctx, s, "led");
		if (!option)
			continue;	// error

		led = led_find_by_name(option);
		if (!led)
			continue;	// error

		dbg("led found.");

		trigger = uci_lookup_option_string(Uci_ctx, s, "trigger");
		if (!trigger)
			continue;	// error

		dbg("trigger: %s", trigger);

		ltrigger = calloc(1, sizeof(*ltrigger));
		if (!ltrigger)
			goto error;

		ltrigger->led = led;
		ltrigger->trigger = trigger;

		o = uci_lookup_option(Uci_ctx, s, "params");
		if (!o || o->type != UCI_TYPE_LIST)
			continue;

		uci_foreach_element(&o->v.list, list) {
			if (!list->name)
				continue;

			tmp = strdup(list->name);	// "mode:link tx"
			if (!tmp)
				continue;	// error

			dbg("param found: %s", tmp);

			fname = strsep(&tmp, ":");
			fvalue = strsep(&tmp, ":");

			dbg("fname: %s, fvalue: %s", fname, fvalue);

			if (!fname || !fvalue) {
				free(tmp);
				continue;	// error
			}

			tparams = calloc(1, sizeof(*tparams));
			if (!tparams)
				goto error;

			tparams->fname = fname;
			tparams->fvalue = fvalue;
			tparams->next = ltrigger->params;
			ltrigger->params = tparams;
		}
	}

	dbg("done.\n");

	return (ltrigger);

error:
	dbg("error\n");
}

static void config_init_led_combos(void)
{
	struct uci_element * e, * list;
	struct uci_section * s;
	struct uci_option * o;
	struct led_trigger * newltrigger, * ltrigger = NULL;
	const char * name, * option;

	dbg("init led combos");

	uci_foreach_element(&Uci_pkg->sections, e) {
		s = uci_to_section(e);

		if (!s->type)
			continue;

		if (strcmp(s->type, "led_combo") != 0)
			continue;

		name = s->e.name;
		if (!name)
			continue;	// error

		dbg("name: %s", name);

		o = uci_lookup_option(Uci_ctx, s, "led_trigger");
		if (!o || o->type != UCI_TYPE_LIST)
			continue;	// error

		uci_foreach_element(&o->v.list, list) {
			option = list->name;	// target combo name

			dbg("ltrigger: %s", option);

			newltrigger = config_parse_led_triggers(option);
			if (!newltrigger)
				continue;	// error

			newltrigger->next = ltrigger;
			ltrigger = newltrigger;
		}

		if (new_led_combo(name, ltrigger))
			dbg("new led combo failed\n");
		else
			dbg("done.\n");
	}
}

static struct uci_package * config_init_package(const char * config)
{
	struct uci_context * ctx = Uci_ctx;
	struct uci_package * pkg = NULL;

	if (!ctx) {
		ctx = uci_alloc_context();
		// error checking

		ctx->flags &= ~UCI_FLAG_STRICT;
		Uci_ctx = ctx;
	}
	else {
		pkg = uci_lookup_package(ctx, config);
		if (pkg)
			uci_unload(ctx, pkg);
	}

	if (uci_load(ctx, config, &pkg))
		return NULL;

	return pkg;
}


int config_init(void)
{
	int ret = 0;

	Uci_pkg = config_init_package("ledtriggerd");
	if (!Uci_pkg) {
		syslog(LOG_CRIT, "Failed to load config\n");
		return -1;
	}

	config_init_leds();
	config_init_led_combos();
	//config_init_events();		// XXX TODO

	return ret;
}

void config_exit(void)
{
	if (Uci_ctx)
		uci_free_context(Uci_ctx);
}
