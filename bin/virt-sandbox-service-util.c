/*
 * virt-sandbox-service-util.c: libvirt sandbox service util command
 *
 * Copyright (C) 2012-2013 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Author: Daniel J Walsh <dwalsh@redhat.com>
 * Author: Daniel P. Berrange <berrange@redhat.com>
 */

#include <config.h>

#include <libvirt-sandbox/libvirt-sandbox.h>
#include <glib/gi18n.h>

#define STREQ(x,y) (strcmp(x,y) == 0)

static gboolean do_close(GVirSandboxConsole *con G_GNUC_UNUSED,
                         gboolean error G_GNUC_UNUSED,
                         gpointer opaque)
{
    GMainLoop *loop = opaque;
    g_main_loop_quit(loop);
    return FALSE;
}


static void libvirt_sandbox_version(void)
{
    g_print(_("%s version %s\n"), PACKAGE, VERSION);
    exit(EXIT_SUCCESS);
}


static int container_start( GVirSandboxContext *ctx, GMainLoop *loop ) {

    int ret = EXIT_FAILURE;
    GError *err = NULL;
    GVirSandboxConsole *con = NULL;

    if (!(gvir_sandbox_context_start(ctx, &err))) {
        g_printerr(_("Unable to start container: %s\n"),
                   err && err->message ? err->message : _("unknown"));
        return ret;
    }

    if (!(con = gvir_sandbox_context_get_log_console(ctx, &err)))  {
        g_printerr(_("Unable to get log console for container: %s\n"),
                   err && err->message ? err->message : _("unknown"));
        return ret;
    }

    g_signal_connect(con, "closed", (GCallback)do_close, loop);

    if (gvir_sandbox_console_attach_stderr(con, &err) < 0) {
        g_printerr(_("Unable to attach console to stderr in the container: %s\n"),
                   err && err->message ? err->message : _("unknown"));
        return ret;
    }

    g_main_loop_run(loop);

    return EXIT_SUCCESS;
}

static int container_attach( GVirSandboxContext *ctx,  GMainLoop *loop ) {

    GError *err = NULL;
    GVirSandboxConsole *con = NULL;
    int ret = EXIT_FAILURE;

    if (!(gvir_sandbox_context_attach(ctx, &err))) {
        g_printerr(_("Unable to attach to container: %s\n"),
                   err && err->message ? err->message : _("unknown"));
        return ret;
    }

    if (!(con = gvir_sandbox_context_get_shell_console(ctx, &err)))  {
        g_printerr(_("Unable to get shell console for container: %s\n"),
                   err && err->message ? err->message : _("unknown"));
        return ret;
    }

    g_signal_connect(con, "closed", (GCallback)do_close, loop);

    if (!(gvir_sandbox_console_attach_stdio(con, &err))) {
        g_printerr(_("Unable to attach to container: %s\n"),
                   err && err->message ? err->message : _("unknown"));
        return ret;
    }

    g_main_loop_run(loop);

    return EXIT_SUCCESS;
}

static int container_stop( GVirSandboxContext *ctx, GMainLoop *loop G_GNUC_UNUSED) {

    GError *err = NULL;

    if (!(gvir_sandbox_context_attach(ctx, &err))) {
        g_printerr(_("Unable to attach to container: %s\n"),
                   err && err->message ? err->message : _("unknown"));
        return EXIT_FAILURE;
    }

    if (!(gvir_sandbox_context_stop(ctx, &err))) {
        g_printerr(_("Unable to stop container: %s\n"),
                   err && err->message ? err->message : _("unknown"));
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

static int (*container_func)( GVirSandboxContext *ctx, GMainLoop *loop ) = NULL;

static gboolean libvirt_lxc_start(const gchar *option_name,
                                  const gchar *value,
                                  const gpointer *data,
                                  const GError **error)

{
    if (container_func) return FALSE;
    container_func = container_start;
    return TRUE;
}

static gboolean libvirt_lxc_stop(const gchar *option_name,
                                 const gchar *value,
                                 const gpointer *data,
                                 const GError **error)

{
    if (container_func) return FALSE;
    container_func = container_stop;
    return TRUE;
}

static gboolean libvirt_lxc_attach(const gchar *option_name,
                                   const gchar *value,
                                   const gpointer *data,
                                   const GError **error)

{
    if (container_func) return FALSE;
    container_func = container_attach;
    return TRUE;
}

int main(int argc, char **argv) {
    GMainLoop *loop = NULL;
    GVirSandboxConfig *config = NULL;
    GVirSandboxContextService *ctx = NULL;
    GError *err = NULL;
    GVirConnection *hv = NULL;
    int ret = EXIT_FAILURE;
    pid_t pid = 0;
    gchar *buf=NULL;
    gchar *uri = NULL;

    gchar **cmdargs = NULL;
    GOptionContext *context;
    GOptionEntry options[] = {
        { "version", 'V', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
          libvirt_sandbox_version, N_("Display version information"), NULL },
        { "start", 's', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
          libvirt_lxc_start, N_("Start a container"), NULL },
        { "stop", 'S', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
          libvirt_lxc_stop, N_("Stop a container"), NULL },
        { "attach", 'a', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
          libvirt_lxc_attach, N_("Attach to a container"), NULL },
        { "pid", 'p', 0, G_OPTION_ARG_INT, &pid,
          N_("Pid of process in container to which the command will run"), "PID"},
        { "connect", 'c', 0, G_OPTION_ARG_STRING, &uri,
          N_("Connect to hypervisor Default:'lxc:///'"), "URI"},
        { G_OPTION_REMAINING, '\0', 0, G_OPTION_ARG_STRING_ARRAY, &cmdargs,
          NULL, "CONTAINER_NAME" },
        { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, NULL }
    };
    const char *help_msg = N_("Run 'virt-sandbox-service-util --help' to see a full list of available command line options\n");

    setlocale(LC_ALL, "");
    bindtextdomain(PACKAGE, LOCALEDIR);
    bind_textdomain_codeset(PACKAGE, "UTF-8");
    textdomain(PACKAGE);

    if (!gvir_sandbox_init_check(&argc, &argv, &err))
        exit(EXIT_FAILURE);

    context = g_option_context_new (_("- Libvirt Sandbox Service"));
    g_option_context_add_main_entries (context, options, NULL);
    g_option_context_parse (context, &argc, &argv, &err);

    if (err) {
        g_printerr("%s\n%s\n",
                   err->message,
                   gettext(help_msg));
        goto cleanup;
    }

    if ( container_func == NULL ) {
        g_printerr(_("Invalid command: You must specify --start, --stop or --attach\n%s"),
                   gettext(help_msg));
        goto cleanup;
    }

    if (!cmdargs || !cmdargs[0] ) {
        g_printerr(_("Invalid command CONTAINER_NAME required: %s"),
                   gettext(help_msg));
        goto cleanup;
    }

    g_option_context_free(context);

    g_set_application_name(_("Libvirt Sandbox Service"));

    buf = g_strdup_printf("/etc/libvirt-sandbox/services/%s.sandbox", cmdargs[0]);
    if (!buf) {
        g_printerr(_("Out of Memory\n"));
        goto cleanup;
    }

    if (uri)
        hv = gvir_connection_new(uri);
    else
        hv = gvir_connection_new("lxc:///");

    if (!hv) {
        g_printerr(_("error opening connect to lxc:/// \n"));
        goto cleanup;
    }

    if (!gvir_connection_open(hv, NULL, &err)) {
        g_printerr(_("Unable to open connection: %s\n"),
                   err && err->message ? err->message : _("unknown"));
        goto cleanup;
    }

    if (!(config = gvir_sandbox_config_load_from_path(buf, &err))) {
        g_printerr(_("Unable to read config file %s: %s\n"), buf,
                   err && err->message ? err->message : _("unknown"));
        goto cleanup;
    }

    if (!(ctx = gvir_sandbox_context_service_new(hv, GVIR_SANDBOX_CONFIG_SERVICE(config)))) {
        g_printerr(_("Unable to create new context service: %s\n"),
                   err && err->message ? err->message : _("unknown"));
        goto cleanup;
    }

    loop = g_main_loop_new(g_main_context_default(), 1);
    ret = container_func(GVIR_SANDBOX_CONTEXT(ctx), loop);

cleanup:
    if (hv)
        gvir_connection_close(hv);

    free(buf);

    if (config)
        g_object_unref(config);

    exit(ret);
}
