/* Nettante - net configuration software for dyne:bolic GNU/Linux distribution
 * http://dynebolic.org
 *
 * Copyright (C) 2003-2006 Denis Rojo aka jaromil <jaromil@dyne.org>
 * 
 * This source code is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Public License as published 
 * by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 *
 * This source code is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * Please refer to the GNU Public License for more details.
 *
 * You should have received a copy of the GNU Public License along with
 * this source code; if not, write to:
 * Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * "Header: $"
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>

#include "callbacks.h"
#include "interface.h"
#include "support.h"

#define NONE 0
#define STATIC_IP 1
#define DHCP 2

static GtkWidget *win_staticip = NULL;
static GtkWidget *ip_address;
static GtkWidget *netmask;
static GtkWidget *gateway;
static GtkWidget *dns;
static GtkWidget *hostname;
static GtkWidget *toggle_staticip = NULL;
static GtkWidget *toggle_dhcp = NULL;
static GtkWidget *check_active = NULL;

struct eth_card {
  char desc[256];
  int conf;
  char ip[128];
  char netmask[128];
  char dns[128];
  char gw[128];
};

static struct eth_card eth[64]; /* i bet you don't have more than 64 ethernet cards */
static int eth_num = -1;
static int eth_sel = -1;

int render_settings(char *file) {
  FILE *fd;
  char tmp[512];
  time_t now;
  int c;
  
  umask(022);
  fd = fopen(file,"w");
  if(!fd) {
    fprintf(stderr,"ERROR: couldn't open %s for writing. abort operation.\n",file);
    return 0;
  }

  fputs("# network configuration file\n",fd);
  now = time(NULL);
  sprintf(tmp,"# generated on %s\n\n",ctime(&now)); fputs(tmp,fd);

  // hostname
  sprintf(tmp,"HOSTNAME=%s\n", gtk_entry_get_text((GtkEntry*)hostname));
  fputs(tmp,fd);
 
  for(c=0;c<=eth_num;c++) {
    // TODO: support multiple network devices
    sprintf(tmp,"# eth%u : %s\n",c,eth[c].desc); fputs(tmp,fd);

    sprintf(tmp,"NET_DEV=eth%u\n",c);  fputs(tmp,fd);

    if(eth[c].conf == DHCP) /* DHCP */

      fputs("NET_IP=dhcp\n",fd);

    else { /* STATIC IP */

      /* env variable for eth configuration */
      sprintf(tmp, "NET_IP=%s\n",   eth[c].ip);        fputs(tmp,fd);
      sprintf(tmp, "NET_GW=%s\n",   eth[c].gw);        fputs(tmp,fd);
      sprintf(tmp, "NET_MASK=%s\n", eth[c].netmask);   fputs(tmp,fd);
      sprintf(tmp, "NET_DNS=%s\n",  eth[c].dns);       fputs(tmp,fd);

    }

    fputs("# --\n\n",fd);
  }
  

  fclose(fd);
  return 1;
};

  

void fetch_staticip_settings(int card) {
  char tmp[256];
  char *env, *tok;

  if(card>eth_num || card<0) {
    fprintf(stderr,"illegal call: card %i doesn't exists\n",card);
    return;
  }

  sprintf(tmp,"dynebolic_net_iface_eth%u",card);
  env = getenv(tmp);
  if(!env) {
    /* card wasn't configured */
    memset(eth[card].ip,128*sizeof(char),0);
    strcpy(eth[card].netmask,"255.255.255.0");
    memset(eth[card].gw,128*sizeof(char),0);
    memset(eth[card].dns,128*sizeof(char),0);
    eth[card].conf = NONE;
  } else if (strncasecmp(env,"dhcp",4)==0) {
    /* DHCP */
    memset(eth[card].ip,128*sizeof(char),0);
    strcpy(eth[card].netmask,"255.255.255.0");
    memset(eth[card].gw,128*sizeof(char),0);
    memset(eth[card].dns,128*sizeof(char),0);
    eth[card].conf = DHCP;
  } else {
    /* STATIC_IP : env is in format "ip_address:netmask:gateway:dns" */
    tok = strtok(env,":");
    strncpy(eth[card].ip,tok,128);
    tok = strtok(NULL,":");
    strncpy(eth[card].netmask,tok,128);
    tok = strtok(NULL,":");
    strncpy(eth[card].gw,tok,128);
    tok = strtok(NULL,":");
    strncpy(eth[card].dns,tok,128);
    eth[card].conf = STATIC_IP;
  }

}

void
on_combo_eth_realize                   (GtkWidget       *widget,
                                        gpointer         user_data)
{
  GList *items = NULL;
  FILE *fd;
  char tmp[256], label[64][256];
  char *p, *pp;

  /* gathers description from /proc/pci and fills the gtk combo box */
  fd = fopen("/boot/pcilist","r");
  if(!fd) {
    fprintf(stderr,"can't run netconf: /boot/pcilist not found\n");
    fprintf(stderr,"run 'lspci > /boot/pcilist'\n");
    exit(-1);
  }
  while(fgets(tmp,256,fd)) {
    if(strstr(tmp,"Ethernet")) {
      eth_num++;
      /* parse string "Ethernet controller: **** (rev n)" */
      p = tmp; while(*p != ':') p++;
      pp = p+1; while(*p != '(') p++;
      p--; *p = '\0'; 
      strncpy(eth[eth_num].desc,pp,256);
      snprintf(label[eth_num],255,"eth%u: %s",eth_num, pp);
      fprintf(stderr,"%s\n",label[eth_num]);
      items = g_list_append(items, label[eth_num]);
      /* gathers current settings for the card
	 fetch_staticip_settings(eth_num); */
    }
  }
  fclose(fd);

  if(eth_num<0) g_list_append(items,(void*)"no ethernet card found on your system");
  gtk_combo_set_popdown_strings(GTK_COMBO(widget), items);
  g_list_free(items);
}


void
on_eth_choice_changed                  (GtkEditable     *editable,
                                        gpointer         user_data)
{
  sscanf(gtk_entry_get_text((GtkEntry*)editable),"eth%i:",&eth_sel);
  /* gathers current settings for the card */
  fetch_staticip_settings(eth_sel);
  switch(eth[eth_sel].conf) {
  case DHCP:
    if(toggle_dhcp)
      gtk_toggle_button_set_active((GtkToggleButton*)toggle_dhcp,TRUE);
    gtk_toggle_button_set_active((GtkToggleButton*)check_active,TRUE);
    break;
  case STATIC_IP:
    if(toggle_staticip)
      gtk_toggle_button_set_active((GtkToggleButton*)toggle_staticip,TRUE);
    gtk_toggle_button_set_active((GtkToggleButton*)check_active,TRUE);
    break;
  case NONE:
    gtk_toggle_button_set_active((GtkToggleButton*)check_active,FALSE);
  }

  if(win_staticip) {
    gtk_widget_destroy(win_staticip);
    win_staticip = NULL;
  }
}


void
on_toggle_dhcp_released                (GtkButton       *button,
                                        gpointer         user_data)
{
  if(eth_sel>eth_num || eth_sel<0) {
    fprintf(stderr,"wrong procedure call: card %i doesn't exists\n",eth_sel);
    return;
  }

  
  if(gtk_toggle_button_get_active((GtkToggleButton*)button)) {
    /* activating */
    eth[eth_sel].conf = DHCP;
    gtk_toggle_button_set_active((GtkToggleButton*)toggle_staticip,FALSE);
    gtk_toggle_button_set_active((GtkToggleButton*)check_active,TRUE);
  } else {
    /* deactivating */
    eth[eth_sel].conf = NONE;
    gtk_toggle_button_set_active((GtkToggleButton*)check_active,FALSE);
  }
    
}

void
on_toggle_staticip_released            (GtkButton       *button,
                                        gpointer         user_data)
{
  if(eth_sel>eth_num || eth_sel<0) {
    fprintf(stderr,"wrong procedure call: card %i doesn't exists\n",eth_sel);
    return;
  }

  if(gtk_toggle_button_get_active((GtkToggleButton*)button)) {
    /* activating */
    if(!win_staticip) {  
      win_staticip = create_win_staticip();
      gtk_widget_show(win_staticip);
    }
  } else {
    /* deactivating */
    eth[eth_sel].conf = NONE;
    gtk_toggle_button_set_active((GtkToggleButton*)check_active,FALSE);
  }
}

void
on_toggle_dhcp_realize                 (GtkWidget       *widget,
                                        gpointer         user_data)
{
  toggle_dhcp = widget;
  if(eth[eth_sel].conf == DHCP)
    gtk_toggle_button_set_active((GtkToggleButton*)widget,TRUE);
}


void
on_toggle_staticip_realize             (GtkWidget       *widget,
                                        gpointer         user_data)
{
  toggle_staticip = widget;
  if(eth[eth_sel].conf == STATIC_IP)
    gtk_toggle_button_set_active((GtkToggleButton*)widget,TRUE);
}


void
on_check_active_realize                (GtkWidget       *widget,
                                        gpointer         user_data)
{ check_active = widget; }


void
on_entry_staticip_address_realize      (GtkWidget       *widget,
                                        gpointer         user_data)
{
  ip_address = widget;
  gtk_entry_set_text((GtkEntry*)widget,eth[eth_sel].ip);
}


void
on_entry_staticip_netmask_realize      (GtkWidget       *widget,
                                        gpointer         user_data)
{
  netmask = widget;
  gtk_entry_set_text((GtkEntry*)widget,eth[eth_sel].netmask);
}


void
on_entry_staticip_gw_realize           (GtkWidget       *widget,
                                        gpointer         user_data)
{
  gateway = widget;
  gtk_entry_set_text((GtkEntry*)widget,eth[eth_sel].gw);
}


void
on_entry_staticip_dns_realize          (GtkWidget       *widget,
                                        gpointer         user_data)
{
  dns = widget;
  gtk_entry_set_text((GtkEntry*)widget,eth[eth_sel].dns);
}

void
on_button_staticip_ok_released         (GtkButton       *button,
                                        gpointer         user_data)
{
  int i1,i2,i3,i4, res;
  
  strncpy(eth[eth_sel].ip,gtk_entry_get_text((GtkEntry*)ip_address),128);
  res = sscanf(eth[eth_sel].ip,"%u.%u.%u.%u",&i1,&i2,&i3,&i4);
  if( !res || !strlen(eth[eth_sel].ip) ) {
    fprintf(stderr,"ERROR: invalid ip address\n"); return; }

  strncpy(eth[eth_sel].netmask,gtk_entry_get_text((GtkEntry*)netmask),128);
  res = sscanf(eth[eth_sel].netmask,"%u.%u.%u.%u",&i1,&i2,&i3,&i4);
  if( !res || !strlen(eth[eth_sel].netmask) ) {
    fprintf(stderr,"ERROR: invalid netmask, fallback to 255.255.255.0\n");
    strcpy(eth[eth_sel].netmask,"255.255.255.0");
  }

  strncpy(eth[eth_sel].gw,gtk_entry_get_text((GtkEntry*)gateway),128);
  res = sscanf(eth[eth_sel].gw,"%u.%u.%u.%u",&i1,&i2,&i3,&i4);
  if( !res || !strlen(eth[eth_sel].gw) ) {
    fprintf(stderr,"ERROR: invalid gateway\n");
    strcpy(eth[eth_sel].gw,"none");
  }
  
  strncpy(eth[eth_sel].dns,gtk_entry_get_text((GtkEntry*)dns),128);
  res = sscanf(eth[eth_sel].dns,"%u.%u.%u.%u",&i1,&i2,&i3,&i4);
  if( !res || !strlen(eth[eth_sel].dns) ) {
    fprintf(stderr,"ERROR: invalid domain name server\n");
    strcpy(eth[eth_sel].dns,"none");
  }
  
  eth[eth_sel].conf = STATIC_IP;
  gtk_widget_destroy(win_staticip);
}

void
on_button_main_apply_released          (GtkButton       *button,
                                        gpointer         user_data)
{
  pid_t proc;
  int res;
  if(!render_settings("/tmp/netconf")) return;
  proc = fork();
  if(proc==0) {
    execlp("xterm","netconf","-tn","linux","-bg","darkgrey","-fg","black",
	   "-T","Activating network settings","-geometry","118x20",
	   "-e","zsh","-c","netconf.apply /tmp/netconf; sleep 2", NULL);
    perror("can't fork to activate configuration");
    _exit(1);
  }
  wait(&res);
  unlink("/tmp/netconf");
}


void
on_button_main_save_released           (GtkButton       *button,
                                        gpointer         user_data)
{
  pid_t proc;
  int res;
  if(!render_settings("/etc/NETWORK")) return;
  proc = fork();
  if(proc==0) {
    execlp("xterm","netconf","-tn","linux","-bg","darkgrey","-fg","black",
	   "-T","Saving and activating network settings","-geometry","118x20",
	   "-e","zsh","-c","netconf.apply; sleep 2", NULL);
    perror("can't fork to activate configuration");
    _exit(1);
  }
  wait(&res);
  
}


void
on_entry_hostname_realize              (GtkWidget       *widget,
                                        gpointer         user_data)
{
  char *tmp;
  hostname = widget;

  tmp = getenv("HOSTNAME");
  if(tmp)
    gtk_entry_set_text((GtkEntry*)widget,tmp);
  else
    gtk_entry_set_text((GtkEntry*)widget,"dynebolic");
}



void
on_check_active_pressed                (GtkButton       *button,
                                        gpointer         user_data)
{
  gboolean res = FALSE;
  res |= gtk_toggle_button_get_active((GtkToggleButton*)toggle_dhcp);
  res |= gtk_toggle_button_get_active((GtkToggleButton*)toggle_staticip);
  if(!res) gtk_toggle_button_set_active((GtkToggleButton*)button,TRUE);
}


void
on_win_staticip_destroy                (GtkObject       *object,
                                        gpointer         user_data)
{ 
  win_staticip = NULL;
  if( eth[eth_sel].conf != STATIC_IP ) {
    gtk_toggle_button_set_active((GtkToggleButton*)toggle_staticip, FALSE);
  } else {
    gtk_toggle_button_set_active((GtkToggleButton*)check_active,TRUE);
    gtk_toggle_button_set_active((GtkToggleButton*)toggle_dhcp,FALSE);
  }
}


void
on_button_main_quit_released           (GtkButton       *button,
                                        gpointer         user_data)
{
  gtk_main_quit();
}
