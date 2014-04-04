/* purple
 *
 * Purple is the legal property of its developers, whose names are too numerous
 * to list here.  Please refer to the COPYRIGHT file distributed with this
 * source distribution.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#include "internal.h"
#include "glibcompat.h"

#include "smiley.h"
#include "smiley-remote.h"

#define PURPLE_REMOTE_SMILEY_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), PURPLE_TYPE_REMOTE_SMILEY, PurpleRemoteSmileyPrivate))

typedef struct {
} PurpleRemoteSmileyPrivate;

#if 0
enum
{
	PROP_0,
	PROP_LAST
};

enum
{
	SIG_READY,
	SIG_LAST
};
#endif

static GObjectClass *parent_class;

#if 0
static guint signals[SIG_LAST];
static GParamSpec *properties[PROP_LAST];
#endif

/******************************************************************************
 * API implementation
 ******************************************************************************/

/******************************************************************************
 * Object stuff
 ******************************************************************************/

#if 0
static void
purple_remote_smiley_init(GTypeInstance *instance, gpointer klass)
{
	PurpleRemoteSmiley *smiley = PURPLE_REMOTE_SMILEY(instance);
}

static void
purple_remote_smiley_finalize(GObject *obj)
{
	PurpleRemoteSmiley *smiley = PURPLE_REMOTE_SMILEY(obj);
	PurpleRemoteSmileyPrivate *priv = PURPLE_REMOTE_SMILEY_GET_PRIVATE(smiley);
}

static void
purple_remote_smiley_get_property(GObject *object, guint par_id, GValue *value,
	GParamSpec *pspec)
{
	PurpleRemoteSmiley *remote_smiley = PURPLE_REMOTE_SMILEY(object);
	PurpleRemoteSmileyPrivate *priv = PURPLE_REMOTE_SMILEY_GET_PRIVATE(remote_smiley);

	switch (par_id) {
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, par_id, pspec);
			break;
	}
}

static void
purple_remote_smiley_set_property(GObject *object, guint par_id, const GValue *value,
	GParamSpec *pspec)
{
	PurpleRemoteSmiley *remote_smiley = PURPLE_REMOTE_SMILEY(object);
	PurpleRemoteSmileyPrivate *priv = PURPLE_REMOTE_SMILEY_GET_PRIVATE(remote_smiley);

	switch (par_id) {
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, par_id, pspec);
			break;
	}
}
#endif

static void
purple_remote_smiley_class_init(PurpleRemoteSmileyClass *klass)
{
//	GObjectClass *gobj_class = G_OBJECT_CLASS(klass);

	parent_class = g_type_class_peek_parent(klass);

	g_type_class_add_private(klass, sizeof(PurpleRemoteSmileyPrivate));

#if 0
	gobj_class->get_property = purple_remote_smiley_get_property;
	gobj_class->set_property = purple_remote_smiley_set_property;
	gobj_class->finalize = purple_remote_smiley_finalize;

	g_object_class_install_properties(gobj_class, PROP_LAST, properties);
#endif
}

GType
purple_remote_smiley_get_type(void)
{
	static GType type = 0;

	if (G_UNLIKELY(type == 0)) {
		static const GTypeInfo info = {
			.class_size = sizeof(PurpleRemoteSmileyClass),
			.class_init = (GClassInitFunc)purple_remote_smiley_class_init,
			.instance_size = sizeof(PurpleRemoteSmiley),
//			.instance_init = purple_remote_smiley_init,
		};

		type = g_type_register_static(PURPLE_TYPE_REMOTE_SMILEY,
			"PurpleRemoteSmiley", &info, 0);
	}

	return type;
}
