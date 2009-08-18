// omc-learn.c
// LiVES (lives-exe)
// (c) G. Finch 2008 - 2009
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

#include "../libweed/weed.h"
#include "../libweed/weed-host.h"
#include "../libweed/weed-effects.h"

#include "support.h"
#include "main.h"
#include "paramwindow.h"
#include "effects-weed.h"
#include "interface.h"

#include "omc-learn.h"

#ifdef OMC_JS_IMPL
#include <linux/joystick.h>
#endif

#ifdef OMC_MIDI_IMPL
#include <sys/soundcard.h>
#endif

#include <errno.h>

// learn and match with an external control
// generally, external data is passed in as a type and a string (a sequence ascii encoded ints separated by spaces)
// the string will have a fixed sig(nature) which is matched against learned nodes
//
// the number of fixed values depends on the origin of the data; for example for a MIDI controller it is 2 (controller + controller number)
// the rest of the string is variables. These are either mapped in order to the parameters of the macro or can be filtered against

// these types/strings are matched against OMC macros - the macros have slots for parameters which are filled in order from variables in the input

// TODO !! - greedy matching should done - i.e. if an input sequence matches more than one macro, each of those macros will be triggered
// for now, only first match is acted on


// some events are filtered out, for example MIDI_NOTE_OFF, joystick button release; this needs to be done automatically

// TODO: we need end up with a table (struct *) like:
// int supertype;
// int ntypes;
// int *nfixed;
// int **min;
// int **max;
// boolean *uses_index;
// char **ignore;

// where min/max are not known we will need to calibrate


static OSCbuf obuf;
static gchar byarr[OSC_BUF_SIZE];
static lives_omc_macro_t omc_macros[N_OMC_MACROS];
static GSList *omc_node_list;
static gboolean omc_macros_inited=FALSE;

static gint tbl_rows;
static gint tbl_currow;

static GtkWidget *omclearn_dialog;
static GtkWidget *table;
static GtkWidget *top_vbox;


///////////////////////////


enum {
  TITLE_COLUMN,
  VALUE_COLUMN,
  FILTER_COLUMN,
  RANGE_COLUMN,
  OFFS1_COLUMN,
  SCALE_COLUMN,
  OFFS2_COLUMN,
  NUM_COLUMNS
};
 

enum {
  TITLE2_COLUMN,
  VALUE2_COLUMN,
  NUM2_COLUMNS
};



//////////////////////////////////////////////////////////////


static void omc_match_node_free(lives_omc_match_node_t *mnode) {

  if (mnode->nvars>0) {
    g_free(mnode->offs0);
    g_free(mnode->scale);
    g_free(mnode->offs1);
    g_free(mnode->min);
    g_free(mnode->max);
    g_free(mnode->matchp);
    g_free(mnode->matchi);
  }

  if (mnode->map!=NULL) g_free(mnode->map);
  if (mnode->fvali!=NULL) g_free(mnode->fvali);
  if (mnode->fvald!=NULL) g_free(mnode->fvald);

  g_free(mnode->srch);

  g_free(mnode);

}



static void remove_unused_nodes(void) {
  lives_omc_match_node_t *mnode;
  GSList *slist_last=NULL,*slist_next;
  GSList *slist=omc_node_list;

  while (slist!=NULL) {
    slist_next=slist->next;
    
    mnode=(lives_omc_match_node_t *)slist->data;
    
    if (mnode->macro==-1) {
      if (slist_last!=NULL) slist_last->next=slist->next;
      else omc_node_list=slist->next;
      omc_match_node_free(mnode);
    }
    else slist_last=slist;
    slist=slist_next;
  }
}


static inline int js_index(gchar *string) {
  // js index, or midi channel number
  gchar **array=g_strsplit(string," ",-1);
  gint res=atoi(array[1]);
  g_strfreev(array);
  return res;
}


static inline int midi_index(gchar *string) {
  // midi controller number
  gchar **array;
  gint res;
  if (get_token_count(string,' ')<3) return -1;

  array=g_strsplit(string," ",-1);
  res=atoi(array[2]);
  g_strfreev(array);
  return res;
}


#ifdef OMC_JS_IMPL



static int js_fd;




gchar * get_js_filename(void) {
  gchar *js_fname;

  // OPEN DEVICE FILE
  // first try to open /dev/input/js
  js_fname = "/dev/input/js"; 
  js_fd = open(js_fname, O_RDONLY|O_NONBLOCK);
  if (js_fd < 0) {
    // if it doesn't open, try to open /dev/input/js0
    js_fname = "/dev/input/js0"; 
    js_fd = open(js_fname, O_RDONLY|O_NONBLOCK);
    if (js_fd < 0) {
      js_fname = "/dev/js0";
      js_fd = open(js_fname, O_RDONLY|O_NONBLOCK);
      // if no device is found
      if (js_fd < 0) {
	return NULL;
      }
    }
  }
  return js_fname;
}



gboolean js_open(void) {
  gchar *msg;

  if (!(prefs->omc_dev_opts&OMC_DEV_JS)) return TRUE;

  if (prefs->omc_js_fname!=NULL) {
    js_fd = open(prefs->omc_js_fname, O_RDONLY|O_NONBLOCK);
    if (js_fd < 0) return FALSE;
  }
  else {
    gchar *tmp=get_js_filename();
    if (tmp!=NULL) {
      g_snprintf(prefs->omc_js_fname,256,"%s",tmp);
    }
  }
  if (prefs->omc_js_fname==NULL) return FALSE;

  mainw->ext_cntl[EXT_CNTL_JS]=TRUE;
  msg=g_strdup_printf(_("Responding to joystick events from %s\n"),prefs->omc_js_fname);
  d_print(msg);
  g_free(msg);

  return TRUE;
}



void js_close(void) {
  if (mainw->ext_cntl[EXT_CNTL_JS]) {
    close (js_fd);
    mainw->ext_cntl[EXT_CNTL_JS]=FALSE;
  }
}


gchar *js_mangle(void) {
  // get js event and process it
  struct js_event jse;
  size_t bytes;
  gchar *ret;
  int type=0;

  bytes = read(js_fd, &jse, sizeof(jse));

  if (bytes!=sizeof(jse)) return NULL;

  jse.type &= ~JS_EVENT_INIT; /* ignore synthetic events */
  if (jse.type == JS_EVENT_AXIS) {
    type=OMC_JS_AXIS;
    if (jse.value==0) return NULL;
  } else if (jse.type == JS_EVENT_BUTTON) {
    if (jse.value==0) return NULL;
    type=OMC_JS_BUTTON;
  }

  ret=g_strdup_printf("%d %d %d",type,jse.number,jse.value);

  return ret;

}


static inline int js_msg_type(gchar *string) {
  return atoi(string);
}



#endif  // OMC_JS


#ifdef OMC_MIDI_IMPL

static int midi_fd;


gchar *get_midi_filename(void) {
gchar *midi_fname;

  // OPEN DEVICE FILE
  midi_fname = "/dev/midi";
  midi_fd = open(midi_fname, O_RDONLY|O_NONBLOCK);
  if (midi_fd < 0) {
    midi_fname = "/dev/midi0";
    midi_fd = open(midi_fname, O_RDONLY|O_NONBLOCK);
    if (midi_fd < 0) {
      midi_fname = "/dev/midi1";
      midi_fd = open(midi_fname, O_RDONLY|O_NONBLOCK);
      if (midi_fd < 0) {
	return NULL;
      }
    }
  }
  return midi_fname;
}


gboolean midi_open(void) {

#ifdef ALSA_MIDI
  int port_in_id;
  mainw->seq_handle=NULL;
#endif

  gchar *msg;

  if (!(prefs->omc_dev_opts&OMC_DEV_MIDI)) return TRUE;

#ifdef ALSA_MIDI
  if (prefs->use_alsa_midi) {

    d_print(_("Creating ALSA seq port..."));

    // ORL Ouverture d'un port ALSA
    if (snd_seq_open(&mainw->seq_handle, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0) {
      d_print_failed();
      return FALSE;
    }
    
    snd_seq_set_client_name(mainw->seq_handle, "LiVES");
    if ((port_in_id = snd_seq_create_simple_port(mainw->seq_handle, "LiVES",
						 SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE,
						 SND_SEQ_PORT_TYPE_APPLICATION)) < 0) {
      
      d_print_failed();
      return FALSE;
    }
    
    d_print_done();
  }
  else {

#endif
  
  if (prefs->omc_midi_fname!=NULL) {
    midi_fd = open(prefs->omc_midi_fname, O_RDONLY|O_NONBLOCK);
    if (midi_fd < 0) return FALSE;
  }
  else {
    gchar *tmp=get_midi_filename();
    if (tmp!=NULL) {
      g_snprintf(prefs->omc_midi_fname,256,"%s",tmp);
    }
  }
  if (prefs->omc_midi_fname==NULL) return FALSE;

  msg=g_strdup_printf(_("Responding to MIDI events from %s\n"),prefs->omc_midi_fname);
  d_print(msg);
  g_free(msg);

#ifdef ALSA_MIDI
  }
#endif

  mainw->ext_cntl[EXT_CNTL_MIDI]=TRUE;

  return TRUE;
}



void midi_close(void) {
  if (mainw->ext_cntl[EXT_CNTL_MIDI]) {

#ifdef ALSA_MIDI
    if (mainw->seq_handle!=NULL) {
      // close
    }
    else {

#endif

    close (midi_fd);

#ifdef ALSA_MIDI
    }
#endif

    mainw->ext_cntl[EXT_CNTL_MIDI]=FALSE;
  }
}



static int get_midi_len(int msgtype) {
  switch (msgtype) {
  case OMC_MIDI_CONTROLLER:
  case OMC_MIDI_NOTE:
  case OMC_MIDI_PITCH_BEND:
    return 3;
  case OMC_MIDI_NOTE_OFF:
    return 2;
  }
  return 0;
}


static gint midi_msg_type(gchar *string) {
  gint type=atoi(string);

  if ((type&0XF0)==0X90) return OMC_MIDI_NOTE;
  if ((type&0XF0)==0x80) return OMC_MIDI_NOTE_OFF;
  if ((type&0XF0)==0xB0) return OMC_MIDI_CONTROLLER;
  if ((type&0XF0)==0xC0) return OMC_MIDI_PGM_CHANGE;
  if ((type&0XF0)==0xE0) return OMC_MIDI_PITCH_BEND;


  // other types are currently ignored
  // 0XA0 is aftertouch, has key and value

  // OxC0 is patch change, with 2 bytes parm
  // 0xD0 is channel pressure, 1 byte parm

  // 0xE0 is pitch bend: lsb 7 bits, msb 7 bits

  // 0XF0 is sysex

  return 0;
}



gchar *midi_mangle(void) {
  // get MIDI event and process it
  gchar *string=NULL;

  ssize_t bytes,tot=0,allowed=prefs->midi_rpt;
  unsigned char midbuf[4],xbuf[4];
  gint target=1,mtype=0,idx;
  gboolean got_target=FALSE;
  gchar *str;

#ifdef ALSA_MIDI
  int npfd;
  struct pollfd *pfd;
  snd_seq_event_t *ev;
  int typeNumber;

  if (mainw->seq_handle!=NULL) {

    npfd = snd_seq_poll_descriptors_count(mainw->seq_handle, POLLIN);
    pfd = (struct pollfd *)alloca(npfd * sizeof(struct pollfd));
    snd_seq_poll_descriptors(mainw->seq_handle, pfd, npfd, POLLIN);
    
    if (poll(pfd, npfd, 0) > 0) {
      
      do {
	snd_seq_event_input(mainw->seq_handle, &ev);
	switch (ev->type) {
	case SND_SEQ_EVENT_CONTROLLER:   
	  typeNumber=176;
	  string=g_strdup_printf("%d %d %d %d\n",typeNumber+ev->data.control.channel, ev->data.control.channel, ev->data.control.param,ev->data.control.value);
	  
	  break;
	  /*              case SND_SEQ_EVENT_PITCHBEND:
			  typeNumber=224;
			  fprintf(stderr, "Pitchbender event on Channel %2d: %5d   \n", 
			  ev->data.control.channel, ev->data.control.value);
			  fprintf(stderr,"Essai String: %d %d %d %d\n",typeNumber+ev->data.control.channel, ev->data.control.channel, ev->data.control.param,ev->data.control.value);
			  break;  */
	case SND_SEQ_EVENT_NOTEON:
	  typeNumber=144;
	  string=g_strdup_printf("%d %d %d %d\n",typeNumber+ev->data.note.channel, ev->data.note.channel, ev->data.note.note,ev->data.note.velocity);
	  
	  break;        
	case SND_SEQ_EVENT_NOTEOFF:       
	  typeNumber=128;
	  string=g_strdup_printf("%d %d %d\n",typeNumber+ev->data.note.channel, ev->data.note.channel, ev->data.note.note);
	  
	  break;        
	case SND_SEQ_EVENT_PGMCHANGE:       
	  typeNumber=192;
	  string=g_strdup_printf("%d %d %d %d\n",typeNumber+ev->data.note.channel, ev->data.note.channel, ev->data.control.param, ev->data.control.value);
	  
	  break;        
	  
	}
	snd_seq_free_event(ev);
	
      } while (snd_seq_event_input_pending(mainw->seq_handle, 0) > 0);
      
    }
  }
  else {

#endif

  if (midi_fd==-1) return NULL;

  while (tot<target) {
    bytes = read(midi_fd, xbuf, target-tot);
    
    if (bytes<1) {
      if (--allowed<0) return NULL;
      continue;
    }
    
    str=g_strdup_printf("%d",xbuf[0]);

    if (!got_target) {
      target=get_midi_len((mtype=midi_msg_type(str)));
      got_target=TRUE;
    }
    
    g_free(str);

    //g_print("midi pip %d %02X , tg=%d\n",bytes,xbuf[0],target);

    memcpy(midbuf+tot,xbuf,bytes);

    tot+=bytes;

  }

  if (mtype==0) return NULL;

  idx=(midbuf[0]&0x0F);

  if (target==2) string=g_strdup_printf("%u %u %u",midbuf[0],idx,midbuf[1]);
  else if (target==3) string=g_strdup_printf("%u %u %u %u",midbuf[0],idx,midbuf[1],midbuf[2]);
  else string=g_strdup_printf("%u %u %u %u %u",midbuf[0],idx,midbuf[1],midbuf[2],midbuf[3]);


#ifdef ALSA_MIDI
    }
#endif

  //g_print("got %s\n",string);

  return string;
}


#endif //OMC_MIDI_IMPL



static inline gchar *cut_string_elems(gchar *string, gint nelems) {
  // remove elements after nelems

  gchar *retval=g_strdup(string);
  register int i;
  size_t slen=strlen(string);

  if (nelems<0) return retval;

  for (i=0;i<slen;i++) {
    if (!strncmp((string+i)," ",1)) {
      if (--nelems==0) {
	memset(retval+i,0,1);
	return retval;
      }
    }
  }
  return retval;
}







static gchar *omc_learn_get_pname(gint type, gint idx) {
  switch (type) {
  case OMC_MIDI_CONTROLLER:
  case OMC_MIDI_PITCH_BEND:
    return g_strdup(_("data"));
  case OMC_MIDI_NOTE:
    if (idx==1) return (_("velocity"));
  case OMC_MIDI_NOTE_OFF:
    return g_strdup(_("note"));
  case OMC_JS_AXIS:
    return g_strdup(_("value"));
  default:
    return (_("state"));
  }
}



static gint omc_learn_get_pvalue(gint type, gint idx, gchar *string) {
  gchar **array=g_strsplit(string," ",-1);
  gint res;

  switch (type) {
  case OMC_MIDI_PITCH_BEND:
    res=(atoi(array[3+idx])<<7)+atoi(array[2+idx]);
    break;
  case OMC_MIDI_CONTROLLER:
    res=atoi(array[3+idx]);
    break;
  default:
    res=atoi(array[2+idx]);
    break;
  }

  g_strfreev(array);
  return res;
}



static void cell1_edited_callback (GtkCellRendererSpin *spinbutton, gchar *path_string, gchar *new_text, gpointer user_data) {
  lives_omc_match_node_t *mnode=(lives_omc_match_node_t *)user_data;

  lives_omc_macro_t omacro=omc_macros[mnode->macro];

  gint vali;
  gdouble vald;

  GtkTreeIter iter;
  GtkTreeModel *tmodel;

  gchar **array=g_strsplit(path_string,":",2);
  gint row=atoi(array[1]);

  g_strfreev(array);

  if (row>(omacro.nparams-mnode->nvars)) {
    // text, so dont alter
    return;
  }

  tmodel=gtk_tree_view_get_model (GTK_TREE_VIEW(mnode->treev2));
  gtk_tree_model_get_iter_from_string(tmodel,&iter,path_string);
  
  switch (omacro.ptypes[row]) {
  case OMC_PARAM_INT:
    vali=atoi(new_text);
    mnode->fvali[row]=vali;
    break;
  case OMC_PARAM_DOUBLE:
    vald=g_strtod (new_text,NULL);
    mnode->fvald[row]=vald;
    break;
  }
  
  gtk_tree_store_set(mnode->gtkstore2,&iter,VALUE2_COLUMN,new_text,-1);
  
}



static void omc_macro_row_add_params(lives_omc_match_node_t *mnode, gint row) {
  lives_omc_macro_t macro=omc_macros[mnode->macro];

  GtkCellRenderer *renderer;
  GtkTreeViewColumn *column;
  
  int i;
  gchar *strval=NULL,*vname;

  gchar *oldval=NULL,*final=NULL;

  gint mfrom;
  
  GtkTreeIter iter1,iter2;

  GtkObject *spinadj;

  mnode->gtkstore2 = gtk_tree_store_new (NUM2_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

  if (macro.nparams==0) return;

  gtk_tree_store_append (mnode->gtkstore2, &iter1, NULL);  /* Acquire an iterator */
  gtk_tree_store_set (mnode->gtkstore2, &iter1, TITLE2_COLUMN, (_("Params.")), -1);

  for (i=0;i<macro.nparams;i++) {
    gtk_tree_store_append (mnode->gtkstore2, &iter2, &iter1);  /* Acquire a child iterator */
       
    if (oldval!=NULL) {
      g_free(oldval);
      oldval=NULL;
    }
       
    if (final!=NULL) {
      g_free(final);
      final=NULL;
    }

    if ((mfrom=mnode->map[i])!=-1) strval=g_strdup(_("variable"));
    else {
      switch (macro.ptypes[i]) {
      case OMC_PARAM_INT:
	strval=g_strdup_printf("%d",mnode->fvali[i]);
	break;
      case OMC_PARAM_DOUBLE:
	strval=g_strdup_printf("%.*f",OMC_FP_FIX,mnode->fvald[i]);
	break;
	
      }
    }

    vname=macro.pname[i];

    gtk_tree_store_set (mnode->gtkstore2, &iter2, TITLE2_COLUMN, vname, VALUE2_COLUMN, strval, -1);
  }

  g_free (strval);

  mnode->treev2 = gtk_tree_view_new_with_model (GTK_TREE_MODEL (mnode->gtkstore2));

  gtk_widget_modify_base(mnode->treev2, GTK_STATE_NORMAL, &palette->menu_and_bars);
  
  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes (NULL,
						     renderer,
						     "text", TITLE2_COLUMN,
						     NULL);
  
  gtk_tree_view_append_column (GTK_TREE_VIEW (mnode->treev2), column);
  
  renderer = gtk_cell_renderer_spin_new ();
  
  spinadj=gtk_adjustment_new (0., -100000., 100000., 1., 10., 0);
  
  g_object_set (renderer, "width-chars", 7, "mode", GTK_CELL_RENDERER_MODE_EDITABLE,
		"editable", TRUE, "xalign", 1.0, "adjustment", spinadj, NULL);
  
  g_signal_connect(renderer, "edited", (GCallback) cell1_edited_callback, mnode);
  
  
  //  renderer = gtk_cell_renderer_text_new ();
  column = gtk_tree_view_column_new_with_attributes (_("value"),
						     renderer,
						     "text", VALUE2_COLUMN,
						     NULL);
  gtk_tree_view_append_column (GTK_TREE_VIEW (mnode->treev2), column);






  gtk_widget_show (mnode->treev2);
  
  gtk_table_attach (GTK_TABLE (table), mnode->treev2, 3, 4, row, row+1,
		    (GtkAttachOptions) (GTK_FILL|GTK_EXPAND),
		    (GtkAttachOptions) (0), 0, 0);
  


}




static void omc_learn_link_params(lives_omc_match_node_t *mnode) {
  lives_omc_macro_t omc_macro=omc_macros[mnode->macro];
  int mps=omc_macro.nparams-1;
  int lps=mnode->nvars-1;
  int i;
  
  mnode->map=(gint *)g_malloc(omc_macro.nparams*sizint);
  mnode->fvali=(gint *)g_malloc(omc_macro.nparams*sizint);
  mnode->fvald=(gdouble *)g_malloc(omc_macro.nparams*sizdbl);

  for (i=mps;i>=0;i--) {
    if (lps<0) {
      mnode->map[i]=-1;
      if (omc_macro.ptypes[i]==OMC_PARAM_INT) mnode->fvali[i]=omc_macro.vali[i];
      else mnode->fvald[i]=omc_macro.vald[i];
    }
    else {
      if (!mnode->matchp[lps]) mnode->map[i]=lps;
      else i++;
    }
    lps--;
  }

}









static void on_omc_combo_entry_changed (GtkEntry *macro_entry, gpointer ptr) {
  const gchar *macro_text=gtk_entry_get_text(macro_entry);

  int i;
    
  lives_omc_match_node_t *mnode=(lives_omc_match_node_t *)ptr;

  gint row=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(macro_entry),"row"));

  if (mnode->treev2!=NULL) {
    // remove old mapping
    gtk_widget_destroy(mnode->treev2);
    mnode->treev2=NULL;
    
    mnode->macro=-1;

    g_free(mnode->map);
    g_free(mnode->fvali);
    g_free(mnode->fvald);

    mnode->map=mnode->fvali=NULL;
    mnode->fvald=NULL;

  }

  if (!strcmp(macro_text,mainw->none_string)) return;

  for (i=0;i<=N_OMC_MACROS;i++) {
    if (!strcmp(macro_text,omc_macros[i].macro_text)) break;
  }


  if (i>0) {
    mnode->macro=i;
    omc_learn_link_params(mnode);
    omc_macro_row_add_params(mnode,row);
  }
}




static void cell_toggled_callback (GtkCellRendererToggle *toggle, gchar *path_string, gpointer user_data) {
  lives_omc_match_node_t *mnode=(lives_omc_match_node_t *)user_data;
  gchar **array;
  gint row;

  if (get_token_count(path_string,':')<2) return;
  
  array=g_strsplit(path_string,":",2);
  row=atoi(array[1]);
  g_strfreev(array);

  gtk_cell_renderer_toggle_set_active(toggle,!(gtk_cell_renderer_toggle_get_active(toggle)));

  mnode->matchp[row]=!(mnode->matchp[row]);

}



static void cell_edited_callback (GtkCellRendererSpin *spinbutton, gchar *path_string, gchar *new_text, gpointer user_data) {
  lives_omc_match_node_t *mnode=(lives_omc_match_node_t *)user_data;

  gint col=GPOINTER_TO_INT(g_object_get_data(G_OBJECT(spinbutton),"colnum"));

  gint vali;
  gdouble vald;

  GtkTreeIter iter;
  GtkTreeModel *tmodel;

  gchar **array=g_strsplit(path_string,":",2);
  gint row=atoi(array[1]);

  g_strfreev(array);

  tmodel=gtk_tree_view_get_model (GTK_TREE_VIEW(mnode->treev1));
  gtk_tree_model_get_iter_from_string(tmodel,&iter,path_string);

  switch (col) {
  case OFFS1_COLUMN:
    vali=atoi(new_text);
    mnode->offs0[row]=vali;
   break;
  case OFFS2_COLUMN:
    vali=atoi(new_text);
    mnode->offs1[row]=vali;
    break;
  case SCALE_COLUMN:
    vald=g_strtod (new_text,NULL);
    mnode->scale[row]=vald;
    break;
  }

  gtk_tree_store_set(mnode->gtkstore,&iter,col,new_text,-1);
 
}








static GtkWidget *create_omc_macro_combo(lives_omc_match_node_t *mnode, gint row) {
  GList *macro_list=NULL;
  int i;
  gulong omc_combo_fn;

  GtkWidget *combo = gtk_combo_new ();

  macro_list=g_list_append(macro_list,mainw->none_string);

  for (i=0;i<N_OMC_MACROS;i++) {
    if (omc_macros[i].msg==NULL) break;
    macro_list=g_list_append(macro_list,omc_macros[i].macro_text);
  }

  combo_set_popdown_strings (GTK_COMBO (combo), macro_list);

  g_list_free(macro_list);

  gtk_editable_set_editable (GTK_EDITABLE((GTK_COMBO(combo))->entry),FALSE);
  omc_combo_fn=g_signal_connect_after (G_OBJECT (GTK_COMBO(combo)->entry), "changed", G_CALLBACK (on_omc_combo_entry_changed), mnode);
  
  if (mnode->macro!=-1) {
    g_signal_handler_block(GTK_COMBO(combo)->entry,omc_combo_fn);
    gtk_entry_set_text (GTK_ENTRY((GTK_COMBO(combo))->entry),omc_macros[mnode->macro].macro_text);
    g_signal_handler_unblock(GTK_COMBO(combo)->entry,omc_combo_fn);
  }

  g_object_set_data(G_OBJECT((GTK_COMBO(combo))->entry),"row",GINT_TO_POINTER(row));

  return combo;
}



static void omc_learner_add_row(gint type, gint detail, lives_omc_match_node_t *mnode, gchar *string) {
   GtkWidget *label,*combo;
   GtkCellRenderer *renderer;
   GtkTreeViewColumn *column;
  
   int i,val;
   gchar *strval,*strval2,*strval3,*strval4,*vname,*valstr;

   gchar *oldval=NULL,*final=NULL;

   gchar *labelt=NULL;

   GtkTreeIter iter1,iter2;

   GtkObject *spinadj;

   gint chan;

   tbl_rows++;
   gtk_table_resize(GTK_TABLE(table),tbl_rows,6);
			
   mnode->gtkstore = gtk_tree_store_new (NUM_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

   gtk_tree_store_append (mnode->gtkstore, &iter1, NULL);  /* Acquire an iterator */
   gtk_tree_store_set (mnode->gtkstore, &iter1, TITLE_COLUMN, (_("Vars.")), -1);

   for (i=0;i<mnode->nvars;i++) {

     gtk_tree_store_append (mnode->gtkstore, &iter2, &iter1);  /* Acquire a child iterator */
       
     if (oldval!=NULL) {
       g_free(oldval);
       oldval=NULL;
     }
       
     if (final!=NULL) {
       g_free(final);
       final=NULL;
     }

     strval=g_strdup_printf("%d - %d",mnode->min[i],mnode->max[i]);
     strval2=g_strdup_printf("%d",mnode->offs0[i]);
     strval3=g_strdup_printf("%.*f",OMC_FP_FIX,mnode->scale[i]);
     strval4=g_strdup_printf("%d",mnode->offs1[i]);

     if (type>0) {
       vname=omc_learn_get_pname(type,i);
       val=omc_learn_get_pvalue(type,i,string);
       valstr=g_strdup_printf("%d",val);
       if (!mnode->matchp[i]) {
	 mnode->matchi[i]=val;
       }
     }
     else {
       type=-type;
       vname=omc_learn_get_pname(type,i);
       if (mnode->matchp[i]) valstr=g_strdup_printf("%d",mnode->matchi[i]);
       else valstr=g_strdup("-");
     }

     gtk_tree_store_set (mnode->gtkstore, &iter2, TITLE_COLUMN, vname, VALUE_COLUMN, valstr, FILTER_COLUMN, mnode->matchp[i], RANGE_COLUMN, strval, OFFS1_COLUMN, strval2, SCALE_COLUMN, strval3, OFFS2_COLUMN, strval4, -1);

     g_free (strval);
     g_free (strval2);
     g_free (strval3);
     g_free (strval4);
     g_free(valstr);
     g_free(vname);
   }

   mnode->treev1 = gtk_tree_view_new_with_model (GTK_TREE_MODEL (mnode->gtkstore));

   if (type<0) type=-type;

   switch (type) {
   case OMC_MIDI_NOTE:
     chan=js_index(string);
     labelt=g_strdup_printf(_("MIDI ch %d note on"),chan);
     break;
   case OMC_MIDI_NOTE_OFF:
     chan=js_index(string);
     labelt=g_strdup_printf(_("MIDI ch %d note off"),chan);
     break;
   case OMC_MIDI_CONTROLLER:
     chan=js_index(string);
     labelt=g_strdup_printf(_("MIDI ch %d controller %d"),chan,detail);
     break;
   case OMC_MIDI_PITCH_BEND:
     chan=js_index(string);
     labelt=g_strdup_printf(_("MIDI ch %d pitch bend"),chan,detail);
     break;
   case OMC_JS_BUTTON:
     labelt=g_strdup_printf(_("Joystick button %d"),detail);
     break;
   case OMC_JS_AXIS:
     labelt=g_strdup_printf(_("Joystick axis %d"),detail);
     break;
   }
   
   label = gtk_label_new (labelt);
   gtk_widget_show (label);
   
   if (palette->style&STYLE_1) {
     gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &palette->normal_fore);
   }

   if (labelt!=NULL) g_free(labelt);
     
   tbl_currow++;
   gtk_table_attach (GTK_TABLE (table), label, 0, 1, tbl_currow, tbl_currow+1,
		     (GtkAttachOptions) (0),
		     (GtkAttachOptions) (0), 0, 0);
  
   // properties
   gtk_widget_modify_base(mnode->treev1, GTK_STATE_NORMAL, &palette->menu_and_bars);
  
   renderer = gtk_cell_renderer_text_new ();
   column = gtk_tree_view_column_new_with_attributes (NULL,
						      renderer,
						      "text", TITLE_COLUMN,
						      NULL);
  
   gtk_tree_view_append_column (GTK_TREE_VIEW (mnode->treev1), column);
  
   renderer = gtk_cell_renderer_text_new ();
   column = gtk_tree_view_column_new_with_attributes (_("value"),
						      renderer,
						      "text", VALUE_COLUMN,
						      NULL);
   gtk_tree_view_append_column (GTK_TREE_VIEW (mnode->treev1), column);
  
  
   renderer = gtk_cell_renderer_toggle_new ();
   column = gtk_tree_view_column_new_with_attributes (_("x"),
						      renderer,
						      NULL);
   gtk_tree_view_append_column (GTK_TREE_VIEW (mnode->treev1), column);

   g_signal_connect(renderer, "toggled", (GCallback) cell_toggled_callback, mnode);

   renderer = gtk_cell_renderer_text_new ();
   column = gtk_tree_view_column_new_with_attributes (_("range"),
						      renderer,
						      "text", RANGE_COLUMN,
						      NULL);
   gtk_tree_view_append_column (GTK_TREE_VIEW (mnode->treev1), column);


   renderer = gtk_cell_renderer_spin_new ();
   g_object_set_data(G_OBJECT(renderer), "colnum", GUINT_TO_POINTER(OFFS1_COLUMN));

   spinadj=gtk_adjustment_new (0., -100000., 100000., 1., 10., 0);

   g_object_set (renderer, "width-chars", 7, "mode", GTK_CELL_RENDERER_MODE_EDITABLE,
		 "editable", TRUE, "xalign", 1.0, "adjustment", spinadj, NULL);

   g_signal_connect(renderer, "edited", (GCallback) cell_edited_callback, mnode);




   column = gtk_tree_view_column_new_with_attributes (_("+ offset1"),
						      renderer,
						       "text", OFFS1_COLUMN,
						      NULL);
   gtk_tree_view_append_column (GTK_TREE_VIEW (mnode->treev1), column);

   renderer = gtk_cell_renderer_spin_new ();

   spinadj=gtk_adjustment_new (1., -100000., 100000., 1., 10., 0);

   g_object_set (renderer, "width-chars", 12, "mode", GTK_CELL_RENDERER_MODE_EDITABLE,
		 "editable", TRUE, "xalign", 1.0, "adjustment", spinadj, 
		 "digits", OMC_FP_FIX, NULL);

   g_object_set_data(G_OBJECT(renderer), "colnum", GUINT_TO_POINTER(SCALE_COLUMN));
   g_signal_connect(renderer, "edited", (GCallback) cell_edited_callback, mnode);


   column = gtk_tree_view_column_new_with_attributes (_("* scale"),
						      renderer,
						      "text", SCALE_COLUMN,
						      NULL);
   gtk_tree_view_append_column (GTK_TREE_VIEW (mnode->treev1), column);

   renderer = gtk_cell_renderer_spin_new ();


   spinadj=gtk_adjustment_new (0., -100000., 100000., 1., 10., 0);

   g_object_set (renderer, "width-chars", 7, "mode", GTK_CELL_RENDERER_MODE_EDITABLE,
		 "editable", TRUE, "xalign", 1.0, "adjustment", spinadj, NULL);

   g_object_set_data(G_OBJECT(renderer), "colnum", GUINT_TO_POINTER(OFFS2_COLUMN));
   g_signal_connect(renderer, "edited", (GCallback) cell_edited_callback, mnode);


   column = gtk_tree_view_column_new_with_attributes (_("+ offset2"),
						      renderer,
						      "text", OFFS2_COLUMN,
						      NULL);
   gtk_tree_view_append_column (GTK_TREE_VIEW (mnode->treev1), column);

   gtk_widget_show (mnode->treev1);
  
   gtk_table_attach (GTK_TABLE (table), mnode->treev1, 1, 2, tbl_currow, tbl_currow+1,
		     (GtkAttachOptions) (0),
		     (GtkAttachOptions) (0), 0, 0);

   combo=create_omc_macro_combo(mnode,tbl_currow);

   gtk_widget_show (combo);
  
   gtk_table_attach (GTK_TABLE (table), combo, 2, 3, tbl_currow, tbl_currow+1,
		     (GtkAttachOptions) 0,
		     (GtkAttachOptions) (0), 0, 0);

}
   




static void killit(GtkWidget *widget, gpointer user_data) {
  gtk_widget_destroy(widget);
}


static void show_existing(void) {
  GSList *slist=omc_node_list;
  lives_omc_match_node_t *mnode;
  gint type,supertype;
  gchar **array,*srch;
  gint idx;

  while (slist!=NULL) {
    mnode=(lives_omc_match_node_t *)slist->data;

    srch=g_strdup(mnode->srch);
    array=g_strsplit(srch," ",-1);

    supertype=atoi(array[0]);
#ifdef OMC_MIDI_IMPL
    if (supertype==OMC_MIDI) {
      size_t blen;
      gchar *tmp;

      type=midi_msg_type(array[1]);
      if (get_token_count(srch,' ')>3) idx=atoi(array[3]);
      else idx=-1;
      srch=g_strdup(mnode->srch);
      tmp=cut_string_elems(srch,1);
      blen=strlen(tmp);
      tmp=g_strdup(srch+blen+1);
      g_free(srch);
      srch=tmp;
    }
    else {
#endif
      type=supertype;
      idx=atoi(array[1]);
#ifdef OMC_MIDI_IMPL
    }
#endif
    g_strfreev(array);

    omc_learner_add_row(-type,idx,mnode,srch);
    g_free(srch);

    omc_macro_row_add_params(mnode,tbl_currow);

    slist=slist->next;
  }
}



static void clear_unmatched (GtkButton *button, gpointer user_data) {

  // destroy everything in table
  
  gtk_container_foreach(GTK_CONTAINER(table),killit,NULL);

  remove_unused_nodes();

  show_existing();

}



static void close_learner_dialog (GtkButton *button, gpointer user_data) {
  mainw->cancelled=CANCEL_USER;
}



GtkWidget *create_omclearn_dialog(void) {
  GtkWidget *ok_button;
  GtkWidget *clear_button;
  GtkWidget *hbuttonbox;
  GtkWidget *scrolledwindow;
  gint winsize_h,scr_width=mainw->scr_width;
  gint winsize_v,scr_height=mainw->scr_height;
  
  tbl_rows=4;
  tbl_currow=-1;

  if (prefs->gui_monitor!=0) {
    scr_width=mainw->mgeom[prefs->gui_monitor-1].width;
    scr_height=mainw->mgeom[prefs->gui_monitor-1].height;
  }
  
  winsize_h=scr_width-100;
  winsize_v=scr_height-100;
  
  omclearn_dialog = gtk_dialog_new ();
  
  gtk_widget_modify_bg(omclearn_dialog, GTK_STATE_NORMAL, &palette->menu_and_bars);
  gtk_window_set_title (GTK_WINDOW (omclearn_dialog), _("LiVES: OMC learner"));
  gtk_window_add_accel_group (GTK_WINDOW (omclearn_dialog), mainw->accel_group);
  top_vbox=GTK_DIALOG(omclearn_dialog)->vbox;

  table = gtk_table_new (tbl_rows, 4, FALSE);
  gtk_widget_show (table);

  gtk_table_set_col_spacings(GTK_TABLE(table),20);

  scrolledwindow = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_show (scrolledwindow);
  gtk_box_pack_start (GTK_BOX (top_vbox), scrolledwindow, TRUE, TRUE, 0);
  
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolledwindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (scrolledwindow), table);
  gtk_widget_set_size_request (scrolledwindow, winsize_h, winsize_v);
  
  if (palette->style&STYLE_1) {
    gtk_widget_modify_bg(gtk_bin_get_child (GTK_BIN (scrolledwindow)), GTK_STATE_NORMAL, &palette->normal_back);
  }
  
  gtk_viewport_set_shadow_type (GTK_VIEWPORT (gtk_bin_get_child (GTK_BIN (scrolledwindow))),GTK_SHADOW_IN);
  
  hbuttonbox = gtk_hbutton_box_new ();
  gtk_widget_show (hbuttonbox);
  
  gtk_box_pack_start (GTK_BOX (top_vbox), hbuttonbox, TRUE, TRUE, 0);
  
  gtk_button_box_set_child_size (GTK_BUTTON_BOX (hbuttonbox), DEF_BUTTON_WIDTH, -1);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (hbuttonbox), GTK_BUTTONBOX_SPREAD);
  
  clear_button = gtk_button_new_with_mnemonic (_("Clear _unmatched"));
  gtk_widget_show (clear_button);
  gtk_container_add (GTK_CONTAINER (hbuttonbox), clear_button);
  
  GTK_WIDGET_SET_FLAGS (clear_button, GTK_CAN_DEFAULT|GTK_CAN_FOCUS);

  g_signal_connect (GTK_OBJECT (clear_button), "clicked",
		    G_CALLBACK (clear_unmatched),
		    NULL);

  ok_button = gtk_button_new_with_mnemonic (_("Close _window"));
  gtk_widget_show (ok_button);
  gtk_container_add (GTK_CONTAINER (hbuttonbox), ok_button);
  
  GTK_WIDGET_SET_FLAGS (ok_button, GTK_CAN_DEFAULT|GTK_CAN_FOCUS);
  gtk_widget_grab_default (ok_button);
  
  g_signal_connect (GTK_OBJECT (ok_button), "clicked",
		    G_CALLBACK (close_learner_dialog),
		    NULL);
  
  if (prefs->gui_monitor!=0) {
    gint xcen=mainw->mgeom[prefs->gui_monitor-1].x+(mainw->mgeom[prefs->gui_monitor-1].width-omclearn_dialog->allocation.width)/2;
    gint ycen=mainw->mgeom[prefs->gui_monitor-1].y+(mainw->mgeom[prefs->gui_monitor-1].height-omclearn_dialog->allocation.height)/2;
    gtk_window_set_screen(GTK_WINDOW(omclearn_dialog),mainw->mgeom[prefs->gui_monitor-1].screen);
    gtk_window_move(GTK_WINDOW(omclearn_dialog),xcen,ycen);
  }
  
  if (prefs->open_maximised) {
    gtk_window_maximize (GTK_WINDOW(omclearn_dialog));
  }
  
  return omclearn_dialog;
}





static void init_omc_macros(void) {
  int i;

  for (i=0;i<N_OMC_MACROS;i++) {
    omc_macros[i].macro_text=NULL;
    omc_macros[i].info_text=NULL;
    omc_macros[i].msg=NULL;
    omc_macros[i].nparams=0;
    omc_macros[i].pname=NULL;
  }

  omc_macros[0].msg=g_strdup("/video/play");
  omc_macros[0].macro_text=g_strdup(_("Start video playback"));

  omc_macros[1].msg=g_strdup("/video/stop");
  omc_macros[1].macro_text=g_strdup(_("Stop video playback"));


  omc_macros[2].msg=g_strdup("/clip/foreground/select");
  omc_macros[2].macro_text=g_strdup(_("Clip select <clipnum>"));
  omc_macros[2].info_text=g_strdup(_("Switch foreground clip to the nth valid clip"));
  omc_macros[2].nparams=1;

  omc_macros[3].msg=g_strdup("/video/play/forwards");
  omc_macros[3].macro_text=g_strdup(_("Play forwards"));
  omc_macros[3].info_text=g_strdup(_("Play video in a forwards direction"));

  omc_macros[4].msg=g_strdup("/video/play/backwards");
  omc_macros[4].macro_text=g_strdup(_("Play backwards"));
  omc_macros[4].info_text=g_strdup(_("Play video in a backwards direction"));

  omc_macros[5].msg=g_strdup("/video/play/reverse");
  omc_macros[5].macro_text=g_strdup(_("Reverse playback direction"));
  omc_macros[5].info_text=g_strdup(_("Reverse direction of video playback"));

  omc_macros[6].msg=g_strdup("/video/play/faster");
  omc_macros[6].macro_text=g_strdup(_("Play video faster"));
  omc_macros[6].info_text=g_strdup(_("Play video at a slightly faster rate"));

  omc_macros[7].msg=g_strdup("/video/play/slower");
  omc_macros[7].macro_text=g_strdup(_("Play video slower"));
  omc_macros[7].info_text=g_strdup(_("Play video at a slightly slower rate"));

  omc_macros[8].msg=g_strdup("/video/freeze/toggle");
  omc_macros[8].macro_text=g_strdup(_("Toggle video freeze"));
  omc_macros[8].info_text=g_strdup(_("Freeze video, or if already frozen, unfreeze it"));

  omc_macros[9].msg=g_strdup("/video/fps/set");
  omc_macros[9].macro_text=g_strdup(_("Set video framerate to <fps>"));
  omc_macros[9].info_text=g_strdup(_("Set framerate of foreground clip to <float fps>"));
  omc_macros[9].nparams=1;
  
  omc_macros[10].msg=g_strdup("/record/enable");
  omc_macros[10].macro_text=g_strdup(_("Start recording"));

  omc_macros[11].msg=g_strdup("/record/disable");
  omc_macros[11].macro_text=g_strdup(_("Stop recording"));

  omc_macros[12].msg=g_strdup("/record/toggle");
  omc_macros[12].macro_text=g_strdup(_("Toggle recording state"));

  omc_macros[13].msg=g_strdup("/clip/foreground/background/swap");
  omc_macros[13].macro_text=g_strdup(_("Swap foreground and background clips"));
  omc_macros[14].msg=g_strdup("/effect_key/reset");
  omc_macros[14].macro_text=g_strdup(_("Reset effect keys"));
  omc_macros[14].info_text=g_strdup(_("Switch all effects off."));

  omc_macros[15].msg=g_strdup("/effect_key/enable");
  omc_macros[15].macro_text=g_strdup(_("Enable effect key <key>"));
  omc_macros[15].nparams=1;

  omc_macros[16].msg=g_strdup("/effect_key/disable");
  omc_macros[16].macro_text=g_strdup(_("Disable effect key <key>"));
  omc_macros[16].nparams=1;

  omc_macros[17].msg=g_strdup("/effect_key/toggle");
  omc_macros[17].macro_text=g_strdup(_("Toggle effect key <key>"));
  omc_macros[17].nparams=1;

  omc_macros[18].msg=g_strdup("/effect_key/nparameter/value/set");
  omc_macros[18].macro_text=g_strdup(_("Set parameter value <key> <pnum> = <value>"));
  omc_macros[18].info_text=g_strdup(_("Set <value> of pth (numerical) parameter for effect key <key>."));
  omc_macros[18].nparams=3;

  omc_macros[19].msg=g_strdup("/clip/select/next");
  omc_macros[19].macro_text=g_strdup(_("Switch foreground to next clip"));

  omc_macros[20].msg=g_strdup("/clip/select/previous");
  omc_macros[20].macro_text=g_strdup(_("Switch foreground to previous clip"));

  omc_macros[21].msg=g_strdup("/video/fps/ratio/set");
  omc_macros[21].macro_text=g_strdup(_("Set video framerate to ratio <fps_ratio>"));
  omc_macros[21].info_text=g_strdup(_("Set framerate ratio of foreground clip to <float fps_ratio>"));
  omc_macros[21].nparams=1;
  
  omc_macros[22].msg=g_strdup("/clip/foreground/retrigger");
  omc_macros[22].macro_text=g_strdup(_("Retrigger clip <clipnum>"));
  omc_macros[22].info_text=g_strdup(_("Switch foreground clip to the nth valid clip, and reset the frame number"));
  omc_macros[22].nparams=1;


  for (i=0;i<N_OMC_MACROS;i++) {
    if (omc_macros[i].msg!=NULL) {
      if (omc_macros[i].nparams>0) {
	omc_macros[i].ptypes=(gint *)g_malloc(omc_macros[i].nparams*sizint);
	omc_macros[i].mini=(gint *)g_malloc(omc_macros[i].nparams*sizint);
	omc_macros[i].maxi=(gint *)g_malloc(omc_macros[i].nparams*sizint);
	omc_macros[i].vali=(gint *)g_malloc(omc_macros[i].nparams*sizint);

	omc_macros[i].mind=(gdouble *)g_malloc(omc_macros[i].nparams*sizdbl);
	omc_macros[i].maxd=(gdouble *)g_malloc(omc_macros[i].nparams*sizdbl);
	omc_macros[i].vald=(gdouble *)g_malloc(omc_macros[i].nparams*sizdbl);
	omc_macros[i].pname=(gchar **)g_malloc(omc_macros[i].nparams*sizeof(gchar *));

      }
    }
  }


  // clip select
  omc_macros[2].ptypes[0]=OMC_PARAM_INT;
  omc_macros[2].mini[0]=omc_macros[2].vali[0]=1;
  omc_macros[2].maxi[0]=100000;
  omc_macros[2].pname[0]=g_strdup(_("clipnum")); // translators - short form of "clip number"


  // set fps (will be handled to avoid 0.)
  omc_macros[9].ptypes[0]=OMC_PARAM_DOUBLE;
  omc_macros[9].mind[0]=-200.;
  omc_macros[9].vald[0]=25.;
  omc_macros[9].maxd[0]=200.;
  omc_macros[9].pname[0]=g_strdup(_("fps")); // translators - short form of "frames per second"

  // effect_key enable,disable, toggle
  omc_macros[15].ptypes[0]=OMC_PARAM_INT;
  omc_macros[15].mini[0]=1;
  omc_macros[15].vali[0]=1;
  omc_macros[15].maxi[0]=prefs->rte_keys_virtual;
  omc_macros[15].pname[0]=g_strdup(_("key")); // translators - as in keyboard key

  omc_macros[16].ptypes[0]=OMC_PARAM_INT;
  omc_macros[16].mini[0]=1;
  omc_macros[16].vali[0]=1;
  omc_macros[16].maxi[0]=prefs->rte_keys_virtual;
  omc_macros[16].pname[0]=g_strdup(_("key")); // translators - as in keyboard key

  omc_macros[17].ptypes[0]=OMC_PARAM_INT;
  omc_macros[17].mini[0]=1;
  omc_macros[17].vali[0]=1;
  omc_macros[17].maxi[0]=prefs->rte_keys_virtual;
  omc_macros[17].pname[0]=g_strdup(_("key")); // translators - as in keyboard key

  // key
  omc_macros[18].ptypes[0]=OMC_PARAM_INT;
  omc_macros[18].mini[0]=1;
  omc_macros[18].vali[0]=1;
  omc_macros[18].maxi[0]=prefs->rte_keys_virtual;
  omc_macros[18].pname[0]=g_strdup(_("key")); // translators - as in keyboard key

  // param (this will be matched with numeric params)
  omc_macros[18].ptypes[1]=OMC_PARAM_INT;
  omc_macros[18].mini[1]=0;
  omc_macros[18].maxi[1]=32;
  omc_macros[18].vali[1]=0;
  omc_macros[18].pname[1]=g_strdup(_("pnum")); // translators - short form of "parameter number"

  // value (this will get special handling)
  // type conversion and auto offset/scaling will be done
  omc_macros[18].ptypes[2]=OMC_PARAM_DOUBLE;
  omc_macros[18].mind[2]=0.;
  omc_macros[18].maxd[2]=0.;
  omc_macros[18].vald[2]=0.;
  omc_macros[18].pname[2]=g_strdup(_("value"));

  // set ratio fps (will be handled to avoid 0.)
  omc_macros[21].ptypes[0]=OMC_PARAM_DOUBLE;
  omc_macros[21].mind[0]=-10.;
  omc_macros[21].vald[0]=1.;
  omc_macros[21].maxd[0]=10.;
  omc_macros[21].pname[0]=g_strdup(_("fps_ratio")); // translators - fps short form of "frames per second"


  // clip retrigger 
  omc_macros[22].ptypes[0]=OMC_PARAM_INT;
  omc_macros[22].mini[0]=omc_macros[22].vali[0]=1;
  omc_macros[22].maxi[0]=100000;
  omc_macros[22].pname[0]=g_strdup(_("clipnum")); // translators - short form of "clip number"

}


static int get_nfixed(gint type, gchar *string) {
  int nfixed=0;

  switch (type) {
  case OMC_JS_BUTTON:
    nfixed=3; // type, index, value
    break;
  case OMC_JS_AXIS:
    nfixed=2;  // type, index
    break;
#ifdef OMC_MIDI_IMPL
  case OMC_MIDI:
    type=midi_msg_type(string);
    return get_nfixed(type,NULL);
  case OMC_MIDI_CONTROLLER:
    nfixed=3;     // type, channel, cnum
    break;
  case OMC_MIDI_NOTE:
  case OMC_MIDI_NOTE_OFF:
  case OMC_MIDI_PITCH_BEND:
    nfixed=2; // type, channel
    break;
#endif
  }
  return nfixed;
}



static gboolean match_filtered_params(lives_omc_match_node_t *mnode, gchar *sig, int nfixed) {
  int i;
  gchar **array=g_strsplit(sig," ",-1);

  for (i=0;i<mnode->nvars;i++) {
    if (mnode->matchp[i]) {
      if (mnode->matchi[i]!=atoi(array[nfixed+i])) {
	//g_print("data mismatch %d %d %d\n",mnode->matchi[i],atoi(array[nfixed+i]),nfixed);
	g_strfreev(array);
	return FALSE;
      }
    }
  }
  //g_print("data match\n");
  g_strfreev(array);
  return TRUE;
}




static lives_omc_match_node_t *omc_match_sig(gint type, gint index, gchar *sig) {
  GSList *nlist=omc_node_list;
  gchar *srch;
  lives_omc_match_node_t *cnode;
  int nfixed;

  if (type==OMC_MIDI) {
    if (index==-1) srch=g_strdup_printf("%d %s",type,sig);
    else srch=g_strdup_printf("%d %d %s",type,index,sig);
  }
  else srch=g_strdup(sig);

  nfixed=get_nfixed(type,sig);

  while (nlist!=NULL) {
    cnode=(lives_omc_match_node_t *)nlist->data;
    //g_print("cf %s and %s\n",cnode->srch,srch);
    if (!strncmp(cnode->srch,srch,strlen(cnode->srch))) {
      // got a possible match
      // now check the data
      if (match_filtered_params(cnode,sig,nfixed)) {
	g_free(srch);
	return cnode;
      }
    }
    nlist=nlist->next;
  }
  g_free(srch);
  return NULL;
}



static gchar *omclearn_request_min(gint type) {
  gchar *msg=NULL;

  switch (type) {
  case OMC_JS_AXIS:
    msg=g_strdup(_("\n\nNow move the stick to the opposite position and click OK\n\n"));
    break;
  case OMC_MIDI_CONTROLLER:
    msg=g_strdup(_("\n\nPlease set the control to its minimum value and click OK\n\n"));
    break;
  case OMC_MIDI_NOTE:
    msg=g_strdup(_("\n\nPlease release the note\n\n"));
    break;
  }
  
  do_blocking_error_dialog(msg);
  if (msg!=NULL) g_free(msg);


  // now we need to re-read our source

  //return omc_source_get(type);

  return NULL;
}



inline static gint omclearn_get_fixed_elems(gchar *string1, gchar *string2) {
  // count how many (non-space) elements match
  // e.g "a b c" and "a b d" returns 2

  // neither string may end in a space

  register int i;

  gint match=0;
  gint stlen=MIN(strlen(string1),strlen(string2));

  for (i=0;i<stlen;i++) {
    if (strcmp((string1+i),(string2+i))) return match;
    if (!strcmp((string1+i)," ")) match++;
  }

  return match+1;

}



static inline gint get_nth_elem(gchar *string, gint idx) {
  gchar **array=g_strsplit(string," ",-1);
  gint retval=atoi(array[idx]);
  g_strfreev(array);
  return retval;
}





static lives_omc_match_node_t *lives_omc_match_node_new(gint str_type, gint index, gchar *string, gint nfixed) {
  int i;
  gchar *tmp;
  gchar *srch_str;
  lives_omc_match_node_t *mnode=(lives_omc_match_node_t *)g_malloc(sizeof(lives_omc_match_node_t));

  if (str_type==OMC_MIDI) {
    if (index>-1) srch_str=g_strdup_printf("%d %d %s",str_type,index,(tmp=cut_string_elems(string,nfixed<0?-1:nfixed)));
    else srch_str=g_strdup_printf("%d %s",str_type,(tmp=cut_string_elems(string,nfixed<0?-1:nfixed)));

    g_free(tmp);
  }
  else srch_str=g_strdup(string);

  //g_print("astoring %d:%s\n",str_type,string);
  //g_print("storing %s\n",srch_str);

  mnode->srch=srch_str;
  mnode->macro=-1;

  if (nfixed<0) mnode->nvars=-(nfixed+1);
  else mnode->nvars=get_token_count(string,' ')-nfixed;

  if (mnode->nvars>0) {
    mnode->offs0=(gint *)g_malloc(mnode->nvars*sizint);
    mnode->scale=(gdouble *)g_malloc(mnode->nvars*sizdbl);
    mnode->offs1=(gint *)g_malloc(mnode->nvars*sizint);
    mnode->min=(gint *)g_malloc(mnode->nvars*sizint);
    mnode->max=(gint *)g_malloc(mnode->nvars*sizint);
    mnode->matchp=(gboolean *)g_malloc(mnode->nvars*sizeof(gboolean));
    mnode->matchi=(gint *)g_malloc(mnode->nvars*sizint);
  }

  for (i=0;i<mnode->nvars;i++) {
    mnode->offs0[i]=mnode->offs1[i]=0;
    mnode->scale[i]=1.;
    mnode->matchp[i]=FALSE;
  }

  mnode->map=mnode->fvali=NULL;
  mnode->fvald=NULL;

  mnode->treev1=mnode->treev2=NULL;
  mnode->gtkstore=mnode->gtkstore2=NULL;

  return mnode;
}




static gint *omclearn_get_values(gchar *string, gint nfixed) {
  register int i,j;
  size_t slen,tslen;
  gint *retvals,count=0,nvars;

  slen=strlen(string);

  nvars=get_token_count(string,' ')-nfixed;

  retvals=(gint *)g_malloc(nvars*sizint);

  for (i=0;i<slen;i++) {
    if (!strncmp((string+i)," ",1)) {
      if (--nfixed<=0) {
	gchar *tmp=g_strdup(string+i+1);
	tslen=strlen(tmp);
	for (j=0;j<tslen;j++) {
	  if (!strncmp((tmp+j)," ",1)) {
	    memset(tmp+j,0,1);
	    retvals[count++]=atoi(tmp);
	    g_free(tmp);
	    break;
	  }
	}
	if (j==tslen) {
	  retvals[count++]=atoi(tmp);
	  g_free(tmp);
	  return retvals;
	}
	i+=j;
      }
    }
  }

  // should never reach here
  return NULL;
}




void omclearn_match_control (lives_omc_match_node_t *mnode, gint str_type, gint index, gchar *string, gint nfixed) {

  if (nfixed==-1) {
    // already there : allow user to update
    return;
  }

  if (index==-1) {
    index=get_nth_elem(string,1);
  }

  // add descriptive text on left
  // add combo box on right

  omc_learner_add_row(str_type,index,mnode,string);


}





lives_omc_match_node_t *omc_learn(gchar *string, gint str_type, gint idx) {
  // here we come with a string, which must be a sequence of integers
  // separated by single spaces

  // the str_type is one of JS_AXIS, JS_BUTTON, MIDI_CONTROLLER, MIDI_KEY, etc.

  // idx is -1, excpet for JS_BUTTON and JS_AXIS where it can be used

  // the string is first transformed into
  // signifier and value

  // next, we check if signifier is already matched to a macro

  // if not we allow the user to match it to any macro that has n or less parameters, where n is the number of variables in string 


  lives_omc_match_node_t *mnode;

  gint nfixed=get_nfixed(str_type,string);


  switch (str_type) {
  case OMC_MIDI_CONTROLLER:
    // display controller and allow it to be matched
    // then request min
    
    mnode=omc_match_sig(OMC_MIDI,idx,string);
    //g_print("autoscale !\n");
    
    if (mnode==NULL||mnode->macro==-1) {
      mnode=lives_omc_match_node_new(OMC_MIDI,idx,string,nfixed);
      mnode->max[0]=127;
      mnode->min[0]=0;
      idx=midi_index(string);
      omclearn_match_control(mnode,str_type,idx,string,nfixed);
      return mnode;
    }
    break;
  case OMC_MIDI_PITCH_BEND:
    // display controller and allow it to be matched
    // then request min
    
    mnode=omc_match_sig(OMC_MIDI,idx,string);
    //g_print("autoscale !\n");
    
    if (mnode==NULL||mnode->macro==-1) {
      mnode=lives_omc_match_node_new(OMC_MIDI,idx,string,-2);
      mnode->max[0]=16384;
      mnode->min[0]=0;
      omclearn_match_control(mnode,str_type,idx,string,nfixed);
      return mnode;
    }
    break;
  case OMC_MIDI_NOTE:
  case OMC_MIDI_NOTE_OFF:
    // display note and allow it to be matched
    mnode=omc_match_sig(OMC_MIDI,idx,string);
    
    if (mnode==NULL||mnode->macro==-1) {
      mnode=lives_omc_match_node_new(OMC_MIDI,idx,string,nfixed);

      mnode->max[0]=255;
      mnode->min[0]=0;

      mnode->max[1]=127;
      mnode->min[1]=0;

      omclearn_match_control(mnode,str_type,idx,string,nfixed);

      return mnode;
    }
    break;
  case OMC_JS_AXIS:
    // display axis and allow it to be matched
    // then request min
    
    mnode=omc_match_sig(str_type,idx,string);
    
    if (mnode==NULL||mnode->macro==-1) {
      mnode=lives_omc_match_node_new(str_type,idx,string,nfixed);

      mnode->min[0]=-128;
      mnode->max[0]=128;

      omclearn_match_control(mnode,str_type,idx,string,nfixed);
      return mnode;
    }
    break;
  case OMC_JS_BUTTON:
    // display note and allow it to be matched
    nfixed=3;
    mnode=omc_match_sig(str_type,idx,string);
     
    if (mnode==NULL||mnode->macro==-1) {
      mnode=lives_omc_match_node_new(str_type,idx,string,nfixed);
      omclearn_match_control(mnode,str_type,idx,string,nfixed);
      return mnode;
    }
    break;
  default:
    // hmmm....
    
    break;
  }
  return NULL;
}







void omc_process_string(gint supertype, gchar *string, gboolean learn) {
  int type=0,idx=-1;
  lives_omc_match_node_t *mnode;

  if (string==NULL)  return;

  switch (supertype) {
  case OMC_JS:
#ifdef OMC_JS_IMPL
    supertype=type=js_msg_type(string);
    idx=js_index(string);
#endif
    break;
#ifdef OMC_MIDI_IMPL
  case OMC_MIDI:
    type=midi_msg_type(string);
    //idx=midi_index(string);
    idx=-1;
#endif
  }
  if (type>0) {
    if (learn) {
      // pass to learner
      mnode=omc_learn(string,type,idx);
      if (mnode!=NULL) omc_node_list=g_slist_append(omc_node_list,mnode);
    }
    else {
      OSCbuf *oscbuf=omc_learner_decode(supertype,idx,string);
      //g_print("decode str %s\n",string);
      if (oscbuf!=NULL) lives_osc_act(oscbuf);
    }
  }
  
}





void on_midi_learn_activate (GtkMenuItem *menuitem, gpointer user_data) {
  GtkWidget *omclearn_dialog=create_omclearn_dialog();
  gchar *string=NULL;

  if (!omc_macros_inited) {
    init_omc_macros();
    omc_macros_inited=TRUE;
    OSC_initBuffer(&obuf,OSC_BUF_SIZE,byarr);
  }

#ifdef OMC_MIDI_IMPL
  if (!mainw->ext_cntl[EXT_CNTL_MIDI]) midi_open();
#endif

#ifdef OMC_JS_IMPL
  if (!mainw->ext_cntl[EXT_CNTL_JS]) js_open();
#endif

  gtk_widget_show(omclearn_dialog);

  mainw->cancelled=CANCEL_NONE;

  show_existing();

  // read controls and notes
  while (mainw->cancelled==CANCEL_NONE) {
    // read from devices

#ifdef OMC_JS_IMPL
    if (mainw->ext_cntl[EXT_CNTL_JS]) string=js_mangle();
    if (string!=NULL) {
      omc_process_string(OMC_JS,string,TRUE);
      g_free(string);
      string=NULL;
    }
    else {
#endif

#ifdef OMC_MIDI_IMPL
      if (mainw->ext_cntl[EXT_CNTL_MIDI]) string=midi_mangle();
      if (string!=NULL) {
	omc_process_string(OMC_MIDI,string,TRUE);
	g_free(string);
	string=NULL;
      }
#endif

#ifdef OMC_JS_IMPL
  }
#endif

    g_usleep(prefs->sleep_time);

    while (g_main_context_iteration(NULL,FALSE));
  }


  gtk_widget_destroy(omclearn_dialog);

  remove_unused_nodes();

  mainw->cancelled=CANCEL_NONE;

}




OSCbuf *omc_learner_decode(gint type, gint idx, gchar *string) {
  gint macro,nfixed;
  lives_omc_match_node_t *mnode;
  lives_omc_macro_t omacro;
  int oval0=1,oval1=0;

  int i,j,k;

  int *vals;

  gchar typetags[OSC_MAX_TYPETAGS];

  mnode=omc_match_sig(type,idx,string);

  if (mnode==NULL) return NULL;

  macro=mnode->macro;

  if (macro==-1) return NULL;

  omacro=omc_macros[macro];

  if (omacro.msg==NULL) return NULL;

  OSC_resetBuffer(&obuf);

  g_snprintf(typetags,64,",");

  // get typetags
  for (i=0;i<omacro.nparams;i++) {
    if (omacro.ptypes[i]==OMC_PARAM_INT) g_strappend(typetags,OSC_MAX_TYPETAGS,"i");
    else g_strappend(typetags,OSC_MAX_TYPETAGS,"f");
  }

  OSC_writeAddressAndTypes(&obuf,omacro.msg,typetags);

  nfixed=get_token_count(string,' ')-mnode->nvars;

  if (omacro.nparams>0) {

    vals=omclearn_get_values(string,nfixed);

    for (i=0;i<omacro.nparams;i++) {
      // get fixed val or map from
      j=mnode->map[i];

      if (j>-1) {
	if (macro==18&&i==2) {
	  // auto scale for fx param
	  int error,ntmpls,hint,flags;
	  gint mode=rte_key_getmode(oval0);
	  weed_plant_t *filter;
	  weed_plant_t **ptmpls;
	  weed_plant_t *ptmpl;

	  if (mode==-1) return NULL;

	  filter=rte_keymode_get_filter(oval0,mode);

	  ntmpls=weed_leaf_num_elements(filter,"in_parameter_templates");

	  ptmpls=weed_get_plantptr_array(filter,"in_parameter_templates",&error);
	  for (k=0;k<ntmpls;k++) {
	    ptmpl=ptmpls[k];
	    hint=weed_get_int_value(ptmpl,"hint",&error);
	    flags=weed_get_int_value(ptmpl,"flags",&error);
	    if ((hint==WEED_HINT_INTEGER||hint==WEED_HINT_FLOAT)&&flags==0&&weed_leaf_num_elements(ptmpl,"default")==1) {
	      if (oval1==0) {
		if (hint==WEED_HINT_INTEGER) {
		  int omin=mnode->min[j];
		  int omax=mnode->max[j];
		  int mini=weed_get_int_value(ptmpl,"min",&error);
		  int maxi=weed_get_int_value(ptmpl,"max",&error);

		  int oval=(int)((double)(vals[j]-omin)/(double)(omax-omin)*(double)(maxi-mini))+mini;
		  OSC_writeIntArg(&obuf,oval);
		}
		else {
		  // float
		  int omin=mnode->min[j];
		  int omax=mnode->max[j];
		  double minf=weed_get_double_value(ptmpl,"min",&error);
		  double maxf=weed_get_double_value(ptmpl,"max",&error);

		  double oval=(double)(vals[j]-omin)/(double)(omax-omin)*(maxf-minf)+minf;

		  OSC_writeFloatArg(&obuf,(float)oval);
		} // end float
	      }
	      oval1--;
	    }
	  }
	  weed_free(ptmpls);
	}
	else {
	  if (omacro.ptypes[i]==OMC_PARAM_INT) {
	    int oval=myround((double)(vals[j]+mnode->offs0[j])*mnode->scale[j])+mnode->offs1[j];
	    if (i==0) oval0=oval;
	    if (i==1) oval1=oval;
	    OSC_writeIntArg(&obuf,oval);
	  }
	  else {
	    double oval=(double)(vals[j]+mnode->offs0[j])*mnode->scale[j]+(double)mnode->offs1[j];
	    OSC_writeFloatArg(&obuf,oval);
	  }
	}
      }
      else {
	if (omacro.ptypes[i]==OMC_PARAM_INT) {
	  OSC_writeIntArg(&obuf,mnode->fvali[i]);
	  if (i==0) oval0=mnode->fvali[i];
	  if (i==1) oval1=mnode->fvali[i];
	}
	else {
	  OSC_writeFloatArg(&obuf,(float)mnode->fvald[i]);
	}
      }
    }

    g_free(vals);
  }

  return &obuf;
}






/////////////////////////////////////
void on_midi_save_activate (GtkMenuItem *menuitem, gpointer user_data) {
  gchar *save_file=choose_file(NULL,NULL,NULL,GTK_FILE_CHOOSER_ACTION_SAVE,NULL);
  int fd;
  GSList *slist=omc_node_list;
  size_t srchlen;
  lives_omc_match_node_t *mnode;
  lives_omc_macro_t omacro;
  int nnodes;
  int i;
  gchar *msg;

  if (!(fd=open(save_file,O_WRONLY|O_CREAT|O_TRUNC,S_IRUSR|S_IWUSR))) {
    msg=g_strdup_printf (_("\n\nUnable to open file\n%s\nError code %d\n"),save_file,errno);
    do_blocking_error_dialog (msg);
    g_free (msg);
    g_free (save_file);
    d_print_failed();
    return;
  }

  msg=g_strdup_printf(_("Saving device mapping to file %s..."),save_file);
  d_print(msg);
  g_free(msg);

  g_free (save_file);

  dummyvar=write(fd,OMC_FILE_VSTRING,strlen(OMC_FILE_VSTRING));

  nnodes=g_slist_length(omc_node_list);
  dummyvar=write(fd,&nnodes,sizint);

  while (slist!=NULL) {
    mnode=(lives_omc_match_node_t *)slist->data;
    srchlen=strlen(mnode->srch);

    dummyvar=write(fd,&srchlen,sizint);
    dummyvar=write(fd,mnode->srch,srchlen);

    dummyvar=write(fd,&mnode->macro,sizint);
    dummyvar=write(fd,&mnode->nvars,sizint);
    
    for (i=0;i<mnode->nvars;i++) {
      dummyvar=write(fd,&mnode->offs0[i],sizint);
      dummyvar=write(fd,&mnode->scale[i],sizdbl);
      dummyvar=write(fd,&mnode->offs1[i],sizint);
      
      dummyvar=write(fd,&mnode->min[i],sizint);
      dummyvar=write(fd,&mnode->max[i],sizint);

      dummyvar=write(fd,&mnode->matchp[i],sizint);
      dummyvar=write(fd,&mnode->matchi[i],sizint);
    }

    omacro=omc_macros[mnode->macro];

    for (i=0;i<omacro.nparams;i++) {
      dummyvar=write(fd,&mnode->map[i],sizint);
      dummyvar=write(fd,&mnode->fvali[i],sizint);
      dummyvar=write(fd,&mnode->fvald[i],sizdbl);
    }
    slist=slist->next;
  }
  close (fd);
  d_print_done();

}


static void omc_node_list_free(GSList *slist) {
  while (slist!=NULL) {
    omc_match_node_free(slist->data);
    slist=slist->next;
  }
  g_slist_free(slist);
  slist=NULL;
}


static void do_midi_load_error(gchar *fname) {
  gchar *msg=g_strdup_printf (_("\n\nError parsing file\n%s\n"),fname);
  do_blocking_error_dialog (msg);
  g_free (msg);
  d_print_failed();
}

static void do_midi_version_error(gchar *fname) {
  gchar *msg=g_strdup_printf (_("\n\nInvalid version in file\n%s\n"),fname);
  do_blocking_error_dialog (msg);
  g_free (msg);
  d_print_failed();
}




void on_midi_load_activate (GtkMenuItem *menuitem, gpointer user_data) {
  gchar *load_file=choose_file(NULL,NULL,NULL,GTK_FILE_CHOOSER_ACTION_OPEN,NULL);
  int fd;
  size_t bytes,srchlen;
  lives_omc_match_node_t *mnode;
  lives_omc_macro_t omacro;
  gchar tstring[512];
  int nnodes,i,macro,nvars,type=0,supertype,j,idx=-1;
  gchar *srch;
  gchar *msg;

#ifdef OMC_MIDI_IMPL
  size_t blen;
  gchar *tmp;
#endif 

  if (!(fd=open(load_file,O_RDONLY))) {
    msg=g_strdup_printf (_("\n\nUnable to open file\n%s\nError code %d\n"),load_file,errno);
    do_blocking_error_dialog (msg);
    g_free (msg);
    g_free (load_file);
    d_print_failed();
    return;
  }

  msg=g_strdup_printf(_("Loading device mapping from file %s..."),load_file);
  d_print(msg);
  g_free(msg);

  if (!omc_macros_inited) {
    init_omc_macros();
    omc_macros_inited=TRUE;
    OSC_initBuffer(&obuf,OSC_BUF_SIZE,byarr);
  }

  bytes=read(fd,tstring,strlen(OMC_FILE_VSTRING));
  if (bytes<strlen(OMC_FILE_VSTRING)) {
    do_midi_load_error(load_file);
    g_free (load_file);
    return;
  }

  if (strncmp(tstring,OMC_FILE_VSTRING,strlen(OMC_FILE_VSTRING))) {
    do_midi_version_error(load_file);
    g_free (load_file);
    return;
  }

  bytes=read(fd,&nnodes,sizint);
  if (bytes<sizint) {
    do_midi_load_error(load_file);
    g_free (load_file);
    return;
  }

  if (omc_node_list!=NULL) {
    omc_node_list_free(omc_node_list);
    omc_node_list=NULL;
  }

  for (i=0;i<nnodes;i++) {

    bytes=read(fd,&srchlen,sizint);
    if (bytes<sizint) {
      do_midi_load_error(load_file);
      g_free (load_file);
      return;
    }
    
    srch=g_malloc(srchlen+1);

    bytes=read(fd,srch,srchlen);
    if (bytes<srchlen) {
      do_midi_load_error(load_file);
      g_free (load_file);
      return;
    }

    memset(srch+srchlen,0,1);

    bytes=read(fd,&macro,sizint);
    if (bytes<sizint) {
      do_midi_load_error(load_file);
      g_free (load_file);
      g_free(srch);
      return;
    }
    
    bytes=read(fd,&nvars,sizint);
    if (bytes<sizint) {
      do_midi_load_error(load_file);
      g_free (load_file);
      g_free(srch);
      return;
    }
    
    supertype=atoi(srch);
    
    switch (supertype) {
#ifdef OMC_JS_IMPL
    case OMC_JS:
      supertype=js_msg_type(srch);
    case OMC_JS_BUTTON:
    case OMC_JS_AXIS:
      type=supertype;
      idx=js_index(srch);
      break;
#endif
#ifdef OMC_MIDI_IMPL
    case OMC_MIDI:
      type=midi_msg_type(srch);
      idx=-1;

      // cut first value (supertype) as we will be added back in match_node_new
      tmp=cut_string_elems(srch,1);
      blen=strlen(tmp);
      tmp=g_strdup(srch+blen+1);
      g_free(srch);
      srch=tmp;

      break;
#endif
    default:
      return;
    }

    mnode=lives_omc_match_node_new(supertype,idx,srch,-(nvars+1));
    g_free(srch);

    mnode->macro=macro;

    for (j=0;j<nvars;j++) {
      bytes=read(fd,&mnode->offs0[j],sizint);
      if (bytes<sizint) {
	do_midi_load_error(load_file);
	g_free (load_file);
	return;
      }
      bytes=read(fd,&mnode->scale[j],sizdbl);
      if (bytes<sizdbl) {
	do_midi_load_error(load_file);
	g_free (load_file);
	return;
      }
      bytes=read(fd,&mnode->offs1[j],sizint);
      if (bytes<sizint) {
	do_midi_load_error(load_file);
	g_free (load_file);
	return;
      }
      
      bytes=read(fd,&mnode->min[j],sizint);
      if (bytes<sizint) {
	do_midi_load_error(load_file);
	g_free (load_file);
	return;
      }
      bytes=read(fd,&mnode->max[j],sizint);
      if (bytes<sizint) {
	do_midi_load_error(load_file);
	return;
      }
      
      bytes=read(fd,&mnode->matchp[j],sizint);
      if (bytes<sizint) {
	do_midi_load_error(load_file);
	g_free (load_file);
	return;
      }
      bytes=read(fd,&mnode->matchi[j],sizint);
      if (bytes<sizint) {
	do_midi_load_error(load_file);
	g_free (load_file);
	return;
      }
    }

    omacro=omc_macros[macro];

    mnode->map=(gint *)g_malloc(omacro.nparams*sizint);
    mnode->fvali=(gint *)g_malloc(omacro.nparams*sizint);
    mnode->fvald=(gdouble *)g_malloc(omacro.nparams*sizdbl);

    for (j=0;j<omacro.nparams;j++) {
      bytes=read(fd,&mnode->map[j],sizint);
      if (bytes<sizint) {
	do_midi_load_error(load_file);
	g_free (load_file);
	return;
      }
      bytes=read(fd,&mnode->fvali[j],sizint);
      if (bytes<sizint) {
	do_midi_load_error(load_file);
	g_free (load_file);
	return;
      }
      bytes=read(fd,&mnode->fvald[j],sizdbl);
      if (bytes<sizdbl) {
	do_midi_load_error(load_file);
	g_free (load_file);
	return;
      }
    }
    omc_node_list=g_slist_append(omc_node_list,(gpointer)mnode);
  }

  close (fd);
  d_print_done();

#ifdef OMC_MIDI_IMPL
  if (!mainw->ext_cntl[EXT_CNTL_MIDI]) midi_open();
#endif

#ifdef OMC_JS_IMPL
  if (!mainw->ext_cntl[EXT_CNTL_JS]) js_open();
#endif

}
