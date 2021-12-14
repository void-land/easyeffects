/*
 *  Copyright © 2017-2022 Wellington Wallace
 *
 *  This file is part of EasyEffects.
 *
 *  EasyEffects is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  EasyEffects is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with EasyEffects.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "stereo_tools_ui.hpp"

namespace ui::stereo_tools_box {

using namespace std::string_literals;

auto constexpr log_tag = "stereo_tools_box: ";

struct Data {
 public:
  ~Data() { util::debug(log_tag + "data struct destroyed"s); }

  std::shared_ptr<StereoTools> stereo_tools;

  std::vector<sigc::connection> connections;

  std::vector<gulong> gconnections;
};

struct _StereoToolsBox {
  GtkBox parent_instance;

  GtkScale *input_gain, *output_gain;

  GtkLevelBar *input_level_left, *input_level_right, *output_level_left, *output_level_right;

  GtkLabel *input_level_left_label, *input_level_right_label, *output_level_left_label, *output_level_right_label;

  GtkToggleButton* bypass;

  GtkComboBoxText* mode;

  GtkSpinButton *balance_in, *balance_out, *slev, *sbal, *mlev, *mpan, *stereo_base, *delay, *sc_level, *stereo_phase;

  GtkToggleButton *softclip, *mutel, *muter, *phasel, *phaser;

  GSettings* settings;

  Data* data;
};

G_DEFINE_TYPE(StereoToolsBox, stereo_tools_box, GTK_TYPE_BOX)

void on_bypass(StereoToolsBox* self, GtkToggleButton* btn) {
  self->data->stereo_tools->bypass = gtk_toggle_button_get_active(btn);
}

void on_reset(StereoToolsBox* self, GtkButton* btn) {
  gtk_toggle_button_set_active(self->bypass, 0);

  util::reset_all_keys(self->settings);
}

void setup(StereoToolsBox* self, std::shared_ptr<StereoTools> stereo_tools, const std::string& schema_path) {
  self->data->stereo_tools = stereo_tools;

  self->settings = g_settings_new_with_path("com.github.wwmm.easyeffects.stereotools", schema_path.c_str());

  stereo_tools->post_messages = true;
  stereo_tools->bypass = false;

  self->data->connections.push_back(stereo_tools->input_level.connect([=](const float& left, const float& right) {
    update_level(self->input_level_left, self->input_level_left_label, self->input_level_right,
                 self->input_level_right_label, left, right);
  }));

  self->data->connections.push_back(stereo_tools->output_level.connect([=](const float& left, const float& right) {
    update_level(self->output_level_left, self->output_level_left_label, self->output_level_right,
                 self->output_level_right_label, left, right);
  }));

  gsettings_bind_widgets<"input-gain", "output-gain">(self->settings, self->input_gain, self->output_gain);

  g_settings_bind(self->settings, "balance-in", gtk_spin_button_get_adjustment(self->balance_in), "value",
                  G_SETTINGS_BIND_DEFAULT);

  g_settings_bind(self->settings, "balance-out", gtk_spin_button_get_adjustment(self->balance_out), "value",
                  G_SETTINGS_BIND_DEFAULT);

  g_settings_bind(self->settings, "slev", gtk_spin_button_get_adjustment(self->slev), "value", G_SETTINGS_BIND_DEFAULT);

  g_settings_bind(self->settings, "sbal", gtk_spin_button_get_adjustment(self->sbal), "value", G_SETTINGS_BIND_DEFAULT);

  g_settings_bind(self->settings, "mlev", gtk_spin_button_get_adjustment(self->mlev), "value", G_SETTINGS_BIND_DEFAULT);

  g_settings_bind(self->settings, "mpan", gtk_spin_button_get_adjustment(self->mpan), "value", G_SETTINGS_BIND_DEFAULT);

  g_settings_bind(self->settings, "stereo-base", gtk_spin_button_get_adjustment(self->stereo_base), "value",
                  G_SETTINGS_BIND_DEFAULT);

  g_settings_bind(self->settings, "delay", gtk_spin_button_get_adjustment(self->delay), "value",
                  G_SETTINGS_BIND_DEFAULT);

  g_settings_bind(self->settings, "sc-level", gtk_spin_button_get_adjustment(self->sc_level), "value",
                  G_SETTINGS_BIND_DEFAULT);

  g_settings_bind(self->settings, "stereo-phase", gtk_spin_button_get_adjustment(self->stereo_phase), "value",
                  G_SETTINGS_BIND_DEFAULT);

  g_settings_bind(self->settings, "softclip", self->softclip, "active", G_SETTINGS_BIND_DEFAULT);

  g_settings_bind(self->settings, "mutel", self->mutel, "active", G_SETTINGS_BIND_DEFAULT);

  g_settings_bind(self->settings, "muter", self->muter, "active", G_SETTINGS_BIND_DEFAULT);

  g_settings_bind(self->settings, "phasel", self->phasel, "active", G_SETTINGS_BIND_DEFAULT);

  g_settings_bind(self->settings, "phaser", self->phaser, "active", G_SETTINGS_BIND_DEFAULT);

  g_settings_bind(self->settings, "mode", self->mode, "active-id", G_SETTINGS_BIND_DEFAULT);
}

void dispose(GObject* object) {
  auto* self = EE_STEREO_TOOLS_BOX(object);

  self->data->stereo_tools->bypass = false;

  for (auto& c : self->data->connections) {
    c.disconnect();
  }

  for (auto& handler_id : self->data->gconnections) {
    g_signal_handler_disconnect(self->settings, handler_id);
  }

  self->data->connections.clear();
  self->data->gconnections.clear();

  g_object_unref(self->settings);

  util::debug(log_tag + "disposed"s);

  G_OBJECT_CLASS(stereo_tools_box_parent_class)->dispose(object);
}

void finalize(GObject* object) {
  auto* self = EE_STEREO_TOOLS_BOX(object);

  delete self->data;

  util::debug(log_tag + "finalized"s);

  G_OBJECT_CLASS(stereo_tools_box_parent_class)->finalize(object);
}

void stereo_tools_box_class_init(StereoToolsBoxClass* klass) {
  auto* object_class = G_OBJECT_CLASS(klass);
  auto* widget_class = GTK_WIDGET_CLASS(klass);

  object_class->dispose = dispose;
  object_class->finalize = finalize;

  gtk_widget_class_set_template_from_resource(widget_class, "/com/github/wwmm/easyeffects/ui/stereo_tools.ui");

  gtk_widget_class_bind_template_child(widget_class, StereoToolsBox, input_gain);
  gtk_widget_class_bind_template_child(widget_class, StereoToolsBox, output_gain);
  gtk_widget_class_bind_template_child(widget_class, StereoToolsBox, input_level_left);
  gtk_widget_class_bind_template_child(widget_class, StereoToolsBox, input_level_right);
  gtk_widget_class_bind_template_child(widget_class, StereoToolsBox, output_level_left);
  gtk_widget_class_bind_template_child(widget_class, StereoToolsBox, output_level_right);
  gtk_widget_class_bind_template_child(widget_class, StereoToolsBox, input_level_left_label);
  gtk_widget_class_bind_template_child(widget_class, StereoToolsBox, input_level_right_label);
  gtk_widget_class_bind_template_child(widget_class, StereoToolsBox, output_level_left_label);
  gtk_widget_class_bind_template_child(widget_class, StereoToolsBox, output_level_right_label);

  gtk_widget_class_bind_template_child(widget_class, StereoToolsBox, bypass);

  gtk_widget_class_bind_template_child(widget_class, StereoToolsBox, mode);
  gtk_widget_class_bind_template_child(widget_class, StereoToolsBox, balance_in);
  gtk_widget_class_bind_template_child(widget_class, StereoToolsBox, balance_out);
  gtk_widget_class_bind_template_child(widget_class, StereoToolsBox, slev);
  gtk_widget_class_bind_template_child(widget_class, StereoToolsBox, sbal);
  gtk_widget_class_bind_template_child(widget_class, StereoToolsBox, mlev);
  gtk_widget_class_bind_template_child(widget_class, StereoToolsBox, mpan);
  gtk_widget_class_bind_template_child(widget_class, StereoToolsBox, stereo_base);
  gtk_widget_class_bind_template_child(widget_class, StereoToolsBox, delay);
  gtk_widget_class_bind_template_child(widget_class, StereoToolsBox, sc_level);
  gtk_widget_class_bind_template_child(widget_class, StereoToolsBox, stereo_phase);
  gtk_widget_class_bind_template_child(widget_class, StereoToolsBox, softclip);
  gtk_widget_class_bind_template_child(widget_class, StereoToolsBox, mutel);
  gtk_widget_class_bind_template_child(widget_class, StereoToolsBox, muter);
  gtk_widget_class_bind_template_child(widget_class, StereoToolsBox, phasel);
  gtk_widget_class_bind_template_child(widget_class, StereoToolsBox, phaser);

  gtk_widget_class_bind_template_callback(widget_class, on_bypass);
  gtk_widget_class_bind_template_callback(widget_class, on_reset);
}

void stereo_tools_box_init(StereoToolsBox* self) {
  gtk_widget_init_template(GTK_WIDGET(self));

  self->data = new Data();

  prepare_scales<"dB">(self->input_gain, self->output_gain);

  prepare_spinbutton<"dB">(self->slev);
  prepare_spinbutton<"dB">(self->mlev);
  prepare_spinbutton<"ms">(self->delay);

  prepare_spinbutton<"">(self->balance_in);
  prepare_spinbutton<"">(self->balance_out);
  prepare_spinbutton<"">(self->sc_level);
  prepare_spinbutton<"">(self->sbal);
  prepare_spinbutton<"">(self->mpan);
  prepare_spinbutton<"">(self->stereo_base);
  prepare_spinbutton<"">(self->stereo_phase);
}

auto create() -> StereoToolsBox* {
  return static_cast<StereoToolsBox*>(g_object_new(EE_TYPE_STEREO_TOOLS_BOX, nullptr));
}

}  // namespace ui::stereo_tools_box
