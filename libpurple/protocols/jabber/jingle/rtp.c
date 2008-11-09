/**
 * @file rtp.c
 *
 * purple
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

#include "config.h"

#include "jabber.h"
#include "jingle.h"
#include "media.h"
#include "mediamanager.h"
#include "rawudp.h"
#include "rtp.h"
#include "session.h"
#include "debug.h"

#include <string.h>

struct _JingleRtpPrivate
{
	gchar *media_type;
	gboolean candidates_ready;
	gboolean codecs_ready;
};

#define JINGLE_RTP_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE((obj), JINGLE_TYPE_RTP, JingleRtpPrivate))

static void jingle_rtp_class_init (JingleRtpClass *klass);
static void jingle_rtp_init (JingleRtp *rtp);
static void jingle_rtp_finalize (GObject *object);
static void jingle_rtp_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static void jingle_rtp_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static JingleContent *jingle_rtp_parse_internal(xmlnode *rtp);
static xmlnode *jingle_rtp_to_xml_internal(JingleContent *rtp, xmlnode *content, JingleActionType action);
static void jingle_rtp_handle_action_internal(JingleContent *content, xmlnode *jingle, JingleActionType action);

static PurpleMedia *jingle_rtp_get_media(JingleSession *session);

static JingleContentClass *parent_class = NULL;
#if 0
enum {
	LAST_SIGNAL
};
static guint jingle_rtp_signals[LAST_SIGNAL] = {0};
#endif

enum {
	PROP_0,
	PROP_MEDIA_TYPE,
};

GType
jingle_rtp_get_type()
{
	static GType type = 0;

	if (type == 0) {
		static const GTypeInfo info = {
			sizeof(JingleRtpClass),
			NULL,
			NULL,
			(GClassInitFunc) jingle_rtp_class_init,
			NULL,
			NULL,
			sizeof(JingleRtp),
			0,
			(GInstanceInitFunc) jingle_rtp_init,
			NULL
		};
		type = g_type_register_static(JINGLE_TYPE_CONTENT, "JingleRtp", &info, 0);
	}
	return type;
}

static void
jingle_rtp_class_init (JingleRtpClass *klass)
{
	GObjectClass *gobject_class = (GObjectClass*)klass;
	parent_class = g_type_class_peek_parent(klass);

	gobject_class->finalize = jingle_rtp_finalize;
	gobject_class->set_property = jingle_rtp_set_property;
	gobject_class->get_property = jingle_rtp_get_property;
	klass->parent_class.to_xml = jingle_rtp_to_xml_internal;
	klass->parent_class.parse = jingle_rtp_parse_internal;
	klass->parent_class.description_type = JINGLE_APP_RTP;
	klass->parent_class.handle_action = jingle_rtp_handle_action_internal;

	g_object_class_install_property(gobject_class, PROP_MEDIA_TYPE,
			g_param_spec_string("media-type",
			"Media Type",
			"The media type (\"audio\" or \"video\") for this rtp session.",
			NULL,
			G_PARAM_READWRITE));
	g_type_class_add_private(klass, sizeof(JingleRtpPrivate));
}

static void
jingle_rtp_init (JingleRtp *rtp)
{
	rtp->priv = JINGLE_RTP_GET_PRIVATE(rtp);
	memset(rtp->priv, 0, sizeof(rtp->priv));
}

static void
jingle_rtp_finalize (GObject *rtp)
{
	JingleRtpPrivate *priv = JINGLE_RTP_GET_PRIVATE(rtp);
	purple_debug_info("jingle-rtp","jingle_rtp_finalize\n");

	g_free(priv->media_type);
}

static void
jingle_rtp_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	JingleRtp *rtp;
	g_return_if_fail(JINGLE_IS_RTP(object));

	rtp = JINGLE_RTP(object);

	switch (prop_id) {
		case PROP_MEDIA_TYPE:
			g_free(rtp->priv->media_type);
			rtp->priv->media_type = g_value_dup_string(value);
			break;
		default:	
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
jingle_rtp_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	JingleRtp *rtp;
	g_return_if_fail(JINGLE_IS_RTP(object));
	
	rtp = JINGLE_RTP(object);

	switch (prop_id) {
		case PROP_MEDIA_TYPE:
			g_value_set_string(value, rtp->priv->media_type);
			break;
		default:	
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);	
			break;
	}
}

static gboolean
jingle_rtp_ready_to_initiate(JingleSession *session, PurpleMedia *media)
{
	if (jingle_session_is_initiator(session)) {
		GList *iter = jingle_session_get_contents(session);
		for (; iter; iter = g_list_next(iter)) {
			JingleContent *content = iter->data;
			gchar *name = jingle_content_get_name(content);
			if (!JINGLE_IS_RTP(content)
					|| JINGLE_RTP_GET_PRIVATE(content)->codecs_ready == FALSE
					|| JINGLE_RTP_GET_PRIVATE(content)->candidates_ready == FALSE) {
				g_free(name);
				return FALSE;
			}
			g_free(name);
		}
		return TRUE;
	}
	return FALSE;
}

gchar *
jingle_rtp_get_media_type(JingleContent *content)
{
	gchar *media_type;
	g_object_get(content, "media-type", &media_type, NULL);
	return media_type;
}

static PurpleMedia *
jingle_rtp_get_media(JingleSession *session)
{
	JabberStream *js = jingle_session_get_js(session);
	gchar *sid = jingle_session_get_sid(session);

	PurpleMedia *media = (PurpleMedia *) (js->medias) ?
			  g_hash_table_lookup(js->medias, sid) : NULL;
	g_free(sid);

	return media;
}

static JingleTransport *
jingle_rtp_candidates_to_transport(JingleSession *session, GType type, guint generation, GList *candidates)
{
	if (type == JINGLE_TYPE_RAWUDP) {
		gchar *id = jabber_get_next_id(jingle_session_get_js(session));
		JingleTransport *transport = jingle_transport_create(JINGLE_TRANSPORT_RAWUDP);
		JingleRawUdpCandidate *rawudp_candidate;
		for (; candidates; candidates = g_list_next(candidates)) {
			FsCandidate *candidate = candidates->data;
			id = jabber_get_next_id(jingle_session_get_js(session));
			rawudp_candidate = jingle_rawudp_candidate_new(id,
					generation, candidate->component_id,
					candidate->ip, candidate->port);
			jingle_rawudp_add_local_candidate(JINGLE_RAWUDP(transport), rawudp_candidate);
		}
		g_free(id);
		return transport;
#if 0
	} else if (type == JINGLE_TYPE_ICEUDP) {
		return NULL;
#endif
	} else {
		return NULL;
	}
}

static GList *
jingle_rtp_transport_to_candidates(JingleTransport *transport)
{
	const gchar *type = jingle_transport_get_transport_type(transport);
	GList *ret = NULL;
	if (!strcmp(type, JINGLE_TRANSPORT_RAWUDP)) {
		GList *candidates = jingle_rawudp_get_remote_candidates(JINGLE_RAWUDP(transport));

		for (; candidates; candidates = g_list_delete_link(candidates, candidates)) {
			JingleRawUdpCandidate *candidate = candidates->data;
			ret = g_list_append(ret, fs_candidate_new("", candidate->component,
					FS_CANDIDATE_TYPE_SRFLX, FS_NETWORK_PROTOCOL_UDP,
					candidate->ip, candidate->port));
		}

		return ret;
#if 0
	} else if (type == JINGLE_TRANSPORT_ICEUDP) {
		return NULL;
#endif
	} else {
		return NULL;
	}
}

static void
jingle_rtp_accept_cb(PurpleMedia *media, JingleSession *session)
{
	jabber_iq_send(jingle_session_to_packet(session, JINGLE_TRANSPORT_INFO));
	jabber_iq_send(jingle_session_to_packet(session, JINGLE_SESSION_ACCEPT));
}

static void
jingle_rtp_reject_cb(PurpleMedia *media, JingleSession *session)
{
	jabber_iq_send(jingle_session_to_packet(session, JINGLE_SESSION_TERMINATE));
	g_object_unref(session);
}

static void
jingle_rtp_hangup_cb(PurpleMedia *media, JingleSession *session)
{
	jabber_iq_send(jingle_session_to_packet(session, JINGLE_SESSION_TERMINATE));
	g_object_unref(session);
}

static void
jingle_rtp_new_candidate_cb(PurpleMedia *media, gchar *sid, gchar *name, FsCandidate *candidate, JingleSession *session)
{
	purple_debug_info("jingle-rtp", "jingle_rtp_new_candidate_cb\n");
}

static void
jingle_rtp_candidates_prepared_cb(PurpleMedia *media, gchar *sid, gchar *name, JingleSession *session)
{
	JingleContent *content = jingle_session_find_content(session, sid, "initiator");
	GList *candidates = purple_media_get_local_candidates(media, sid, name);
	JingleTransport *transport =
			JINGLE_TRANSPORT(jingle_rtp_candidates_to_transport(
			session, JINGLE_TYPE_RAWUDP, 0, candidates));
	g_list_free(candidates);

	JINGLE_RTP_GET_PRIVATE(content)->candidates_ready = TRUE;

	jingle_content_set_pending_transport(content, transport);
	jingle_content_accept_transport(content);

	if (jingle_rtp_ready_to_initiate(session, media))
		jabber_iq_send(jingle_session_to_packet(session, JINGLE_SESSION_INITIATE));
}

static void
jingle_rtp_candidate_pair_established_cb(PurpleMedia *media, FsCandidate *local_candidate, FsCandidate *remote_candidate, JingleSession *session)
{

}

static void
jingle_rtp_codecs_ready_cb(PurpleMedia *media, gchar *sid, JingleSession *session)
{
	JingleContent *content = jingle_session_find_content(session, sid, "initiator");

	if (content == NULL)
		content = jingle_session_find_content(session, sid, "responder");

	if (JINGLE_RTP_GET_PRIVATE(content)->codecs_ready == FALSE) {
		JINGLE_RTP_GET_PRIVATE(content)->codecs_ready =
				purple_media_codecs_ready(media, sid);

		if (jingle_rtp_ready_to_initiate(session, media))
			jabber_iq_send(jingle_session_to_packet(session, JINGLE_SESSION_INITIATE));
	}
}

static PurpleMedia *
jingle_rtp_create_media(JingleContent *content)
{
	JingleSession *session = jingle_content_get_session(content);
	JabberStream *js = jingle_session_get_js(session);
	gchar *remote_jid = jingle_session_get_remote_jid(session);
	gchar *sid = jingle_session_get_sid(session);

	PurpleMedia *media = purple_media_manager_create_media(purple_media_manager_get(), 
						  js->gc, "fsrtpconference", remote_jid);
	g_free(remote_jid);

	if (!media) {
		purple_debug_error("jingle-rtp", "Couldn't create media session\n");
		return NULL;
	}

	/* insert it into the hash table */
	if (!js->medias) {
		purple_debug_info("jingle-rtp", "Creating hash table for media\n");
		js->medias = g_hash_table_new(g_str_hash, g_str_equal);
	}
	purple_debug_info("jingle-rtp", "inserting media with sid: %s into table\n", sid);
	g_hash_table_insert(js->medias, sid, media);

	/* connect callbacks */
	g_signal_connect(G_OBJECT(media), "accepted",
				 G_CALLBACK(jingle_rtp_accept_cb), session);
	g_signal_connect(G_OBJECT(media), "reject",
				 G_CALLBACK(jingle_rtp_reject_cb), session);
	g_signal_connect(G_OBJECT(media), "hangup",
				 G_CALLBACK(jingle_rtp_hangup_cb), session);
	g_signal_connect(G_OBJECT(media), "new-candidate",
				 G_CALLBACK(jingle_rtp_new_candidate_cb), session);
	g_signal_connect(G_OBJECT(media), "candidates-prepared",
				 G_CALLBACK(jingle_rtp_candidates_prepared_cb), session);
	g_signal_connect(G_OBJECT(media), "candidate-pair",
				 G_CALLBACK(jingle_rtp_candidate_pair_established_cb), session);
	g_signal_connect(G_OBJECT(media), "codecs-ready",
				 G_CALLBACK(jingle_rtp_codecs_ready_cb), session);

	g_object_unref(session);
	return media;
}

static gboolean
jingle_rtp_init_media(JingleContent *content)
{
	JingleSession *session = jingle_content_get_session(content);
	PurpleMedia *media = jingle_rtp_get_media(session);
	gchar *media_type;
	gchar *remote_jid;
	gchar *senders;
	gchar *name;
	const gchar *transmitter;
	FsMediaType type;
	FsStreamDirection direction;
	JingleTransport *transport;

	/* maybe this create ought to just be in initiate and handle initiate */
	if (media == NULL)
		media = jingle_rtp_create_media(content);

	if (media == NULL)
		return FALSE;

	name = jingle_content_get_name(content);
	media_type = jingle_rtp_get_media_type(content);
	remote_jid = jingle_session_get_remote_jid(session);
	senders = jingle_content_get_senders(content);
	transport = jingle_content_get_transport(content);

	if (JINGLE_IS_RAWUDP(transport))
		transmitter = "rawudp";
	else
		transmitter = "notransmitter";

	if (!strcmp(media_type, "audio"))
		type = FS_MEDIA_TYPE_AUDIO;
	else
		type = FS_MEDIA_TYPE_VIDEO;

	if (!strcmp(senders, "both"))
		direction = FS_DIRECTION_BOTH;
	else if (!strcmp(senders, "initiator")
			&& jingle_session_is_initiator(session))
		direction = FS_DIRECTION_SEND;
	else
		direction = FS_DIRECTION_RECV;

	purple_media_add_stream(media, name, remote_jid,
			purple_media_from_fs(type, direction),
			transmitter, 0, NULL);


	g_free(name);
	g_free(media_type);
	g_free(remote_jid);
	g_free(senders);
	g_object_unref(session);

	return TRUE;
}

static GList *
jingle_rtp_parse_codecs(xmlnode *description)
{
	GList *codecs = NULL;
	xmlnode *codec_element = NULL;
	const char *encoding_name,*id, *clock_rate;
	FsCodec *codec;
	const gchar *media = xmlnode_get_attrib(description, "media");
	FsMediaType type = !strcmp(media, "video") ? FS_MEDIA_TYPE_VIDEO :
			!strcmp(media, "audio") ? FS_MEDIA_TYPE_AUDIO : 0;

	for (codec_element = xmlnode_get_child(description, "payload-type") ;
		 codec_element ;
		 codec_element = xmlnode_get_next_twin(codec_element)) {
		xmlnode *param;
		gchar *codec_str;
		encoding_name = xmlnode_get_attrib(codec_element, "name");

		id = xmlnode_get_attrib(codec_element, "id");
		clock_rate = xmlnode_get_attrib(codec_element, "clockrate");

		codec = fs_codec_new(atoi(id), encoding_name, 
				     type, 
				     clock_rate ? atoi(clock_rate) : 0);

		for (param = xmlnode_get_child(codec_element, "parameter");
				param; param = xmlnode_get_next_twin(param)) {
			fs_codec_add_optional_parameter(codec,
					xmlnode_get_attrib(param, "name"),
					xmlnode_get_attrib(param, "value"));
		}

		codec_str = fs_codec_to_string(codec);
		purple_debug_info("jingle-rtp", "received codec: %s\n", codec_str);
		g_free(codec_str);

		codecs = g_list_append(codecs, codec);
	}
	return codecs;
}

static JingleContent *
jingle_rtp_parse_internal(xmlnode *rtp)
{
	JingleContent *content = parent_class->parse(rtp);
	xmlnode *description = xmlnode_get_child(rtp, "description");
	const gchar *media_type = xmlnode_get_attrib(description, "media");
	purple_debug_info("jingle-rtp", "rtp parse\n");
	g_object_set(content, "media-type", media_type, NULL);
	return content;
}

static void
jingle_rtp_add_payloads(xmlnode *description, GList *codecs)
{
	for (; codecs ; codecs = codecs->next) {
		FsCodec *codec = (FsCodec*)codecs->data;
		GList *iter = codec->optional_params;
		char id[8], clockrate[10], channels[10];
		gchar *codec_str;
		xmlnode *payload = xmlnode_new_child(description, "payload-type");
		
		g_snprintf(id, sizeof(id), "%d", codec->id);
		g_snprintf(clockrate, sizeof(clockrate), "%d", codec->clock_rate);
		g_snprintf(channels, sizeof(channels), "%d", codec->channels);
		
		xmlnode_set_attrib(payload, "name", codec->encoding_name);
		xmlnode_set_attrib(payload, "id", id);
		xmlnode_set_attrib(payload, "clockrate", clockrate);
		xmlnode_set_attrib(payload, "channels", channels);

		for (; iter; iter = g_list_next(iter)) {
			FsCodecParameter *fsparam = iter->data;
			xmlnode *param = xmlnode_new_child(payload, "parameter");
			xmlnode_set_attrib(param, "name", fsparam->name);
			xmlnode_set_attrib(param, "value", fsparam->value);
		}

		codec_str = fs_codec_to_string(codec);
		purple_debug_info("jingle", "adding codec: %s\n", codec_str);
		g_free(codec_str);
	}
}

static xmlnode *
jingle_rtp_to_xml_internal(JingleContent *rtp, xmlnode *content, JingleActionType action)
{
	xmlnode *node = parent_class->to_xml(rtp, content, action);
	xmlnode *description = xmlnode_get_child(node, "description");
	if (description != NULL) {
		JingleSession *session = jingle_content_get_session(rtp);
		PurpleMedia *media = jingle_rtp_get_media(session);
		gchar *media_type = jingle_rtp_get_media_type(rtp);
		gchar *name = jingle_content_get_name(rtp);
		GList *codecs = purple_media_get_local_codecs(media, name);

		xmlnode_set_attrib(description, "media", media_type);

		g_free(media_type);
		g_free(name);
		g_object_unref(session);

		jingle_rtp_add_payloads(description, codecs);
	}
	return node;
}

static void
jingle_rtp_handle_action_internal(JingleContent *content, xmlnode *xmlcontent, JingleActionType action)
{
	switch (action) {
		case JINGLE_SESSION_ACCEPT: {
			JingleSession *session = jingle_content_get_session(content);
			xmlnode *description = xmlnode_get_child(xmlcontent, "description");
			GList *codecs = jingle_rtp_parse_codecs(description);

			purple_media_set_remote_codecs(jingle_rtp_get_media(session),
					jingle_content_get_name(content),
					jingle_session_get_remote_jid(session), codecs);

			/* This needs to be for the entire session, not a single content */
			/* very hacky */
			if (xmlnode_get_next_twin(xmlcontent) == NULL)
				purple_media_got_accept(jingle_rtp_get_media(session));

			g_object_unref(session);
			break;
		}
		case JINGLE_SESSION_INITIATE: {
			JingleSession *session = jingle_content_get_session(content);
			JingleTransport *transport = jingle_transport_parse(
					xmlnode_get_child(xmlcontent, "transport"));
			xmlnode *description = xmlnode_get_child(xmlcontent, "description");
			GList *candidates = jingle_rtp_transport_to_candidates(transport);
			GList *codecs = jingle_rtp_parse_codecs(description);

			if (jingle_rtp_init_media(content) == FALSE) {
				/* XXX: send error */
				jabber_iq_send(jingle_session_to_packet(session,
						 JINGLE_SESSION_TERMINATE));
				g_object_unref(session);
				break;
			}

			purple_media_set_remote_codecs(jingle_rtp_get_media(session),
					jingle_content_get_name(content),
					jingle_session_get_remote_jid(session), codecs);

			purple_media_add_remote_candidates(jingle_rtp_get_media(session),
					jingle_content_get_name(content),
					jingle_session_get_remote_jid(session),
					candidates);

			/* very hacky */
			if (xmlnode_get_next_twin(xmlcontent) == NULL)
				purple_media_ready(jingle_rtp_get_media(session));

			g_object_unref(session);
			break;
		}
		case JINGLE_SESSION_TERMINATE: {
			JingleSession *session = jingle_content_get_session(content);
			purple_media_got_hangup(jingle_rtp_get_media(session));
			g_object_unref(session);
			break;
		}
		case JINGLE_TRANSPORT_INFO: {
			JingleSession *session = jingle_content_get_session(content);
			JingleTransport *transport = jingle_transport_parse(
					xmlnode_get_child(xmlcontent, "transport"));
			GList *candidates = jingle_rtp_transport_to_candidates(transport);

			purple_media_add_remote_candidates(jingle_rtp_get_media(session),
					jingle_content_get_name(content),
					jingle_session_get_remote_jid(session),
					candidates);
			g_object_unref(session);
			break;
		}
		default:
			break;
	}
}

PurpleMedia *
jingle_rtp_initiate_media(JabberStream *js, const gchar *who, 
		      PurpleMediaSessionType type)
{
	/* create content negotiation */
	JingleSession *session;
	JingleContent *content;
	JingleTransport *transport;
	JabberBuddy *jb;
	JabberBuddyResource *jbr;
	
	gchar *jid = NULL, *me = NULL, *sid = NULL;

	/* construct JID to send to */
	jb = jabber_buddy_find(js, who, FALSE);
	if (!jb) {
		purple_debug_error("jingle-rtp", "Could not find Jabber buddy\n");
		return NULL;
	}
	jbr = jabber_buddy_find_resource(jb, NULL);
	if (!jbr) {
		purple_debug_error("jingle-rtp", "Could not find buddy's resource\n");
	}

	if ((strchr(who, '/') == NULL) && jbr && (jbr->name != NULL)) {
		jid = g_strdup_printf("%s/%s", who, jbr->name);
	} else {
		jid = g_strdup(who);
	}
	
	/* set ourselves as initiator */
	me = g_strdup_printf("%s@%s/%s", js->user->node, js->user->domain, js->user->resource);

	sid = jabber_get_next_id(js);
	session = jingle_session_create(js, sid, me, jid, TRUE);
	g_free(sid);


	if (type & PURPLE_MEDIA_AUDIO) {
		transport = jingle_transport_create(JINGLE_TRANSPORT_RAWUDP);
		content = jingle_content_create(JINGLE_APP_RTP, "initiator",
				"session", "audio-session", "both", transport);
		jingle_session_add_content(session, content);
		JINGLE_RTP(content)->priv->media_type = g_strdup("audio");
		jingle_rtp_init_media(content);
	}
	if (type & PURPLE_MEDIA_VIDEO) {
		transport = jingle_transport_create(JINGLE_TRANSPORT_RAWUDP);
		content = jingle_content_create(JINGLE_APP_RTP, "initiator",
				"session", "video-session", "both", transport);
		jingle_session_add_content(session, content);
		JINGLE_RTP(content)->priv->media_type = g_strdup("video");
		jingle_rtp_init_media(content);
	}

	purple_media_ready(jingle_rtp_get_media(session));
	purple_media_wait(jingle_rtp_get_media(session));

	g_free(jid);
	g_free(me);

	return NULL;
}

void
jingle_rtp_terminate_session(JabberStream *js, const gchar *who)
{
	JingleSession *session;
/* XXX: This may cause file transfers and xml sessions to stop as well */
	session = jingle_session_find_by_jid(js, who);

	if (session) {
		PurpleMedia *media = jingle_rtp_get_media(session);
		if (media) {
			purple_debug_info("jingle-rtp", "hanging up media\n");
			purple_media_hangup(media);
		}
	}
}
