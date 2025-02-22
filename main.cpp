/*
run this as root for serial access. If root user need X, run this:
  $ xhost local:root
*/

#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <errno.h>

#include <chrono>
using std::chrono::high_resolution_clock;
using std::chrono::duration_cast;
using std::chrono::duration;
using std::chrono::milliseconds;
// auto t1 = high_resolution_clock::now();
// auto t2 = high_resolution_clock::now();
// duration<double, std::milli> duration = t2 - t1;
// printf("%.6f", duration.count());

#include "motor.h"
#include "gui.h"

#define PLOT_LEN 100

enum Operating_Mode {
	Position,
	Velocity,
	System,
};


int main() {
	Motor motor("/dev/ttyUSB0");

	motor.go_absolute_position(1000);
	sleep(1);
	motor.go_absolute_position(0);
	sleep(1);

	Gui gui;
	// ImGuiIO& io = gui.get_io();

	// 0 -> position mode
	// 1 -> velocity mode
	// 2 -> system mode
	//   using int because im lazy and dont want to warp imgui for this
	int operating_mode = 0;
	int old_operating_mode = operating_mode;

	// data to plot
	float position = motor.get_position();
	float positions[PLOT_LEN] = { position };

	float velocity = motor.get_velocity();
	float velocitys[PLOT_LEN] = { velocity };

	// motor boundary
	float pt_max = 10000;
	float vm_max = 20;
	float v_min = 0;
	float v_max = 20;
	float a_min = 0;
	float a_max = 20;

	// state value
	struct {
		float pt, v, a, old_pt, old_v, old_a;
	} data_p_mode;
	data_p_mode.pt = position;
	data_p_mode.v = motor.default_velocity;
	data_p_mode.a = motor.default_acceleration;
	data_p_mode.old_pt = data_p_mode.pt;
	data_p_mode.old_v = data_p_mode.v;
	data_p_mode.old_a = data_p_mode.a;

	struct {
		float v, a, old_v, old_a;
	} data_v_mode;
	data_v_mode.v = 0;
	data_v_mode.a = motor.default_acceleration;
	data_v_mode.old_v = data_v_mode.v;
	data_v_mode.old_a = data_v_mode.a;

	// gui value
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar;
	bool p_open = true;
	bool show_demo_window = true;
	int count = 0;

	while (!gui.window_should_close()) {
		gui.new_frame();
		count = (count + 1) % 10;

		ImGui::Begin("Smart Motor", &p_open, window_flags);

		ImGui::RadioButton("Position Mode", &operating_mode, 0); ImGui::SameLine();
		ImGui::RadioButton("Velocity Mode", &operating_mode, 1); ImGui::SameLine();
		ImGui::RadioButton("System", &operating_mode, 2);

		// motor info (mixing the timing for better lag experience)
		if (count == 0) {
			position = motor.get_position();
			for (int i = 0; i < PLOT_LEN-1; ++i) {
				positions[i] = positions[i+1];
			}
			positions[PLOT_LEN-1] = position;
		}
		if (count == 5) {
			velocity = motor.get_velocity();
			for (int i = 0; i < PLOT_LEN-1; ++i) {
				velocitys[i] = velocitys[i+1];
			}
			velocitys[PLOT_LEN-1] = velocity;
		}

		// position mode
		if (operating_mode == 0) {
			if (old_operating_mode != operating_mode) {
				motor.set_mode(Mode::Position);
				old_operating_mode = operating_mode;
				motor.set_velocity(data_p_mode.v);
				motor.set_acceleration(data_p_mode.a);
				motor.stop();
				data_p_mode.pt = position;
			}

			ImGui::SeparatorText("Range");

			ImGui::DragFloat("Position Range", &pt_max, 100.0f, 0.0f, motor.pt_limit, "Max: %.0f");
			ImGui::DragFloatRange2("Velocity Range", &v_min, &v_max,
				0.1f, 0.0f, motor.v_limit, "Min: %.1f", "Max: %.1f", ImGuiSliderFlags_AlwaysClamp);
			ImGui::DragFloatRange2("Acceleration Range", &a_min, &a_max,
				0.1f, 0.0f, motor.a_limit, "Min: %.1f", "Max: %.1f", ImGuiSliderFlags_AlwaysClamp);

			ImGui::SeparatorText("Operator");

			ImGui::SliderFloat("Absolute position", &data_p_mode.pt, -pt_max, pt_max);
			ImGui::SliderFloat("Velocity", &data_p_mode.v, v_min, v_max);
			ImGui::SliderFloat("Acceleration", &data_p_mode.a, a_min, a_max);

			if (ImGui::Button("Set Orgin")) {
				motor.set_origin();
				data_p_mode.pt = 0;
				data_p_mode.old_pt = 0;
			}
			ImGui::SameLine();
			if (ImGui::Button("To Orgin")) {
				motor.go_absolute_position(0);
				data_p_mode.pt = 0;
				data_p_mode.old_pt = 0;
			}

			ImGui::SeparatorText("Plot");

			ImGui::Text("Position: %.1f", position);
			ImGui::PlotLines("Position", positions, PLOT_LEN, 0, NULL,
				-pt_max, pt_max, ImVec2(0, 100));

			ImGui::Text("Velocity: %.1f", velocity);
			ImGui::PlotLines("Velocity", velocitys, PLOT_LEN, 0, NULL, v_min, v_max, ImVec2(0, 100));

			if (count == 0) {
				if (data_p_mode.pt != data_p_mode.old_pt) {
					motor.go_absolute_position(data_p_mode.pt);
					data_p_mode.old_pt = data_p_mode.pt;
				}

				if (data_p_mode.v != data_p_mode.old_v) {
					motor.set_velocity(data_p_mode.v);
					data_p_mode.old_v = data_p_mode.v;
				}

				if (data_p_mode.a != data_p_mode.old_a) {
					motor.set_acceleration(data_p_mode.a);
					data_p_mode.old_a = data_p_mode.a;
				}
			}
			// velocity mode
		} else if (operating_mode == 1) {
			if (old_operating_mode != operating_mode) {
				motor.set_mode(Mode::Velocity);
				old_operating_mode = operating_mode;
				data_v_mode.v = 0;
				motor.set_velocity(data_v_mode.v);
				motor.set_acceleration(data_v_mode.a);
			}

			ImGui::SeparatorText("Range");

			ImGui::DragFloat("Velocity Range", &vm_max, 0.1f, 0.0f, motor.v_limit, "Max: %.1f");
			ImGui::DragFloatRange2("Acceleration Range", &a_min, &a_max,
				0.1f, 0.0f, motor.a_limit, "Min: %.1f", "Max: %.1f", ImGuiSliderFlags_AlwaysClamp);

			ImGui::SeparatorText("Operator");

			ImGui::SliderFloat("Velocity", &data_v_mode.v, -vm_max, vm_max);
			ImGui::SliderFloat("Acceleration", &data_v_mode.a, a_min, a_max);

			if (ImGui::Button("Stop")) {
				motor.stop();
				data_v_mode.v = 0;
			}

			ImGui::SeparatorText("Plot");

			ImGui::Text("Position: %.1f", position);
			ImGui::PlotLines("Position", positions, PLOT_LEN, 0, NULL, FLT_MAX, FLT_MAX, ImVec2(0, 100));

			ImGui::Text("Velocity: %.1f", velocity);
			ImGui::PlotLines("Velocity", velocitys, PLOT_LEN, 0, NULL, -vm_max, vm_max, ImVec2(0, 100));

			if (count == 0) {
				if (data_v_mode.v != data_v_mode.old_v) {
					motor.set_velocity(data_v_mode.v);
					motor.go();
					data_v_mode.old_v = data_v_mode.v;
				}

				if (data_v_mode.a != data_v_mode.old_a) {
					motor.set_acceleration(data_v_mode.a);
					data_v_mode.old_a = data_v_mode.a;
				}
			}
		}

		ImGui::End();

		if (show_demo_window) {
			ImGui::ShowDemoWindow(&show_demo_window);
		}

		gui.render();
	}

	return 0;
}
