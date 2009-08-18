// mainwindow.h
// LiVES (lives-exe)
// (c) G. Finch <salsaman@xs4all.nl> 2003 - 2009
// Released under the GPL 3 or later
// see file ../COPYING for licensing details


#ifndef _HAS_MAINWINDOW_H_
#define _HAS_MAINWINDOW_H_

#include <pthread.h>

#ifdef ALSA_MIDI
#include <alsa/asoundlib.h>
#endif

// hardware related prefs

// fraction of a second quantisation for event timing; these two must match, and must be multiples of 10>=1000000 !
// >10**8 is not recommended, since we sometimes store seconds in a gdouble
#define U_SEC 100000000.
#define U_SECL (gint64)100000000
#define U_SEC_RATIO (U_SECL/1000000) // how many U_SECs (ticks) in a microsecond

// parameters for resizing the image frames, and for capture
// TODO - make prefs
#define V_RESIZE_ADJUST 36
#define H_RESIZE_ADJUST 22

// vert displacement up from center for sepwin (actual value is half this)
#define SEPWIN_VADJUST 200

// number of function keys
#define FN_KEYS 12

// FX keys, 1 - 9 normally
#define FX_KEYS_PHYSICAL 9

// must be >= FX_KEYS_PHYSICAL, and <=64 (number of bits in a 64bit int mask)
// (max number of keys accesible through rte window or via OSC)
#define FX_KEYS_MAX_VIRTUAL 64

// the rest of the keys are accessible through the multitrack renderer (must, be > FX_KEYS_MAX_VIRTUAL)
#define FX_KEYS_MAX 65536


// maximum number of external control peripherals
#define MAX_EXT_CNTL 32

// external control types
#define EXT_CNTL_NONE 0 // not used
#define EXT_CNTL_JS 1
#define EXT_CNTL_MIDI 2



typedef struct {
  // set in set_palette_colours()
  gint style;
#define STYLE_PLAIN 0 // no theme (theme 'none')
#define STYLE_1 1<<0 // turn on theming if set
#define STYLE_2 1<<1 // colour the spinbuttons on the front page if set
#define STYLE_3 1<<2 // style is lightish - allow themeing of widgets with dark text, otherwise use menu bg
#define STYLE_4 1<<3 // coloured bg for poly window in mt
#define STYLE_5 1<<4 // drop down menu text col. in mt
#define STYLE_6 1<<4 // separator col. in mt

  GdkColor white;
  GdkColor black;
  GdkColor light_blue;
  GdkColor light_yellow;
  GdkColor pink;
  GdkColor light_red;
  GdkColor grey20;
  GdkColor grey25;
  GdkColor grey45;
  GdkColor grey60;
  GdkColor dark_orange;
  GdkColor fade_colour;
  GdkColor normal_back;
  GdkColor normal_fore;

  GdkColor menu_and_bars;
  GdkColor banner_fade_text;
  GdkColor info_text;
  GdkColor info_base;

  GdkColor bm_opaque;
  GdkColor bm_trans;

} _palette;


typedef struct {
  gint x;
  gint y;
  gint width;
  gint height;
  GdkScreen *screen;
} lives_mgeometry_t;


// where do we add the builtin tools in the tools menu
#define RFX_TOOL_MENU_POSN 2

// mainw->
typedef struct {
  gchar msg[512];

  // files
  gint current_file;
  gint first_free_file;
  file *files[MAX_FILES+1]; // +1 for the clipboard
  gchar vid_load_dir[256];
  gchar vid_save_dir[256];
  gchar audio_dir[256];
  gchar image_dir[256];
  gchar proj_load_dir[256];
  gchar proj_save_dir[256];
  gchar xmms_dir[256];
  gint untitled_number;
  gint cap_number;
  gint clips_available;

  // hash table of clips in menu order
  GList *cliplist;

  // sets
  gchar set_name[256];

  // playback
  gboolean faded;
  gboolean double_size;
  gboolean sep_win;
  gboolean fs;
  gboolean loop;
  gboolean loop_cont;
  gboolean ping_pong;
  gboolean mute;
  gboolean must_resize; // fixed playback size in gui; playback plugins have their own fwidth and fheight
  gint audio_start;
  gint audio_end;

  gboolean ext_playback; // using external video playback plugin
  gboolean ext_keyboard; // keyboard codes must be polled from video playback plugin

  gint ptr_x;
  gint ptr_y;

  gdouble fps_measure; // show fps stats after playback


  // flags
  gboolean save_with_sound;
  gboolean ccpd_with_sound;
  gboolean selwidth_locked;
  gboolean is_ready;
  gboolean opening_loc;  // opening location (streaming)
  gboolean dvgrab_preview;
  gboolean switch_during_pb;
  gboolean clip_switched; // for recording - did we switch clips ?
  gboolean record;

  gboolean in_fs_preview;
  volatile gint cancelled;

  // no cancel
#define CANCEL_NONE 0

  // user pressed stop
#define CANCEL_USER 1

  // cancel but keep opening
#define CANCEL_NO_PROPOGATE 2

  // effect processing finished during preview
#define CANCEL_PREVIEW_FINISHED 3

  // application quit
#define CANCEL_APP_QUIT 4

  // ran out of preview frames
#define CANCEL_NO_MORE_PREVIEW 5

  // image could not be captured
#define CANCEL_CAPTURE_ERROR 6

  // event_list completed
#define CANCEL_EVENT_LIST_END 7

  // video playback completed
#define CANCEL_VID_END 8

  // generator was stopped
#define CANCEL_GENERATOR_END 9

  // user pressed 'Keep'
#define CANCEL_KEEP 10

  // video playback completed
#define CANCEL_AUD_END 11

  // cancelled because of error
#define CANCEL_ERROR 12

  // cancelled and paused
#define CANCEL_USER_PAUSED 13

  // special cancel for TV toy
#define CANCEL_KEEP_LOOPING 100

  gboolean error;

  guint cancel_type;
#define CANCEL_KILL 0  // normal - kill background processes working on current clip
#define CANCEL_SOFT 1 // just cancel in GUI (for keep, etc)

  weed_plant_t *event_list;

  gshort endian;

  gint pwidth; // playback width in RGB pixels
  gint pheight; // playback height

  gshort whentostop;
  // which stream end should cause playback to finish ?
#define NEVER_STOP 0
#define STOP_ON_VID_END 1
#define STOP_ON_AUD_END 2

  gboolean noframedrop;

  gint play_start;
  gint play_end;
  gboolean playing_sel;
  gboolean preview;

  gboolean is_processing;
  gboolean is_rendering;
  gboolean resizing;

  gboolean foreign;  // for external window capture
  gboolean record_foreign;
  gboolean t_hidden;

  // recording from an external window
  guint foreign_key;
  unsigned int foreign_id;
  GdkColormap *foreign_cmap;
  GdkPixmap *foreign_map;
  gint foreign_width;
  gint foreign_height;
  gint foreign_bpp;

  // some VJ effects
  gboolean nervous;

  lives_rfx_t *rendered_fx;
  gint num_rendered_effects_builtin;
  gint num_rendered_effects_custom;
  gint num_rendered_effects_test;

  // for the merge dialog
  gint last_transition_idx;
  gint last_transition_loops;
  gboolean last_transition_loop_to_fit;
  gboolean last_transition_align_start;
  gboolean last_transition_ins_frames;

#define GU641 ((guint64)1)
  guint64 rte; // current max for VJ mode == 64 effects on fg clip

#define EFFECT_NONE 0

  guint last_grabable_effect;
  guint rte_keys; // which effect is bound to keyboard
  gint num_tr_applied; // number of transitions active
  gdouble blend_factor; // keyboard control parameter
  
  gint blend_file;
  gint last_blend_file;
  gint blend_file_step;

  gint scrap_file; // we throw odd sized frames here when recording in real time; used if a source is a generator or stream

  // which number file we are playing (or -1)
  gint playing_file;

  gint pre_src_file; // file we were editing before any ext input started

  gint scr_width;
  gint scr_height;
  gint toy_type;
#define TOY_NONE 0
#define TOY_RANDOM_FRAMES 1
#define TOY_TV 1
  gboolean toy_go_wild;

  // copy/paste
  gboolean insert_after;
  gboolean with_sound;

  // selection
  gint sel_start;
  gshort sel_move;

  // which bar should we move ?
#define SEL_MOVE_START 1
#define SEL_MOVE_END 2
#define SEL_MOVE_AUTO 3
#define SEL_MOVE_SINGLE 4

  // prefs (Save on exit)
  gint prefs_changed;
#define PREFS_THEME_CHANGED (1<<0)
#define PREFS_JACK_CHANGED (1<<1)
#define PREFS_TEMPDIR_CHANGED (1<<2)

  // default sizes for when no file is loaded
  gint def_width;
  gint def_height;

  // for the framedraw preview - TODO use lives_framedraw_t array
  gint framedraw_frame;


  /////////////////////////////////////////////////

  // end of static-ish info
  gboolean save_all;
  gchar first_info_file[256];
  gboolean leave_files;
  gboolean was_set;

  // extra parameters for opening special files
  gchar *file_open_params;
  gboolean open_deint;

  gint last_dprint_file;
  gboolean no_switch_dprint;

  // actual frame being displayed
  gint actual_frame;

  // and the audio 'frame' for when we are looping
  gdouble aframeno;

  // ticks are measured in 1/U_SEC of a second (by defalt a tick is 10 nano seconds)

  // for the internal player
  gdouble period; // == 1./cfile->pb_fps (unless cfile->pb_fps is 0.)
  gint64 startticks; // effective ticks when last frame was (should have been) displayed
  gint64 timeout_ticks; // incremented if effect/rendering is paused/previewed
  gint64 startsecs; // playback start seconds - subtracted from all other ticks to keep numbers smaller
  gint64 currticks; // current playback ticks : goes from origticks upwards
  gint64 deltaticks; // deltaticks for scratching
  gint64 firstticks; // ticks when audio started playing
  gint64 origticks; // ticks at start of playback
  gint64 stream_ticks;  // ticks since first frame sent to playback plugin

  gboolean size_warn; // warn the user that incorrectly sized frames were found
  gboolean noswitch; // set to TRUE during frame load/display operation. If TRUE we should not switch clips, 
                     // close the current clip, or call load_frame_image()
  gint new_clip;

  gboolean reverse_pb; 

  // TODO - make this a mutex and more finely grained : things we need to block are (clip switches, clip closure, effects on/off, etc)
  gboolean osc_block;

  gboolean osc_auto; // bypass user choices automatically

  // encode width and height set externally
  gint osc_enc_width;
  gint osc_enc_height;


  // fixed fps playback; usually fixed_fpsd==0.
  gint fixed_fps_numer;
  gint fixed_fps_denom;
  gdouble fixed_fpsd; // <=0. means free playback

  // video playback plugin was updated; write settings to a file
  gboolean write_vpp_file;

  gshort scratch;
#define SCRATCH_NONE 0
#define SCRATCH_BACK -1
#define SCRATCH_FWD 1

  // internal fx
  gboolean internal_messaging;
  gint (*progress_fn) (gboolean reset);

  volatile gboolean threaded_dialog;

  // fx controls
  gdouble fx1_val;
  gdouble fx2_val;
  gdouble fx3_val;
  gdouble fx4_val;
  gdouble fx5_val;
  gdouble fx6_val;

  gint fx1_start;
  gint fx2_start;
  gint fx3_start;
  gint fx4_start;

  gint fx1_step;
  gint fx2_step;
  gint fx3_step;
  gint fx4_step;

  gint fx1_end;
  gint fx2_end;
  gint fx3_end;
  gint fx4_end;

  gboolean fx1_bool;
  gboolean fx2_bool;
  gboolean fx3_bool;
  gboolean fx4_bool;
  gboolean fx5_bool;
  gboolean fx6_bool;

  gboolean effects_paused;
  gboolean did_rfx_preview;

  //function pointers
  guint kb_timer;
  gulong config_func;
  gulong pb_fps_func;
  gulong spin_start_func;
  gulong spin_end_func;
  gulong record_perf_func;
  gulong vidbar_func;
  gulong laudbar_func;
  gulong raudbar_func;
  gulong hrule_func;
  gulong toy_func_none;
  gulong toy_func_random_frames;
  gulong toy_func_lives_tv;
  gulong hnd_id;
  gulong loop_cont_func;
  gulong mute_audio_func;

  // for jack transport
  gboolean jack_can_stop;
  gboolean jack_can_start;

  gboolean video_seek_ready;

  // selection pointers
  gulong mouse_fn1;
  gboolean mouse_blocked;
  gboolean hrule_blocked;

  // stored clips
  gint clipstore[FN_KEYS-1];

  // GdkGC (graphics context)
  GdkGC *gc;

  // key function for autorepeat ctrl-arrows
  guint ksnoop;

  lives_mt *multitrack;

  gint new_blend_file;

  // Widgets  -- TODO - move into sub struct mainw->widgets->*
  GdkNativeWindow xwin;
  GtkTooltips *tooltips;
  GtkWidget *frame1;
  GtkWidget *frame2;
  GtkWidget *playframe;
  GdkPixbuf *imframe;
  GdkPixbuf *imsep;
  GtkWidget *LiVES;
  GtkWidget *save;
  GtkWidget *open;
  GtkWidget *open_sel;
  GtkWidget *open_vcd_menu;
  GtkWidget *open_vcd_submenu;
  GtkWidget *open_vcd;
  GtkWidget *open_dvd;
  GtkWidget *open_loc;
  GtkWidget *open_yuv4m;
  GtkWidget *open_lives2lives;
  GtkWidget *send_lives2lives;
  GtkWidget *open_device_menu;
  GtkWidget *open_device_submenu;
  GtkWidget *open_firewire;
  GtkWidget *open_hfirewire;
  GtkWidget *recent_menu;
  GtkWidget *recent_submenu;
  GtkWidget *recent1;
  GtkWidget *recent2;
  GtkWidget *recent3;
  GtkWidget *recent4;
  GtkWidget *save_as;
  GtkWidget *backup;
  GtkWidget *restore;
  GtkWidget *save_selection;
  GtkWidget *close;
  GtkWidget *import_proj;
  GtkWidget *export_proj;
  GtkWidget *sw_sound;
  GtkWidget *clear_ds;
  GtkWidget *ccpd_sound;
  GtkWidget *quit;
  GtkWidget *undo;
  GtkWidget *redo;
  GtkWidget *copy;
  GtkWidget *cut;
  GtkWidget *insert;
  GtkWidget *paste_as_new;
  GtkWidget *merge;
  GtkWidget *delete;
  GtkWidget *select_submenu;
  GtkWidget *select_all;
  GtkWidget *select_new;
  GtkWidget *select_to_end;
  GtkWidget *select_from_start;
  GtkWidget *select_start_only;
  GtkWidget *select_end_only;
  GtkWidget *select_last;
  GtkWidget *select_invert;
  GtkWidget *lock_selwidth;
  GtkWidget *record_perf;
  GtkWidget *playall;
  GtkWidget *playsel;
  GtkWidget *playclip;
  GtkWidget *rev_clipboard;
  GtkWidget *stop;
  GtkWidget *rewind;
  GtkWidget *full_screen;
  GtkWidget *loop_video;
  GtkWidget *loop_continue;
  GtkWidget *loop_ping_pong;
  GtkWidget *sepwin;
  GtkWidget *mute_audio;
  GtkWidget *sticky;
  GtkWidget *showfct;
  GtkWidget *fade;
  GtkWidget *dsize;

  GtkWidget *change_speed;
  GtkWidget *capture;
  GtkWidget *load_audio;
  GtkWidget *load_cdtrack;
  GtkWidget *eject_cd;
  GtkWidget *recaudio_submenu;
  GtkWidget *recaudio_clip;
  GtkWidget *recaudio_sel;
  GtkWidget *export_submenu;
  GtkWidget *export_allaudio;
  GtkWidget *export_selaudio;
  GtkWidget *append_audio;
  GtkWidget *trim_submenu;
  GtkWidget *trim_audio;
  GtkWidget *trim_to_pstart;
  GtkWidget *delaudio_submenu;
  GtkWidget *delsel_audio;
  GtkWidget *delall_audio;
  GtkWidget *ins_silence;
  GtkWidget *fade_aud_in;
  GtkWidget *fade_aud_out;
  GtkWidget *resample_audio;
  GtkWidget *resample_video;
  GtkWidget *preferences;
  GtkWidget *xmms_play_audio;
  GtkWidget *xmms_random_audio;
  GtkWidget *xmms_stop_audio;
  GtkWidget *rename;
  GtkWidget *toys;
  GtkWidget *toy_none;
  GtkWidget *toy_random_frames;
  GtkWidget *toy_tv;
  GtkWidget *show_file_info;
  GtkWidget *show_file_comments;
  GtkWidget *show_clipboard_info;
  GtkWidget *show_messages;
  GtkWidget *show_layout_errors;
  GtkWidget *sel_label;
  GtkAccelGroup *accel_group;
  GtkWidget *sep_image;
  GtkWidget *hruler;
  GtkWidget *vj_menu;
  GtkWidget *vj_save_set;
  GtkWidget *vj_load_set;
  GtkWidget *vj_show_keys;
  GtkWidget *rte_defs_menu;
  GtkWidget *rte_defs;
  GtkWidget *save_rte_defs;
  GtkWidget *vj_reset;
  GtkWidget *mt_menu;

  // for the fileselection preview
  GtkWidget *fs_playarea;
  GtkWidget *fs_playframe;

  // for the framedraw special widget - TODO - use a sub-struct
  GtkWidget *framedraw; // the eventbox
  GtkWidget *framedraw_reset; // the 'redraw' button
  GtkWidget *framedraw_preview; // the 'redraw' button
  GtkWidget *framedraw_spinbutton; // the frame number button
  GtkWidget *framedraw_scale; // the slider
  GtkWidget *framedraw_image; // the image
  GtkWidget *fd_frame; // surrounding frame widget

  GdkPixmap *framedraw_orig_pixmap; // the original frame pixmap
  GdkPixmap *framedraw_copy_pixmap; // the altered frame pixmap
  GdkBitmap *framedraw_bitmap; // and its mask
  GdkGC *framedraw_bitmapgc;  // the GC for the bitmap
  GdkGC *framedraw_colourgc;  // the GC for the pixmap, we can draw in colours !

  // bars here -> actually text above bars
  GtkWidget *vidbar;
  GtkWidget *laudbar;
  GtkWidget *raudbar;

#define MAIN_SPIN_SPACER 76 // pixel spacing for start/end spins for clip and multitrack editors
  GtkWidget *spinbutton_end;
  GtkWidget *spinbutton_start;

  GtkWidget *arrow1;
  GtkWidget *arrow2;

  GdkCursor *cursor;

  weed_plant_t *filter_map;
  void ***pchains;

  // for the internal player
  gint fixed_height;
  GtkWidget *image274;
  GtkWidget *play_window;
  weed_plant_t *frame_layer;
  GtkWidget *plug1;
  gulong pw_exp_func;
  gboolean pw_exp_is_blocked;

  // frame preview in the separate window
  GtkWidget *preview_box;
  GtkWidget *preview_image;
  GtkWidget *preview_spinbutton;
  GtkWidget *preview_scale;
  gint preview_frame;
  gulong preview_spin_func;
  gint prv_link;
#define PRV_FREE 0
#define PRV_START 1
#define PRV_END 2
#define PRV_PTR 3

  GtkWidget *image272;
  GtkWidget *image273;
  GtkWidget *playarea;
  GtkWidget *hseparator;
  GtkWidget *scrolledwindow;
  GtkWidget *message_box;
  GtkWidget *warning_label;

  GtkWidget *textview1;
  GtkWidget *winmenu;
  GtkWidget *eventbox;
  GtkWidget *eventbox2;
  GtkWidget *eventbox3;
  GtkWidget *eventbox4;
  GtkWidget *eventbox5;

  // toolbar buttons
  GtkWidget *t_stopbutton;
  GtkWidget *t_bckground;
  GtkWidget *t_fullscreen;
  GtkWidget *t_sepwin;
  GtkWidget *t_double;
  GtkWidget *t_infobutton;

  GtkWidget *t_slower;
  GtkWidget *t_faster;
  GtkWidget *t_forward;
  GtkWidget *t_back;

  GtkWidget *t_hide;

  GtkWidget *toolbar;
  GtkWidget *tb_hbox;
  GtkWidget *fs1;
  GtkWidget *vbox1;

  GtkWidget *volume_scale;
  GtkWidget *vol_toolitem;
  GtkWidget *vol_label;

  // menubar buttons
  GtkWidget *btoolbar; // button toolbar - clip editor
  GtkWidget *m_sepwinbutton;
  GtkWidget *m_playbutton;
  GtkWidget *m_stopbutton;
  GtkWidget *m_playselbutton;
  GtkWidget *m_rewindbutton;
  GtkWidget *m_loopbutton;
  GtkWidget *m_mutebutton;
  GtkWidget *menu_hbox;
  GtkWidget *menubar;

  // separate window
  gint opwx;
  gint opwy;

  // sepwin buttons
  GtkWidget *p_playbutton;
  GtkWidget *p_playselbutton;
  GtkWidget *p_rewindbutton;
  GtkWidget *p_loopbutton;
  GtkWidget *p_mutebutton;
  GtkWidget *p_mute_img;

  // timer bars
  GtkWidget *video_draw;
  GdkPixmap *video_drawable;
  GtkWidget *laudio_draw;
  GdkPixmap *laudio_drawable;
  GtkWidget *raudio_draw;
  GdkPixmap *raudio_drawable;

  // framecounter
  GtkWidget *framebar;
  GtkWidget *framecounter;
  GtkWidget *spinbutton_pb_fps;
  GtkWidget *vps_label;
  GtkWidget *curf_label;
  GtkWidget *banner;

  // rendered effects
  GtkWidget *effects_menu;
  GtkWidget *tools_menu;
  GtkWidget *utilities_menu;
  GtkWidget *utilities_submenu;
  GtkWidget *gens_menu;
  GtkWidget *gens_submenu;
  GtkWidget *run_test_rfx_submenu;
  GtkWidget *run_test_rfx_menu;
  GtkWidget *custom_effects_menu;
  GtkWidget *custom_effects_submenu;
  GtkWidget *custom_effects_separator;
  GtkWidget *custom_tools_menu;
  GtkWidget *custom_tools_submenu;
  GtkWidget *custom_tools_separator;
  GtkWidget *custom_gens_menu;
  GtkWidget *custom_gens_submenu;
  GtkWidget *custom_utilities_menu;
  GtkWidget *custom_utilities_submenu;
  GtkWidget *custom_utilities_separator;
  GtkWidget *rte_separator;
  GtkWidget *invis;

  gint num_tracks;
  gint *clip_index;
  gint *frame_index;

  GtkWidget *resize_menuitem;

  gboolean only_close; // only close clips - do not exit

#ifdef ENABLE_JACK
  jack_driver_t *jackd; // jack audio playback device
  jack_driver_t *jackd_read; // jack audio recorder device
#endif


  // layouts
  GtkTextBuffer *layout_textbuffer; // stores layout errors
  GList *affected_layouts_map; // map of layouts with errors
  GList *current_layouts_map; // map of all layouts for set

  gchar *recovery_file;  // the filename of our recover file
  gboolean leave_recovery;

  gboolean unordered_blocks; // are we recording unordered blocks ?

  gboolean no_exit; // if TRUE, do not exit after saving set

  mt_opts multi_opts; // some multitrack options that survive between mt calls

  gint rec_aclip;
  gdouble rec_avel;
  gdouble rec_aseek;

  gpointer do_not_free; // mess with memory so that g_object_unref can be forced not to free() the pixel_data
  GMemVTable alt_vtable;
  void (*free_fn)(gpointer);

  pthread_mutex_t gtk_mutex;  // gtk drawing mutex - used by the threaded dialog
  pthread_mutex_t interp_mutex;  // interpolation mutex - parameter interpolation must be single threaded

  pthread_mutex_t abuf_mutex;  // used to synch audio buffer request count - shared between audio and video threads

  lives_fx_candidate_t fx_candidates[MAX_FX_CANDIDATE_TYPES]; // effects which can have candidates from which a delegate is selected (current examples are: audio_volume, resize)

  GList *cached_list;  // cache of preferences or file header file (or NULL)
  FILE *clip_header;

  gfloat volume; // audio volume level (for jack)

  int aud_rec_fd; // fd of file we are recording audio to
  gdouble rec_end_time;
  long rec_samples;
  gdouble rec_fps;
  gint rec_vid_frames;
  gint rec_arate;
  gint rec_achans;
  gint rec_asamps;
  gint rec_signed_endian;

  gboolean suppress_dprint; // tidy up, e.g. by blocking "switched to file..." and "closed file..." messages
  gchar *any_string;  // localised text saying "Any", for encoder and output format
  gchar *none_string;  // localised text saying "None", for playback plugin name, etc.
  gchar *recommended_string;  // localised text saying "recommended", for encoder and output format
  gchar *disabled_string;  // localised text saying "disabled !", for playback plugin name, etc.

  gint opening_frames; // count of frames so far opened, updated after preview (currently)

  gboolean show_procd; // override showing of "processing..." dialog

  gboolean block_param_updates; // block visual param changes from updating real values
  gboolean no_interp; // block interpolation (for single frame previews)

  gdouble fd_scale; // framedraw scale (image scaling downsize factor)

  weed_timecode_t cevent_tc; // timecode of currently processing event

  gboolean opening_multi; // flag to indicate multiple file selection

  gboolean record_paused; // pause during recording
  gboolean record_starting; // start recording at next frame

  gint img_concat_clip;  // when opening multiple, image files can get concatenated here (prefs->concat_images)

  GdkGC *general_gc;  // used for colour drawing

  // rendered generators
  gboolean gen_to_clipboard;
  gboolean is_generating;

  gboolean keep_pre;

  GtkWidget *textwidget_focus;

  _vid_playback_plugin *vpp;

  // multi-head support
  gint nmonitors;
  lives_mgeometry_t *mgeom;


  // external control inputs
  gboolean ext_cntl[MAX_EXT_CNTL];

  #ifdef ALSA_MIDI
  snd_seq_t *seq_handle;
  #endif

  weed_plant_t *rte_textparm; // send keyboard input to this paramter (usually NULL)

  gint write_abuf; // audio buffer number to write to (for multitrack)
  volatile gint abufs_to_fill;

  GtkWidget *splash_window;
  GtkWidget *splash_label;
  GtkWidget *splash_progress;

  gboolean soft_debug; // for testing


} mainwindow;

GdkCursor *hidden_cursor;

_palette *palette;

typedef struct {
  gulong ins_frame_function;

  GtkWidget *merge_dialog;
  GtkWidget *ins_frame_button;
  GtkWidget *drop_frame_button;
  GtkWidget *param_vbox;
  GtkWidget *spinbutton_loops;
  GtkWidget *trans_entry;

  gboolean loop_to_fit;
  gboolean align_start;
  gboolean ins_frames;
  int *list_to_rfx_index;
  GList *trans_list;

} _merge_opts;

_merge_opts* merge_opts;

// note, we can only have two of these currently, one for rendered effects, one for real time effects
GtkWidget *fx_dialog[2];

#endif // HAS_MAINWINDOW_H
