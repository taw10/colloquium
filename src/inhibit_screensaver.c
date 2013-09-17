/*
 * inhibit_screensaver.h
 *
 * Copyright © 2013 Thomas White <taw@bitwiz.org.uk>
 * Copyright © 2009-2012 Rémi Denis-Courmont
 * Copyright © 2007-2012 Rafaël Carré
 *
 * This file is part of Colloquium.
 *
 * Colloquium is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * >>> This file is based on dbus.c from VLC. The original licence is below. <<<
 *
 */

/*****************************************************************************
 * dbus.c: power management inhibition using D-Bus
 *****************************************************************************
 * Copyright © 2009-2012 Rémi Denis-Courmont
 * Copyright © 2007-2012 Rafaël Carré
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/*
 * Based on freedesktop Power Management Specification version 0.2
 * http://people.freedesktop.org/~hughsient/temp/power-management-spec-0.2.html
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <dbus/dbus.h>


enum inhibit_api
{
	FDO_SS, /**< KDE >= 4 and GNOME >= 3.6 */
	FDO_PM, /**< KDE and GNOME <= 2.26 */
	MATE,   /**< >= 1.0 */
	GNOME,  /**< GNOME 2.26..3.4 */
};

#define MAX_API (GNOME+1)


/* Currently, all services have identical service and interface names. */
static const char dbus_service[][40] =
{
	[FDO_SS] = "org.freedesktop.ScreenSaver",
	[FDO_PM] = "org.freedesktop.PowerManagement.Inhibit",
	[MATE]   = "org.mate.SessionManager",
	[GNOME]  = "org.gnome.SessionManager",
};


static const char dbus_path[][33] =
{
	[FDO_SS] = "/ScreenSaver",
	[FDO_PM] = "/org/freedesktop/PowerManagement",
	[MATE]   = "/org/mate/SessionManager",
	[GNOME]  = "/org/gnome/SessionManager",
};


static const char dbus_method_uninhibit[][10] =
{
	[FDO_SS] = "UnInhibit",
	[FDO_PM] = "UnInhibit",
	[MATE]   = "Uninhibit",
	[GNOME]  = "Uninhibit",
};


struct inhibit_sys
{
	DBusConnection *conn;
	DBusPendingCall *pending;
	dbus_uint32_t cookie;
	enum inhibit_api api;
};


void do_inhibit(struct inhibit_sys *sys, int flags)
{
	enum inhibit_api type = sys->api;

	/* Receive reply from previous request, possibly hours later ;-) */
	if ( sys->pending != NULL ) {

		DBusMessage *reply;

		/* NOTE: Unfortunately, the pending reply cannot simply be
		 * cancelled.   Otherwise, the cookie would be lost and
		 * inhibition would remain on (until complete disconnection from
		 * the bus). */
		dbus_pending_call_block(sys->pending);
		reply = dbus_pending_call_steal_reply(sys->pending);
		dbus_pending_call_unref(sys->pending);
		sys->pending = NULL;

		if ( reply != NULL ) {
			if ( !dbus_message_get_args(reply, NULL,
		                                    DBUS_TYPE_UINT32,
			                            &sys->cookie,
		                                    DBUS_TYPE_INVALID))
			{
				sys->cookie = 0;
			}
			dbus_message_unref(reply);
		}
		fprintf(stderr, "got cookie %i", (int)sys->cookie);

	}

	/* FIXME: This check is incorrect if flags change from one non-zero value
	 * to another one. But the D-Bus API cannot currently inhibit suspend
	 * independently from the screensaver. */
	if ( !sys->cookie == !flags ) return; /* nothing to do */

	/* Send request */
	const char *method = flags ? "Inhibit" : dbus_method_uninhibit[type];
	dbus_bool_t ret;

	DBusMessage *msg = dbus_message_new_method_call(dbus_service[type],
	                                                dbus_path[type],
	                                                dbus_service[type],
	                                                method);
	if ( msg == NULL ) return;

    if (flags)
    {
        const char *app = PACKAGE;
        const char *reason = "Presentation in progress";

        assert(sys->cookie == 0);

        switch (type)
        {
            case MATE:
            case GNOME:
            {
                dbus_uint32_t xid = 0; // FIXME ?
                dbus_uint32_t gflags = 0xC;

                ret = dbus_message_append_args(msg, DBUS_TYPE_STRING, &app,
                                                    DBUS_TYPE_UINT32, &xid,
                                                    DBUS_TYPE_STRING, &reason,
                                                    DBUS_TYPE_UINT32, &gflags,
                                                    DBUS_TYPE_INVALID);
                break;
            }
            default:
                ret = dbus_message_append_args(msg, DBUS_TYPE_STRING, &app,
                                                    DBUS_TYPE_STRING, &reason,
                                                    DBUS_TYPE_INVALID);
                break;
        }

        if (!ret
        || !dbus_connection_send_with_reply(sys->conn, msg, &sys->pending, -1))
            sys->pending = NULL;
    }
    else
    {
        assert(sys->cookie != 0);
        if (!dbus_message_append_args(msg, DBUS_TYPE_UINT32, &sys->cookie,
                                           DBUS_TYPE_INVALID)
         || !dbus_connection_send (sys->conn, msg, NULL))
            sys->cookie = 0;
    }
    dbus_connection_flush(sys->conn);
    dbus_message_unref(msg);
}


void inhibit_cleanup(struct inhibit_sys *sys)
{
	if ( sys->pending != NULL ) {
		dbus_pending_call_cancel(sys->pending);
		dbus_pending_call_unref(sys->pending);
	}

	dbus_connection_close(sys->conn);
	dbus_connection_unref(sys->conn);
	free(sys);
}


struct inhibit_sys *inhibit_prepare()
{
	struct inhibit_sys *sys;
	DBusError err;
	int i;

	sys = malloc(sizeof(struct inhibit_sys));
	if ( sys == NULL ) {
		fprintf(stderr, "Failed to allocate inhibit.\n");
		return NULL;
	}

	dbus_error_init(&err);

	sys->conn = dbus_bus_get_private(DBUS_BUS_SESSION, &err);
	if ( sys->conn == NULL ) {
		fprintf(stderr, "cannot connect to session bus: %s",
		        err.message);
		dbus_error_free(&err);
		free(sys);
		return NULL;
	}

	sys->pending = NULL;
	sys->cookie = 0;

	for ( i=0; i<MAX_API; i++ ) {
		if ( dbus_bus_name_has_owner(sys->conn, dbus_service[i], NULL) )
		{
			fprintf(stderr, "found service %s", dbus_service[i]);
			sys->api = i;
			return sys;
		}

		fprintf(stderr, "cannot find service %s", dbus_service[i]);
	}

	inhibit_cleanup(sys);
	return NULL;
}
