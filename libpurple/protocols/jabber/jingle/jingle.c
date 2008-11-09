/*
 * @file jingle.c
 *
 * purple - Jabber Protocol Plugin
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
 *
 */

#include "config.h"
#include "content.h"
#include "debug.h"
#include "jingle.h"
#include <string.h>
#include "session.h"
#include "rawudp.h"
#include "rtp.h"

const gchar *
jingle_get_action_name(JingleActionType action)
{
	switch (action) {
		case JINGLE_CONTENT_ACCEPT:
			return "content-accept";
		case JINGLE_CONTENT_ADD:
			return "content-add";
		case JINGLE_CONTENT_MODIFY:
			return "content-modify";
		case JINGLE_CONTENT_REMOVE:
			return "content-remove";
		case JINGLE_SESSION_ACCEPT:
			return "session-accept";
		case JINGLE_SESSION_INFO:
			return "session-info";
		case JINGLE_SESSION_INITIATE:
			return "session-initiate";
		case JINGLE_SESSION_TERMINATE:
			return "session-terminate";
		case JINGLE_TRANSPORT_ACCEPT:
			return "transport-accept";
		case JINGLE_TRANSPORT_INFO:
			return "transport-info";
		case JINGLE_TRANSPORT_REPLACE:
			return "transport-replace";
		default:
			return "unknown-type";
	}
}

JingleActionType
jingle_get_action_type(const gchar *action)
{
	if (!strcmp(action, "content-accept"))
		return JINGLE_CONTENT_ACCEPT;
	else if (!strcmp(action, "content-add"))
		return JINGLE_CONTENT_ADD;
	else if (!strcmp(action, "content-modify"))
		return JINGLE_CONTENT_MODIFY;
	else if (!strcmp(action, "content-remove"))
		return JINGLE_CONTENT_REMOVE;
	else if (!strcmp(action, "session-accept"))
		return JINGLE_SESSION_ACCEPT;
	else if (!strcmp(action, "session-info"))
		return JINGLE_SESSION_INFO;
	else if (!strcmp(action, "session-initiate"))
		return JINGLE_SESSION_INITIATE;
	else if (!strcmp(action, "session-terminate"))
		return JINGLE_SESSION_TERMINATE;
	else if (!strcmp(action, "transport-accept"))
		return JINGLE_TRANSPORT_ACCEPT;
	else if (!strcmp(action, "transport-info"))
		return JINGLE_TRANSPORT_INFO;
	else if (!strcmp(action, "transport-replace"))
		return JINGLE_TRANSPORT_REPLACE;
	else
		return JINGLE_UNKNOWN_TYPE;
}

GType
jingle_get_type(const gchar *type)
{
	if (!strcmp(type, JINGLE_APP_RTP))
		return JINGLE_TYPE_RTP;
#if 0
	else if (!strcmp(type, JINGLE_APP_FT))
		return JINGLE_TYPE_FT;
	else if (!strcmp(type, JINGLE_APP_XML))
		return JINGLE_TYPE_XML;
#endif
	else if (!strcmp(type, JINGLE_TRANSPORT_RAWUDP))
		return JINGLE_TYPE_RAWUDP;
#if 0
	else if (!strcmp(type, JINGLE_TRANSPORT_ICEUDP))
		return JINGLE_TYPE_ICEUDP;
	else if (!strcmp(type, JINGLE_TRANSPORT_SOCKS))
		return JINGLE_TYPE_SOCKS;
	else if (!strcmp(type, JINGLE_TRANSPORT_IBB))
		return JINGLE_TYPE_IBB;
#endif
	else
		return G_TYPE_NONE;
}

static void
jingle_handle_content_accept(JingleSession *session, xmlnode *jingle)
{
	xmlnode *content = xmlnode_get_child(jingle, "content");
	jabber_iq_send(jingle_session_create_ack(session, jingle));

	for (; content; content = xmlnode_get_next_twin(content)) {
		const gchar *name = xmlnode_get_attrib(content, "name");
		const gchar *creator = xmlnode_get_attrib(content, "creator");
		jingle_session_accept_content(session, name, creator);
		/* signal here */
	}
}

static void
jingle_handle_content_add(JingleSession *session, xmlnode *jingle)
{
	xmlnode *content = xmlnode_get_child(jingle, "content");
	jabber_iq_send(jingle_session_create_ack(session, jingle));

	for (; content; content = xmlnode_get_next_twin(content)) {
		JingleContent *pending_content =
				jingle_content_parse(content);
		if (pending_content == NULL) {
			purple_debug_error("jingle",
					"Error parsing \"content-add\" content.\n");
			/* XXX: send error here */
		} else {
			jingle_session_add_pending_content(session,
					pending_content);
		}
	}

	/* XXX: signal here */
}

static void
jingle_handle_content_modify(JingleSession *session, xmlnode *jingle)
{
	xmlnode *content = xmlnode_get_child(jingle, "content");
	jabber_iq_send(jingle_session_create_ack(session, jingle));

	for (; content; content = xmlnode_get_next_twin(content)) {
		const gchar *name = xmlnode_get_attrib(content, "name");
		const gchar *creator = xmlnode_get_attrib(content, "creator");
		JingleContent *local_content = jingle_session_find_content(session, name, creator);

		if (content != NULL) {
			const gchar *senders = xmlnode_get_attrib(content, "senders");
			gchar *local_senders = jingle_content_get_senders(local_content);
			if (strcmp(senders, local_senders))
				jingle_content_modify(local_content, senders);
			g_free(local_senders);
		} else {
			purple_debug_error("jingle", "content_modify: unknown content\n");
			/* XXX: send error */
		}
	}
}

static void
jingle_handle_content_reject(JingleSession *session, xmlnode *jingle)
{
	xmlnode *content = xmlnode_get_child(jingle, "content");
	jabber_iq_send(jingle_session_create_ack(session, jingle));

	for (; content; content = xmlnode_get_next_twin(content)) {
		const gchar *name = xmlnode_get_attrib(content, "name");
		const gchar *creator = xmlnode_get_attrib(content, "creator");
		jingle_session_remove_pending_content(session, name, creator);
		/* signal here */
	}
}

static void
jingle_handle_content_remove(JingleSession *session, xmlnode *jingle)
{
	xmlnode *content = xmlnode_get_child(jingle, "content");

	jabber_iq_send(jingle_session_create_ack(session, jingle));

	for (; content; content = xmlnode_get_next_twin(content)) {
		const gchar *name = xmlnode_get_attrib(content, "name");
		const gchar *creator = xmlnode_get_attrib(content, "creator");
		jingle_session_remove_content(session, name, creator);
	}
}

static void
jingle_handle_session_accept(JingleSession *session, xmlnode *jingle)
{
	xmlnode *content = xmlnode_get_child(jingle, "content");

	jabber_iq_send(jingle_session_create_ack(session, jingle));

	jingle_session_accept_session(session);
	
	for (; content; content = xmlnode_get_next_twin(content)) {
		const gchar *name = xmlnode_get_attrib(content, "name");
		const gchar *creator = xmlnode_get_attrib(content, "creator");
		JingleContent *parsed_content =
				jingle_session_find_content(session, name, creator);
		if (parsed_content == NULL) {
			purple_debug_error("jingle", "Error parsing content\n");
			/* XXX: send error */
		} else {
			jingle_content_handle_action(parsed_content, content,
					JINGLE_SESSION_ACCEPT);
		}
	}
}

static void
jingle_handle_session_info(JingleSession *session, xmlnode *jingle)
{
	jabber_iq_send(jingle_session_create_ack(session, jingle));
	/* XXX: call signal */
}

static void
jingle_handle_session_initiate(JingleSession *session, xmlnode *jingle)
{
	xmlnode *content = xmlnode_get_child(jingle, "content");

	for (; content; content = xmlnode_get_next_twin(content)) {
		JingleContent *parsed_content = jingle_content_parse(content);
		if (parsed_content == NULL) {
			purple_debug_error("jingle", "Error parsing content\n");
			/* XXX: send error */
		} else {
			jingle_session_add_content(session, parsed_content);
			jingle_content_handle_action(parsed_content, content,
					JINGLE_SESSION_INITIATE);
		}
	}

	jabber_iq_send(jingle_session_create_ack(session, jingle));
}

static void
jingle_handle_session_terminate(JingleSession *session, xmlnode *jingle)
{
	jabber_iq_send(jingle_session_create_ack(session, jingle));

	jingle_session_handle_action(session, jingle,
			JINGLE_SESSION_TERMINATE);
	/* display reason? */
	g_object_unref(session);
}

static void
jingle_handle_transport_accept(JingleSession *session, xmlnode *jingle)
{
	xmlnode *content = xmlnode_get_child(jingle, "content");

	jabber_iq_send(jingle_session_create_ack(session, jingle));
	
	for (; content; content = xmlnode_get_next_twin(content)) {
		const gchar *name = xmlnode_get_attrib(content, "name");
		const gchar *creator = xmlnode_get_attrib(content, "creator");
		JingleContent *content = jingle_session_find_content(session, name, creator);
		jingle_content_accept_transport(content);
	}
}

static void
jingle_handle_transport_info(JingleSession *session, xmlnode *jingle)
{
	xmlnode *content = xmlnode_get_child(jingle, "content");

	jabber_iq_send(jingle_session_create_ack(session, jingle));

	for (; content; content = xmlnode_get_next_twin(content)) {
		const gchar *name = xmlnode_get_attrib(content, "name");
		const gchar *creator = xmlnode_get_attrib(content, "creator");
		JingleContent *parsed_content = 
				jingle_session_find_content(session, name, creator);
		if (parsed_content == NULL) {
			purple_debug_error("jingle", "Error parsing content\n");
			/* XXX: send error */
		} else {
			jingle_content_handle_action(parsed_content, content,
					JINGLE_TRANSPORT_INFO);
		}
	}
}

static void
jingle_handle_transport_reject(JingleSession *session, xmlnode *jingle)
{
	xmlnode *content = xmlnode_get_child(jingle, "content");

	jabber_iq_send(jingle_session_create_ack(session, jingle));
	
	for (; content; content = xmlnode_get_next_twin(content)) {
		const gchar *name = xmlnode_get_attrib(content, "name");
		const gchar *creator = xmlnode_get_attrib(content, "creator");
		JingleContent *content = jingle_session_find_content(session, name, creator);
		jingle_content_remove_pending_transport(content);
	}
}

static void
jingle_handle_transport_replace(JingleSession *session, xmlnode *jingle)
{
	xmlnode *content = xmlnode_get_child(jingle, "content");

	jabber_iq_send(jingle_session_create_ack(session, jingle));

	for (; content; content = xmlnode_get_next_twin(content)) {
		const gchar *name = xmlnode_get_attrib(content, "name");
		const gchar *creator = xmlnode_get_attrib(content, "creator");
		xmlnode *xmltransport = xmlnode_get_child(content, "transport");
		JingleTransport *transport = jingle_transport_parse(xmltransport);
		JingleContent *content = jingle_session_find_content(session, name, creator);

		jingle_content_set_pending_transport(content, transport);
	}
}


void
jingle_parse(JabberStream *js, xmlnode *packet)
{
	const gchar *type = xmlnode_get_attrib(packet, "type");
	xmlnode *jingle;
	const gchar *action;
	const gchar *sid;
	JingleSession *session;

	if (!type || strcmp(type, "set")) {
		/* send iq error here */
		return;
	}

	/* is this a Jingle package? */
	if (!(jingle = xmlnode_get_child(packet, "jingle"))) {
		/* send iq error here */
		return;
	}

	if (!(action = xmlnode_get_attrib(jingle, "action"))) {
		/* send iq error here */
		return;
	}

	purple_debug_info("jabber", "got Jingle package action = %s\n",
			  action);

	if (!(sid = xmlnode_get_attrib(jingle, "sid"))) {
		/* send iq error here */
		return;
	}

	if (!(session = jingle_session_find_by_sid(js, sid))
			&& strcmp(action, "session-initiate")) {
		purple_debug_error("jingle", "jabber_jingle_session_parse couldn't find session\n");
		/* send iq error here */
		return;
	}

	if (!strcmp(action, "session-initiate")) {
		if (session) {
			/* This should only happen if you start a session with yourself */
			purple_debug_error("jingle", "Jingle session with "
					"id={%s} already exists\n", sid);
			/* send iq error */
		} else if ((session = jingle_session_find_by_jid(js,
				xmlnode_get_attrib(packet, "from")))) {
			purple_debug_fatal("jingle", "Jingle session with "
					"jid={%s} already exists\n",
					xmlnode_get_attrib(packet, "from"));
			/* send jingle redirect packet */
			return;
		} else {
			session = jingle_session_create(js, sid,
					xmlnode_get_attrib(packet, "to"),
					xmlnode_get_attrib(packet, "from"), FALSE);
			jingle_handle_session_initiate(session, jingle);
		}
	} else if (!strcmp(action, "content-accept")) {
		jingle_handle_content_accept(session, jingle);
	} else if (!strcmp(action, "content-add")) {
		jingle_handle_content_add(session, jingle);
	} else if (!strcmp(action, "content-modify")) {
		jingle_handle_content_modify(session, jingle);
	} else if (!strcmp(action, "content-reject")) {
		jingle_handle_content_reject(session, jingle);
	} else if (!strcmp(action, "content-remove")) {
		jingle_handle_content_remove(session, jingle);
	} else if (!strcmp(action, "session-accept")) {
		jingle_handle_session_accept(session, jingle);
	} else if (!strcmp(action, "session-info")) {
		jingle_handle_session_info(session, jingle);
	} else if (!strcmp(action, "session-terminate")) {
		jingle_handle_session_terminate(session, jingle);
	} else if (!strcmp(action, "transport-accept")) {
		jingle_handle_transport_accept(session, jingle);
	} else if (!strcmp(action, "transport-info")) {
		jingle_handle_transport_info(session, jingle);
	} else if (!strcmp(action, "transport-reject")) {
		jingle_handle_transport_reject(session, jingle);
	} else if (!strcmp(action, "transport-replace")) {
		jingle_handle_transport_replace(session, jingle);
	}
}

void
jingle_terminate_sessions(JabberStream *js)
{
	GList *values = js->sessions ?
			g_hash_table_get_values(js->sessions) : NULL;

	for (; values; values = g_list_delete_link(values, values)) {
		JingleSession *session = (JingleSession *)values->data;
		g_object_unref(session);
	}
}
