#include <lxpanel/plugin.h>

#include <stdio.h>
#include <string.h>
typedef struct
{
    GtkWidget* label;  //Plugin label
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

    //clear then do final print
    memset(plugin->data, 0, sizeof(plugin->data));
    if (notfound) {
        snprintf(plugin->data,sizeof(plugin->data),
                 "U : %s %s\nD : %s %s",
                 "...",rateunits[2*plugin->rateunit+plugin->preferbitpersec],
                 "...",rateunits[2*plugin->rateunit+plugin->preferbitpersec]);
    }else
    {
        snprintf(plugin->data,sizeof(plugin->data),
                 "U : %.1f %s\nD : %.1f %s",
                 dlt_tx_human,rateunits[2*plugin->rateunit+plugin->preferbitpersec],
                 dlt_rx_human,rateunits[2*plugin->rateunit+plugin->preferbitpersec]);
    }

    gtk_label_set_text((GtkLabel*)plugin->label, plugin->data);

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

    //label
    GtkWidget* pLabel = gtk_label_new("...");

    //Show
    gtk_widget_show(pLabel);

    //Container
    GtkWidget* p = gtk_event_box_new();

     // our widget doesn't have a window...
     // it is usually illegal to call gtk_widget_set_has_window() from application but for GtkEventBox it doesn't hurt
     //gtk_widget_set_has_window(p, FALSE);

    //Set width
    gtk_container_set_border_width(GTK_CONTAINER(p), 1);

    // add the label to the container
    gtk_container_add(GTK_CONTAINER(p), pLabel);

    // set the size we want
    gtk_widget_set_size_request(p, 100, 25);

    const char* tmp;
    if (!config_setting_lookup_string(settings, "iface", &tmp))
        tmp = "eth0";
    plugin->iface=g_strdup(tmp);

    plugin->rateunit=1;
    config_setting_lookup_int(settings,"rateunit",&plugin->rateunit);
    config_setting_lookup_int(settings,"preferbps",&plugin->preferbitpersec);

    //Init
    plugin->label = pLabel;

    //Bind struct
    lxpanel_plugin_set_data(p, plugin, netspeed_delete);

    //done!
    return p;
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

FM_DEFINE_MODULE(lxpanel_gtk, NetSpeedMonitor);

//Descriptor
LXPanelPluginInit fm_module_init_lxpanel_gtk =
{
    .name = "NetSpeedMonitor",
    .description = "Show network speed",
    .new_instance = netspeed_new,
    .config=netspeed_config
};
