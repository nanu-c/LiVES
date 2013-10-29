// callbacks.h
// LiVES
// (c) G. Finch <salsaman@gmail.com> 2003 - 2013
// Released under the GPL 3 or later
// see file ../COPYING for licensing details

#ifndef HAS_LIVES_CALLBACKS_H
#define HAS_LIVES_CALLBACKS_H

boolean
on_LiVES_delete_event                  (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data);

void
on_open_activate                      (GtkMenuItem     *menuitem,
				       gpointer         user_data);

void
on_open_sel_activate                      (GtkMenuItem     *menuitem,
					   gpointer         user_data);

void
on_open_loc_activate                      (GtkMenuItem     *menuitem,
					   gpointer         user_data);

void
on_open_utube_activate                      (GtkMenuItem     *menuitem,
					     gpointer         user_data);

void
on_stop_clicked                         (GtkMenuItem     *menuitem,
                                         gpointer         user_data);

void
on_save_selection_activate            (GtkMenuItem     *menuitem,
				       gpointer         user_data);

void
on_save_as_activate            (GtkMenuItem     *menuitem,
				gpointer         user_data);

void
on_show_clipboard_info_activate            (GtkMenuItem     *menuitem,
					    gpointer         user_data);

void
on_close_activate            (GtkMenuItem     *menuitem,
			      gpointer         user_data);

void
on_import_proj_activate            (GtkMenuItem     *menuitem,
				    gpointer         user_data);

void
on_export_proj_activate            (GtkMenuItem     *menuitem,
				    gpointer         user_data);

void
on_quit_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_undo_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_redo_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_paste_as_new_activate                       (GtkMenuItem     *menuitem,
						gpointer         user_data);

void
on_copy_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_cut_activate                       (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_insert_pre_activate                    (GtkMenuItem     *menuitem,
					   gpointer         user_data);

void
on_insert_activate                    (GtkButton     *button,
                                        gpointer         user_data);

void
on_merge_activate                     (GtkMenuItem     *menuitem,
				       gpointer         user_data);

void
on_delete_activate                    (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_select_all_activate                (GtkMenuItem     *menuitem,
				       gpointer         user_data);

void
on_select_new_activate                (GtkMenuItem     *menuitem,
				       gpointer         user_data);

void
on_select_last_activate                (GtkMenuItem     *menuitem,
				       gpointer         user_data);

void
on_select_to_end_activate                (GtkMenuItem     *menuitem,
					  gpointer         user_data);

void
on_select_from_start_activate                (GtkMenuItem     *menuitem,
					      gpointer         user_data);

void
on_lock_selwidth_activate                (GtkMenuItem     *menuitem,
					  gpointer         user_data);

void
on_playall_activate                    (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_playsel_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_playclip_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_stop_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_rev_clipboard_activate              (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void 
on_encoder_entry_changed(GtkComboBox *, gpointer ptr);

void on_vid_playback_plugin_changed (GtkEntry *vpp_entry, gpointer user_data);


void
on_prefs_apply_clicked                   (GtkButton       *button,
					  gpointer         user_data);

void
on_show_file_info_activate            (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
switch_clip_activate                    (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_about_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
show_manual_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
email_author_activate                     (GtkMenuItem     *menuitem,
					   gpointer         user_data);

void
donate_activate                     (GtkMenuItem     *menuitem,
					   gpointer         user_data);

void
report_bug_activate                     (GtkMenuItem     *menuitem,
					   gpointer         user_data);

void
suggest_feature_activate                     (GtkMenuItem     *menuitem,
					      gpointer         user_data);

void
help_translate_activate                     (GtkMenuItem     *menuitem,
					   gpointer         user_data);

void
on_ok_button1_clicked                  (GtkButton       *button,
                                        gpointer         user_data);


void on_ok_file_open_clicked(GtkFileChooser *, GSList *fnames);

void on_ok_filesel_open_clicked (GtkFileChooser *, gpointer);

void open_sel_range_activate (void);



void
on_location_select                   (GtkButton       *button,
				      gpointer         user_data);

void
on_utube_select                   (GtkButton       *button,
				   gpointer         user_data);

void
on_autoreload_toggled                (GtkToggleButton *togglebutton,
				 gpointer         user_data);

void
on_opensel_range_ok_clicked                  (GtkButton       *button,
					      gpointer         user_data);


void on_open_sel_ok_button_clicked (GtkButton *, gpointer user_data);

void on_save_textview_clicked (GtkButton *, gpointer);

void on_cancel_button1_clicked (GtkWidget *, gpointer data_to_free);


void
on_full_screen_pressed (GtkButton *button,
			gpointer user_data);

void
on_full_screen_activate               (GtkMenuItem     *menuitem,
				       gpointer         user_data);

void
on_double_size_pressed (GtkButton *button,
			gpointer user_data);
void
on_double_size_activate               (GtkMenuItem     *menuitem,
				       gpointer         user_data);

void
on_sepwin_pressed (GtkButton *button,
		   gpointer user_data);

void
on_sepwin_activate               (GtkMenuItem     *menuitem,
				  gpointer         user_data);

void
on_fade_pressed (GtkButton *button,
		 gpointer user_data);

void
on_fade_activate               (GtkMenuItem     *menuitem,
				gpointer         user_data);



void
on_loop_video_activate                (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_mute_button_activate                (GtkMenuItem     *menuitem,
					gpointer         user_data);

void
on_mute_activate                (GtkMenuItem     *menuitem,
				 gpointer         user_data);


void
on_details_button_clicked            (GtkButton       *button,
				      gpointer         user_data);


void
on_resize_activate                    (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_rename_activate                    (GtkMenuItem     *menuitem,
				       gpointer         user_data);

void
on_rename_set_name                   (GtkButton       *button,
				      gpointer         user_data);

void
on_spinbutton_start_value_changed          (GtkSpinButton   *spinbutton,
					    gpointer         user_data);

void
on_spinbutton_end_value_changed          (GtkSpinButton   *spinbutton,
					  gpointer         user_data);

void on_open_new_audio_clicked (GtkFileChooser *, gpointer opt_filename);

void on_load_audio_activate (GtkMenuItem *, gpointer);

void on_load_subs_activate (GtkMenuItem *, gpointer);

void on_save_subs_activate (GtkMenuItem *, gpointer entry_widget);

void on_erase_subs_activate (GtkMenuItem *, gpointer);


void 
on_insfitaudio_toggled                (GtkToggleButton *togglebutton,
				       gpointer         user_data);

void
on_resize_hsize_value_changed           (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

void
on_resize_vsize_value_changed           (GtkSpinButton   *spinbutton,
                                        gpointer         user_data);

void
on_resize_ok_clicked                   (GtkButton       *button,
                                        gpointer         user_data);


void
on_spin_value_changed           (GtkSpinButton   *spinbutton,
				  gpointer         user_data);

void
on_spin_start_value_changed           (GtkSpinButton   *spinbutton,
					gpointer         user_data);

void
on_spin_step_value_changed           (GtkSpinButton   *spinbutton,
				       gpointer         user_data);

void
on_spin_end_value_changed           (GtkSpinButton   *spinbutton,
				       gpointer         user_data);


#if GTK_CHECK_VERSION(3,0,0)
boolean expose_vid_event (GtkWidget *, lives_painter_t *cr, gpointer user_data);
#else
boolean expose_vid_event (GtkWidget *, GdkEventExpose *event);
#endif

#if GTK_CHECK_VERSION(3,0,0)
boolean expose_laud_event (GtkWidget *, lives_painter_t *cr, gpointer user_data);
#else
boolean expose_laud_event (GtkWidget *, GdkEventExpose *event);
#endif

#if GTK_CHECK_VERSION(3,0,0)
boolean expose_raud_event (GtkWidget *, lives_painter_t *cr, gpointer user_data);
#else
boolean expose_raud_event (GtkWidget *, GdkEventExpose *event);
#endif



void
on_preview_clicked                     (GtkButton       *button,
					gpointer         user_data);

void
on_recent_activate                      (GtkMenuItem     *menuitem,
					 gpointer         user_data);

boolean config_event (GtkWidget *, GdkEventConfigure *, gpointer);

void
changed_fps_during_pb           (GtkSpinButton   *spinbutton,
				 gpointer         user_data);

boolean
on_mouse_scroll           (GtkWidget       *widget,
			   GdkEventScroll  *event,
			   gpointer         user_data);

boolean
on_mouse_sel_update           (GtkWidget       *widget,
			       GdkEventMotion  *event,
			       gpointer         user_data);

boolean
on_mouse_sel_reset           (GtkWidget       *widget,
			      GdkEventButton  *event,
			      gpointer         user_data);

boolean
on_mouse_sel_start           (GtkWidget       *widget,
			      GdkEventButton  *event,
			      gpointer         user_data);

void
on_load_cdtrack_activate                (GtkMenuItem     *menuitem,
					 gpointer         user_data);

void on_load_cdtrack_ok_clicked                (GtkButton     *button,
						gpointer         user_data);

void
on_eject_cd_activate                (GtkMenuItem     *menuitem,
				     gpointer         user_data);


void
on_slower_pressed (GtkButton *button,
		   gpointer user_data);

void
on_faster_pressed (GtkButton *button,
		   gpointer user_data);

void
on_back_pressed (GtkButton *button,
		   gpointer user_data);

void
on_forward_pressed (GtkButton *button,
		   gpointer user_data);

void on_capture_activate (GtkMenuItem *, gpointer user_data);

void on_capture2_activate(void);


void
on_select_invert_activate                (GtkMenuItem     *menuitem,
					  gpointer         user_data);

void
on_warn_mask_toggled        (GtkToggleButton *togglebutton,
			     gpointer         user_data);

boolean
frame_context           (GtkWidget       *widget,
			  GdkEventButton  *event,
			  gpointer         which);

void on_fs_preview_clicked (GtkWidget *widget, gpointer user_data);


void
on_restore_activate                      (GtkMenuItem     *menuitem,
				       gpointer         user_data);

void
on_backup_activate                      (GtkMenuItem     *menuitem,
					 gpointer         user_data);


void
on_record_perf_activate                      (GtkMenuItem     *menuitem,
					      gpointer         user_data);

boolean record_toggle_callback (GtkAccelGroup *, GObject *, guint keyval, GdkModifierType mod, gpointer);


boolean fps_reset_callback (GtkAccelGroup *, GObject *, guint keyval, GdkModifierType mod, gpointer);


boolean mute_audio_callback (GtkAccelGroup *, GObject *, guint keyval, GdkModifierType mod, gpointer);



boolean
on_stop_activate_by_del                  (GtkWidget       *widget,
					  GdkEvent        *event,
					  gpointer         user_data);

void on_pause_clicked(void);


void
on_select_start_only_activate                (GtkMenuItem     *menuitem,
					      gpointer         user_data);

void
on_select_end_only_activate                (GtkMenuItem     *menuitem,
					    gpointer         user_data);

void
on_filesel_complex_clicked                      (GtkButton *button,
						GtkEntry *entry);

void
on_filesel_complex_ok_clicked                      (GtkButton *button,
						   GtkEntry *entry);

void 
on_encoder_ofmt_changed(GtkComboBox *combo, gpointer user_data);

void
on_ok_export_audio_clicked                      (GtkButton *button,
						 gpointer user_data);

void on_append_audio_activate (GtkMenuItem *, gpointer user_data);

void
on_menubar_activate_menuitem                    (GtkMenuItem     *menuitem,
						 gpointer         user_data);

void
on_rb_audrec_time_toggled                (GtkToggleButton *togglebutton,
					  gpointer         user_data);

void
on_recaudclip_activate (GtkMenuItem     *menuitem,
			gpointer         user_data);

void
on_recaudsel_activate (GtkMenuItem     *menuitem,
		       gpointer         user_data);


void
on_recaudclip_ok_clicked                      (GtkButton *button,
					       gpointer user_data);
#if GTK_CHECK_VERSION(2,14,0)
void on_volume_slider_value_changed (GtkScaleButton *, gpointer);
#else
void on_volume_slider_value_changed (GtkRange *, gpointer);
#endif

void on_fade_audio_activate (GtkMenuItem *, gpointer user_data);

void on_ok_append_audio_clicked (GtkFileChooser *, gpointer);

void
on_resample_video_activate (GtkMenuItem     *menuitem,
			    gpointer         user_data);

void
on_resample_vid_ok                  (GtkButton       *button,
				  GtkEntry         *entry);

void on_trim_audio_activate (GtkMenuItem *, gpointer user_data);

void
on_resample_audio_activate (GtkMenuItem     *menuitem,
			    gpointer         user_data);


void 
on_export_audio_activate (GtkMenuItem *menuitem, 
			  gpointer user_data);


void
on_resaudio_ok_clicked                      (GtkButton *button,
					     GtkEntry *entry);

void
on_cancel_opensel_clicked              (GtkButton       *button,
					 gpointer         user_data);

void 
end_fs_preview(void);


void
on_sticky_activate               (GtkMenuItem     *menuitem,
				  gpointer         user_data);

void on_resaudw_asamps_changed (GtkWidget *, gpointer);


void 
on_insertwsound_toggled                (GtkToggleButton *togglebutton,
					gpointer         user_data);

void
on_showfct_activate               (GtkMenuItem     *menuitem,
				   gpointer         user_data);

void on_boolean_toggled(GObject *, gpointer user_data);


void on_showsubs_toggled(GObject *, gpointer);

void
on_show_messages_activate            (GtkMenuItem     *menuitem,
				      gpointer         user_data);

boolean on_hrule_enter (GtkWidget *, GdkEventCrossing *, gpointer);

boolean
on_hrule_update           (GtkWidget       *widget,
			   GdkEventMotion  *event,
			   gpointer         user_data);
boolean
on_hrule_reset           (GtkWidget       *widget,
			  GdkEventButton  *event,
			  gpointer         user_data);

boolean
on_hrule_set           (GtkWidget       *widget,
			  GdkEventButton  *event,
			  gpointer         user_data);

void
on_rewind_activate                    (GtkMenuItem     *menuitem,
				       gpointer         user_data);
void
on_loop_button_activate                (GtkMenuItem     *menuitem,
					     gpointer         user_data);

void
on_loop_cont_activate                (GtkMenuItem     *menuitem,
				      gpointer         user_data);

void
on_show_file_comments_activate            (GtkMenuItem     *menuitem,
					   gpointer         user_data);

void
on_toolbar_hide (GtkButton *button,
		 gpointer user_data);

void
on_toy_activate                (GtkMenuItem     *new_toy,
				gpointer         old_toy_p);

void
on_preview_spinbutton_changed          (GtkSpinButton   *spinbutton,
					  gpointer         user_data);

boolean prevclip_callback (GtkAccelGroup *, GObject *, guint, GdkModifierType, gpointer );

boolean nextclip_callback (GtkAccelGroup *, GObject *, guint, GdkModifierType, gpointer );

boolean freeze_callback (GtkAccelGroup *, GObject *, guint, GdkModifierType, gpointer );

boolean storeclip_callback (GtkAccelGroup *, GObject *, guint, GdkModifierType, gpointer );

boolean nervous_callback (GtkAccelGroup *, GObject *, guint, GdkModifierType, gpointer );

boolean show_sync_callback (GtkAccelGroup *, GObject *, guint keyval, GdkModifierType mod, gpointer clip_number);

void on_save_set_activate (GtkMenuItem *, gpointer );

void on_save_set_ok (void);

void on_load_set_activate (GtkMenuItem *, gpointer );

boolean on_load_set_ok (GtkButton *, gpointer skip_threaded_dialog);

void on_open_vcd_activate (GtkMenuItem *, gpointer int_type);

void on_load_vcd_ok_clicked (GtkButton *, gpointer user_data);

void on_ping_pong_activate (GtkMenuItem *, gpointer);

void on_show_keys_activate (GtkMenuItem *, gpointer);

void on_vj_reset_activate (GtkMenuItem *, gpointer);

void on_prv_link_toggled (GtkToggleButton *, gpointer);

boolean on_del_audio_activate (GtkMenuItem *, gpointer);

boolean on_ins_silence_activate (GtkMenuItem *, gpointer);

void on_ins_silence_details_clicked (GtkButton *, gpointer);

void on_lerrors_close_clicked (GtkButton *, gpointer);
void on_lerrors_clear_clicked (GtkButton *, gpointer);
void on_lerrors_delete_clicked (GtkButton *, gpointer);

void drag_from_outside(GtkWidget *, GdkDragContext *, int x, int y, 
		       GtkSelectionData *, guint info, guint time, gpointer user_data);



#endif
