#define GAIM_PLUGINS

#include <stdio.h>
#include "gaim.h"

static GModule *handle = NULL;

char *gaim_plugin_init(GModule *h) {
	printf("plugin loaded.\n");
	handle = h;
	return NULL;
}

void gaim_plugin_remove() {
	printf("plugin unloaded.\n");
	handle = NULL;
}

void gaim_plugin_config() {
	printf("configuring plugin.\n");
}

char *name() {
	return "Simple Plugin Version 1.0";
}

char *description() {
	return "Tests to see that most things are working.";
}
