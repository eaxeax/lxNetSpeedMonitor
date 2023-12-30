#include <lxpanel/plugin.h>
#include <stdio.h>
#include <string.h>
typedef struct
{
    GtkWidget* label_tx;  //tx
    GtkWidget* label_rx;  //rx
    char data[100];    //reusable string buffer
    guint timer;        //Timer

    char* iface;

    //switch to size_t? ...
    guint last_rx;
    guint last_tx;

    int rateunit;
    gboolean preferbitpersec;

    config_setting_t* settings;

}netspeedmon;

const char* catlookup="cat /sys/class/net/%s/statistics/%s";

const char* rateunits[]=
{
    //byte,bit
    "B/s","b/s",
    "KB/s","Kb/s",
    "MB/s","Mb/s",
};

void runScriptAndGetOutput(char *script, char *out, size_t size) {
    FILE* in = popen(script, "r");
    if (!in) {
        perror("Failed to run script");
        exit(1);
    }
    
    fgets(upDownText, size-1, in);
    
    pclose(in);
}


#define ARY_SZ(arr) sizeof(arr) / sizeof(arr[0])
static gboolean update_cmd(netspeedmon* plugin)
{
    //deltas
    double dlt_tx_human=0.0, dlt_rx_human=0.0;
    guint dlt_tx=0, dlt_rx=0;
    int notfound=0;

    snprintf(plugin->data,sizeof (plugin->data),catlookup,plugin->iface,"rx_bytes");
    FILE* fp_rx = popen(plugin->data, "r");

    snprintf(plugin->data,sizeof (plugin->data),catlookup,plugin->iface,"tx_bytes");
    FILE* fp_tx = popen(plugin->data, "r");

    if(fp_rx && fp_tx)
    {
        //read tx
        memset(plugin->data, 0, sizeof(plugin->data));
        while(fgets(plugin->data, sizeof(plugin->data), fp_tx))
        {
            continue;
        }

        if(!pclose(fp_tx))
        {
            guint tx = 0;
            sscanf(plugin->data, "%u", &tx);
            dlt_tx = tx - plugin->last_tx;
            plugin->last_tx = tx;
        }else
        {
            notfound=1;
        }

        //read rx
        memset(plugin->data, 0, sizeof(plugin->data));
        while(fgets(plugin->data, sizeof(plugin->data), fp_rx))
        {
            continue;
        }

        if(!pclose(fp_rx))
        {
            guint rx = 0;
            sscanf(plugin->data, "%u", &rx);
            dlt_rx = rx - plugin->last_rx;
            plugin->last_rx = rx;
        }

        dlt_tx_human=(double)dlt_tx;
        dlt_rx_human=(double)dlt_rx;

        //convert
        for (int i=0; i<plugin->rateunit; i++) {
            dlt_tx_human=(double)dlt_tx_human/1024.0;
            dlt_rx_human=(double)dlt_rx_human/1024.0;
        }

        if(plugin->preferbitpersec)
        {
            dlt_tx_human*=8.0;
            dlt_rx_human*=8.0;
        }
    }

    char ubuffer[200]{}, dbuffer[200]{};

    runScriptAndGetOutput("script1.sh", ubuffer, sizeof(ubuffer));
    runScriptAndGetOutput("script2.sh", dbuffer, sizeof(dbuffer));

    //clear then do final print
    memset(plugin->data, 0, sizeof(plugin->data));
    if (notfound) {
        snprintf(plugin->data,sizeof(plugin->data),
                 "U : %s",
                 ubuffer);
        gtk_label_set_text((GtkLabel*)plugin->label_tx, plugin->data);
        snprintf(plugin->data,sizeof(plugin->data),
                 "D : %s",
                 dbuffer);
        gtk_label_set_text((GtkLabel*)plugin->label_rx, plugin->data);
    }else
    {
        snprintf(plugin->data,sizeof(plugin->data),
                 "U : %.1f %s",
                 dlt_tx_human,rateunits[2*plugin->rateunit+plugin->preferbitpersec]);
        gtk_label_set_text((GtkLabel*)plugin->label_tx, plugin->data);
        snprintf(plugin->data,sizeof(plugin->data),
                 "D : %.1f %s",
                 dlt_rx_human,rateunits[2*plugin->rateunit+plugin->preferbitpersec]);
        gtk_label_set_text((GtkLabel*)plugin->label_rx, plugin->data);
    }
    return TRUE;
}

void netspeed_delete(gpointer user_data)
{
    netspeedmon* nu= (netspeedmon*)user_data;
    g_source_remove(nu->timer);
    g_free(nu->iface);
    g_free(nu);
}
GtkWidget* netspeed_new(LXPanel* panel, config_setting_t* settings)
{
    //Alloc struct
    netspeedmon* plugin = g_new0(netspeedmon, 1);
    memset(plugin, 0, sizeof(netspeedmon));
    plugin->settings=settings;

    //Update count
    plugin->timer = g_timeout_add_seconds(1,(GSourceFunc)update_cmd, plugin);

    //labels
    GtkWidget* label = NULL,* vbox = NULL,* evbox = NULL;

    vbox = gtk_vbox_new(TRUE, 1);

    label = gtk_label_new("U : ...");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0f, 0.5f);
    plugin->label_tx = label;
    gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);

    label = gtk_label_new("D : ...");
    gtk_misc_set_alignment(GTK_MISC(label), 0.0f, 0.5f);
    plugin->label_rx = label;
    gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);

    const char* tmp;
    if (!config_setting_lookup_string(settings, "iface", &tmp))
        tmp = "wlan0";
    plugin->iface=g_strdup(tmp);

    plugin->rateunit=1;
    config_setting_lookup_int(settings,"rateunit",&plugin->rateunit);
    config_setting_lookup_int(settings,"preferbps",&plugin->preferbitpersec);

    evbox = gtk_event_box_new();
    //Set width
    gtk_container_set_border_width(GTK_CONTAINER(evbox), 1);

    //add vbox
    gtk_container_add(GTK_CONTAINER(evbox), vbox);

    gtk_widget_set_size_request(evbox, 100, 25);
    //Show all
    gtk_widget_show_all(evbox);

    //Bind struct
    lxpanel_plugin_set_data(evbox, plugin, netspeed_delete);

    //done!
    return evbox;
}

gboolean apply_config(gpointer user_data)
{
    GtkWidget *p = user_data;
    netspeedmon* nu = lxpanel_plugin_get_data(p);
    config_group_set_string(nu->settings, "iface", nu->iface);

    int ru= nu->rateunit;
    //guard errors
    if (nu->rateunit > ARY_SZ(rateunits)/2-1 || nu->rateunit<0) {
        //did you mean KB/s?
        ru=1;
        nu->rateunit=ru;
    }
    config_group_set_int(nu->settings, "rateunit", ru);
    config_group_set_int(nu->settings, "preferbps", nu->preferbitpersec);
    return FALSE;
}

GtkWidget* netspeed_config(LXPanel* panel, GtkWidget* p)
{
    GtkWidget* dlg;
    netspeedmon* nu=lxpanel_plugin_get_data(p);
    dlg=lxpanel_generic_config_dlg("NetSpeedMonitor Configuration",
                                   panel,apply_config,p,
                                   "Interface",&nu->iface,CONF_TYPE_STR,
                                   "Rate unit (None=0, K=1, M=2)",&nu->rateunit,CONF_TYPE_INT,
                                   "Prefer bits per sec",&nu->preferbitpersec,CONF_TYPE_BOOL,
                                   NULL
                                   );
    return dlg;
}

FM_DEFINE_MODULE(lxpanel_gtk, netspeedmonitor);

//Descriptor
LXPanelPluginInit fm_module_init_lxpanel_gtk =
{
    .name = "NetSpeedMonitor",
    .description = "Show network speed",
    .new_instance = netspeed_new,
    .config = netspeed_config
};
