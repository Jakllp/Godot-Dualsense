#pragma once

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <vector>
#include <cstdint>
using namespace godot;

namespace godot {
	class DualSenseManager : public Node {
		GDCLASS(DualSenseManager, Node)
	public:
		DualSenseManager();
		~DualSenseManager();

		static DualSenseManager *get_singleton() { return singleton; }

		virtual void _ready() override;
		virtual void _process(double delta) override;
		virtual void _exit_tree() override;

		static void test_rumble();
		static void test_weapon();
		static void test_lightbar();
		static void test_custom_trigger();

		static bool is_connected();

		static void reset_trigger(int hand, int device_id);
		static void set_trigger_resistance(int hand, int start_zone, int strength, int device_id);
		static void set_trigger_weapon(int hand, int start_zone, int amplitude, int behavior, int trigger, int device_id);
		static void set_trigger_custom(int hand, PackedByteArray buffer, int device_id);
	private:
		static DualSenseManager *singleton;

	protected:
		static void _bind_methods();
	};
}