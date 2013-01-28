/*************************************************************************
 * Copyright (C) 2011-2013 by Ilari Liusvaara                            *
 *                                                                       *
 * This program is free software: you can redistribute it and/or modify  *
 * it under the terms of the GNU General Public License as published by  *
 * the Free Software Foundation, either version 3 of the License, or     *
 * (at your option) any later version.                                   *
 *                                                                       *
 * This program is distributed in the hope that it will be useful,       *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 * GNU General Public License for more details.                          *
 *                                                                       *
 * You should have received a copy of the GNU General Public License     *
 * along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 *************************************************************************/
#include "lsnes.hpp"
#include <sstream>
#include <map>
#include <string>
#include <cctype>
#include <vector>
#include <fstream>
#include <climits>
#include "core/audioapi.hpp"
#include "core/misc.hpp"
#include "core/command.hpp"
#include "core/controllerframe.hpp"
#include "core/dispatch.hpp"
#include "core/framebuffer.hpp"
#include "core/settings.hpp"
#include "core/window.hpp"
#include "interface/cover.hpp"
#include "interface/romtype.hpp"
#include "interface/setting.hpp"
#include "interface/callbacks.hpp"
#include "library/pixfmt-lrgb.hpp"
#include "library/string.hpp"
#include "library/portfn.hpp"
#include "library/framebuffer.hpp"
#include "library/luabase.hpp"
#include "lua/internal.hpp"
#include <snes/snes.hpp>
#include <gameboy/gameboy.hpp>
#ifdef BSNES_V087
#include <target-libsnes/libsnes.hpp>
#else
#include <ui-libsnes/libsnes.hpp>
#endif

#define DURATION_NTSC_FRAME 357366
#define DURATION_NTSC_FIELD 357368
#define DURATION_PAL_FRAME 425568
#define DURATION_PAL_FIELD 425568
#define ROM_TYPE_NONE 0
#define ROM_TYPE_SNES 1
#define ROM_TYPE_BSX 2
#define ROM_TYPE_BSXSLOTTED 3
#define ROM_TYPE_SUFAMITURBO 4
#define ROM_TYPE_SGB 5

namespace
{
	bool p1disable = false;
	bool do_hreset_flag = false;
	long do_reset_flag = -1;
	bool support_hreset = false;
	bool save_every_frame = false;
	bool have_saved_this_frame = false;
	int16_t blanksound[1070] = {0};
	int16_t soundbuf[8192] = {0};
	size_t soundbuf_fill = 0;
	bool last_hires = false;
	bool last_interlace = false;
	uint64_t trace_counter;
	std::ofstream trace_output;
	bool trace_output_enable;
	SNES::Interface* old;
	bool stepping_into_save;
	bool video_refresh_done;
	bool forced_hook = false;
	std::map<int16_t, std::pair<uint64_t, uint64_t>> ptrmap;
	uint32_t cover_fbmem[512 * 448];
	//Delay reset.
	unsigned long long delayreset_cycles_run;
	unsigned long long delayreset_cycles_target;

	//Framebuffer.
	struct framebuffer_info cover_fbinfo = {
		&_pixel_format_lrgb,		//Format.
		(char*)cover_fbmem,		//Memory.
		512, 448, 2048,			//Physical size.
		512, 448, 2048,			//Logical size.
		0, 0				//Offset.
	};

#include "ports.inc"
#include "slots.inc"
#include "regions.inc"

	core_setting_group bsnes_settings;
	core_setting setting_port1(bsnes_settings, "port1", "Port 1 Type", "gamepad");
	core_setting setting_port2(bsnes_settings, "port2", "Port 2 Type", "none");
	core_setting setting_hardreset(bsnes_settings, "hardreset", "Support hard resets", "0");
	core_setting setting_saveevery(bsnes_settings, "saveevery", "Emulate saving each frame", "0");
	core_setting setting_randinit(bsnes_settings, "radominit", "Random initial state", "0");
	core_setting_value setting_port1_none(setting_port1, "none", "None", 0);
	core_setting_value setting_port2_none(setting_port2, "none", "None", 0);
	core_setting_value setting_port2_gamepad(setting_port2, "gamepad", "Gamepad", 1);
	core_setting_value setting_port1_gamepad(setting_port1, "gamepad", "Gamepad", 1);
	core_setting_value setting_port1_gamepad16(setting_port1, "gamepad16", "Gamepad (16-button)", 2);
	core_setting_value setting_port2_gamepad16(setting_port2, "gamepad16", "Gamepad (16-button)", 2);
	core_setting_value setting_port2_multitap(setting_port2, "multitap", "Multitap", 3);
	core_setting_value setting_port1_multitap(setting_port1, "multitap", "Multitap", 3);
	core_setting_value setting_port2_multitap16(setting_port2, "multitap16", "Multitap (16-button)", 4);
	core_setting_value setting_port1_multitap16(setting_port1, "multitap16", "Multitap (16-button)", 4);
	core_setting_value setting_port2_mouse(setting_port2, "mouse", "Mouse", 5);
	core_setting_value setting_port1_mouse(setting_port1, "mouse", "Mouse", 5);
	core_setting_value setting_port2_superscope(setting_port2, "superscope", "Super Scope", 8);
	core_setting_value setting_port2_justifier(setting_port2, "justifier", "Justifier", 6);
	core_setting_value setting_port2_justifiers(setting_port2, "justifiers", "2 Justifiers", 7);
	core_setting_value setting_hardreset_0(setting_hardreset, "0", "False", 0);
	core_setting_value setting_hardreset_1(setting_hardreset, "1", "True", 1);
	core_setting_value setting_saveevery_0(setting_saveevery, "0", "False", 0);
	core_setting_value setting_saveevery_1(setting_saveevery, "1", "True", 1);
	core_setting_value setting_randinit_0(setting_randinit, "0", "False", 0);
	core_setting_value setting_randinit_1(setting_randinit, "1", "True", 1);
	
	////////////////// PORTS COMMON ///////////////////
	port_type* index_to_ptype[] = {
		&none, &gamepad, &gamepad16, &multitap, &multitap16, &mouse, &justifier, &justifiers, &superscope
	};
	unsigned index_to_bsnes_type[] = {
		SNES_DEVICE_NONE, SNES_DEVICE_JOYPAD, SNES_DEVICE_JOYPAD, SNES_DEVICE_MULTITAP, SNES_DEVICE_MULTITAP,
		SNES_DEVICE_MOUSE, SNES_DEVICE_JUSTIFIER, SNES_DEVICE_JUSTIFIERS, SNES_DEVICE_SUPER_SCOPE
	};


	void snesdbg_on_break();
	void snesdbg_on_trace();

	class my_interfaced : public SNES::Interface
	{
		string path(SNES::Cartridge::Slot slot, const string &hint)
		{
			return "./";
		}
	};

	void basic_init()
	{
		static bool done = false;
		if(done)
			return;
		done = true;
		static my_interfaced i;
		SNES::interface = &i;
	}

	core_type* internal_rom = NULL;
	extern core_core_params _bsnes_core;

	template<bool(*T)(const char*,const unsigned char*, unsigned)>
	bool load_rom_X1(core_romimage* img)
	{
		return T(img[0].markup, img[0].data, img[0].size);
	}

	template<bool(*T)(const char*,const unsigned char*, unsigned, const char*,const unsigned char*, unsigned)>
	bool load_rom_X2(core_romimage* img)
	{
		return T(img[0].markup, img[0].data, img[0].size,
			img[1].markup, img[1].data, img[1].size);
	}

	template<bool(*T)(const char*,const unsigned char*, unsigned, const char*,const unsigned char*, unsigned,
		const char*,const unsigned char*, unsigned)>
	bool load_rom_X3(core_romimage* img)
	{
		return T(img[0].markup, img[0].data, img[0].size,
			img[1].markup, img[1].data, img[1].size,
			img[2].markup, img[2].data, img[2].size);
	}


	template<core_type* ctype>
	int load_rom(core_romimage* img, std::map<std::string, std::string>& settings, uint64_t secs,
		uint64_t subsecs, bool(*fun)(core_romimage*))
	{
		std::map<std::string, std::string> _settings = settings;
		bsnes_settings.fill_defaults(_settings);
		signed type1 = setting_port1.ivalue_to_index(_settings[setting_port1.iname]);
		signed type2 = setting_port2.ivalue_to_index(_settings[setting_port2.iname]);
		signed hreset = setting_hardreset.ivalue_to_index(_settings[setting_hardreset.iname]);
		signed esave = setting_saveevery.ivalue_to_index(_settings[setting_saveevery.iname]);
		signed irandom = setting_randinit.ivalue_to_index(_settings[setting_randinit.iname]);

		basic_init();
		snes_term();
		snes_unload_cartridge();
		SNES::config.random = (irandom != 0);
		save_every_frame = (esave != 0);
		support_hreset = (hreset != 0);
		SNES::config.expansion_port = SNES::System::ExpansionPortDevice::None;
		bool r = fun(img);
		if(r) {
			internal_rom = ctype;
			snes_set_controller_port_device(false, index_to_bsnes_type[type1]);
			snes_set_controller_port_device(true, index_to_bsnes_type[type2]);
			have_saved_this_frame = false;
			do_reset_flag = -1;
		}
		return r ? 0 : -1;
	}

	std::pair<uint64_t, uint64_t> bsnes_get_bus_map()
	{
		return std::make_pair(0x1000000, 0x1000000);
	}

	port_index_triple t(unsigned p, unsigned c, unsigned i, bool nl)
	{
		port_index_triple x;
		x.port = p;
		x.controller = c;
		x.control = i;
		return x;
	}

	void push_port_indices(std::vector<port_index_triple>& tab, unsigned p, port_type& pt)
	{
		unsigned ctrls = pt.controller_info->controller_count;
		for(unsigned i = 0; i < ctrls; i++)
			for(unsigned j = 0; j < pt.controller_info->controllers[i]->button_count; j++)
				tab.push_back(t(p, i, j, true));
	}

	controller_set _controllerconfig(std::map<std::string, std::string>& settings)
	{
		std::map<std::string, std::string> _settings = settings;
		bsnes_settings.fill_defaults(_settings);
		signed type1 = setting_port1.ivalue_to_index(_settings[setting_port1.iname]);
		signed type2 = setting_port2.ivalue_to_index(_settings[setting_port2.iname]);
		signed hreset = setting_hardreset.ivalue_to_index(_settings[setting_hardreset.iname]);
		controller_set r;
		if(hreset)
			r.ports.push_back(&psystem_hreset);
		else
			r.ports.push_back(&psystem);
		r.ports.push_back(index_to_ptype[type1]);
		r.ports.push_back(index_to_ptype[type2]);
		unsigned p1controllers = r.ports[1]->controller_info->controller_count;
		unsigned p2controllers = r.ports[2]->controller_info->controller_count;
		for(unsigned i = 0; i < (hreset ? 5 : 4); i++)
			r.portindex.indices.push_back(t(0, 0, i, false));
		push_port_indices(r.portindex.indices, 1, *r.ports[1]);
		push_port_indices(r.portindex.indices, 2, *r.ports[2]);
		r.portindex.logical_map.resize(p1controllers + p2controllers);
		if(p1controllers == 4) {
			r.portindex.logical_map[0] = std::make_pair(1, 0);
			for(size_t j = 0; j < p2controllers; j++)
				r.portindex.logical_map[j + 1] = std::make_pair(2U, j);
			for(size_t j = 1; j < p1controllers; j++)
				r.portindex.logical_map[j + p2controllers]  = std::make_pair(1U, j);
		} else {
			for(size_t j = 0; j < p1controllers; j++)
				r.portindex.logical_map[j] = std::make_pair(1, j);
			for(size_t j = 0; j < p2controllers; j++)
				r.portindex.logical_map[j + p1controllers]  = std::make_pair(2U, j);
		}
		for(unsigned i = 0; i < 8; i++)
			r.portindex.pcid_map.push_back(std::make_pair(i / 4 + 1, i % 4));
		return r;
	}

#ifdef BSNES_HAS_DEBUGGER
#define BSNES_RESET_LEVEL 6
#else
#define BSNES_RESET_LEVEL 5
#endif

	class my_interface : public SNES::Interface
	{
		string path(SNES::Cartridge::Slot slot, const string &hint)
		{
			const char* _hint = hint;
			std::string _hint2 = _hint;
			std::string fwp = ecore_callbacks->get_firmware_path();
			regex_results r;
			std::string msubase = ecore_callbacks->get_base_path();
			if(regex_match(".*\\.sfc", msubase))
				msubase = msubase.substr(0, msubase.length() - 4);

			if(_hint2 == "msu1.rom" || _hint2 == ".msu") {
				//MSU-1 main ROM.
				std::string x = msubase + ".msu";
				messages << "MSU main data file: " << x << std::endl;
				return x.c_str();
			}
			if(r = regex("(track)?(-([0-9])+\\.pcm)", _hint2)) {
				//MSU track.
				std::string x = msubase + r[2];
				messages << "MSU track " << r[3] << "': " << x << std::endl;
				return x.c_str();
			}
			std::string finalpath = fwp + "/" + _hint2;
			return finalpath.c_str();
		}

		time_t currentTime()
		{
			return ecore_callbacks->get_time();
		}

		time_t randomSeed()
		{
			return ecore_callbacks->get_randomseed();
		}

		void videoRefresh(const uint32_t* data, bool hires, bool interlace, bool overscan)
		{
			last_hires = hires;
			last_interlace = interlace;
			bool region = (SNES::system.region() == SNES::System::Region::PAL);
			if(stepping_into_save)
				messages << "Got video refresh in runtosave, expect desyncs!" << std::endl;
			video_refresh_done = true;
			uint32_t fps_n, fps_d;
			auto fps = _bsnes_core.video_rate();
			fps_n = fps.first;
			fps_d = fps.second;
			uint32_t g = gcd(fps_n, fps_d);
			fps_n /= g;
			fps_d /= g;

			framebuffer_info inf;
			inf.type = &_pixel_format_lrgb;
			inf.mem = const_cast<char*>(reinterpret_cast<const char*>(data));
			inf.physwidth = 512;
			inf.physheight = 512;
			inf.physstride = 2048;
			inf.width = hires ? 512 : 256;
			inf.height = (region ? 239 : 224) * (interlace ? 2 : 1);
			inf.stride = interlace ? 2048 : 4096;
			inf.offset_x = 0;
			inf.offset_y = (region ? (overscan ? 9 : 1) : (overscan ? 16 : 9)) * 2;

			framebuffer_raw ls(inf);
			ecore_callbacks->output_frame(ls, fps_n, fps_d);
			information_dispatch::do_raw_frame(data, hires, interlace, overscan, region ?
				VIDEO_REGION_PAL : VIDEO_REGION_NTSC);
			if(soundbuf_fill > 0) {
				auto freq = SNES::system.apu_frequency();
				audioapi_submit_buffer(soundbuf, soundbuf_fill / 2, true, freq / 768.0);
				soundbuf_fill = 0;
			}
		}

		void audioSample(int16_t l_sample, int16_t r_sample)
		{
			uint16_t _l = l_sample;
			uint16_t _r = r_sample;
			soundbuf[soundbuf_fill++] = l_sample;
			soundbuf[soundbuf_fill++] = r_sample;
			information_dispatch::do_sample(l_sample, r_sample);
			//The SMP emits a sample every 768 ticks of its clock. Use this in order to keep track of
			//time.
			ecore_callbacks->timer_tick(768, SNES::system.apu_frequency());
		}

		int16_t inputPoll(bool port, SNES::Input::Device device, unsigned index, unsigned id)
		{
			int16_t offset = 0;
			//The superscope/justifier handling is nuts.
			if(port && SNES::input.port2) {
				SNES::SuperScope* ss = dynamic_cast<SNES::SuperScope*>(SNES::input.port2);
				SNES::Justifier* js = dynamic_cast<SNES::Justifier*>(SNES::input.port2);
				if(ss && index == 0) {
					if(id == 0)
						offset = ss->x;
					if(id == 1)
						offset = ss->y;
				}
				if(js && index == 0) {
					if(id == 0)
						offset = js->player1.x;
					if(id == 1)
						offset = js->player1.y;
				}
				if(js && js->chained && index == 1) {
					if(id == 0)
						offset = js->player2.x;
					if(id == 1)
						offset = js->player2.y;
				}
			}
			return ecore_callbacks->get_input(port ? 2 : 1, index, id) - offset;
		}
	} my_interface_obj;

	bool trace_fn()
	{
#ifdef BSNES_HAS_DEBUGGER
		if(trace_counter && !--trace_counter) {
			//Trace counter did transition 1->0. Call the hook.
			snesdbg_on_trace();
		}
		if(trace_output_enable) {
			char buffer[1024];
			SNES::cpu.disassemble_opcode(buffer, SNES::cpu.regs.pc);
			trace_output << buffer << std::endl;
		}
		return false;
#endif
	}
	bool delayreset_fn()
	{
		trace_fn();	//Call this also.
		if(delayreset_cycles_run == delayreset_cycles_target || video_refresh_done)
			return true;
		delayreset_cycles_run++;
		return false;
	}


	bool trace_enabled()
	{
		return (trace_counter || !!trace_output_enable);
	}

	void update_trace_hook_state()
	{
		if(forced_hook)
			return;
#ifdef BSNES_HAS_DEBUGGER
		if(!trace_enabled())
			SNES::cpu.step_event = nall::function<bool()>();
		else
			SNES::cpu.step_event = trace_fn;
#endif
	}

	std::string sram_name(const nall::string& _id, SNES::Cartridge::Slot slotname)
	{
		std::string id(_id, _id.length());
		//Fixup name change by bsnes v087...
		if(id == "bsx.ram")
			id = ".bss";
		if(id == "bsx.psram")
			id = ".bsp";
		if(id == "program.rtc")
			id = ".rtc";
		if(id == "upd96050.ram")
			id = ".dsp";
		if(id == "program.ram")
			id = ".srm";
		if(slotname == SNES::Cartridge::Slot::SufamiTurboA)
			return "slota." + id.substr(1);
		if(slotname == SNES::Cartridge::Slot::SufamiTurboB)
			return "slotb." + id.substr(1);
		return id.substr(1);
	}

	uint8_t snes_bus_iospace_rw(uint64_t offset, uint8_t data, bool write)
	{
		if(write)
			SNES::bus.write(offset, data);
		else
			return SNES::bus.read(offset);
	}

	uint8_t ptrtable_iospace_rw(uint64_t offset, uint8_t data, bool write)
	{
		uint16_t entry = offset >> 4;
		if(!ptrmap.count(entry))
			return 0;
		uint64_t val = ((offset & 15) < 8) ? ptrmap[entry].first : ptrmap[entry].second;
		uint8_t byte = offset & 7;
		//These things are always little-endian.
		return (val >> (8 * byte));
	}

	void create_region(std::list<core_vma_info>& inf, const std::string& name, uint64_t base, uint64_t size,
		uint8_t (*iospace_rw)(uint64_t offset, uint8_t data, bool write)) throw(std::bad_alloc)
	{
		if(size == 0)
			return;
		core_vma_info i;
		i.name = name;
		i.base = base;
		i.size = size;
		i.readonly = false;
		i.native_endian = false;
		i.iospace_rw = iospace_rw;
		inf.push_back(i);
	}

	void create_region(std::list<core_vma_info>& inf, const std::string& name, uint64_t base, uint8_t* memory,
		uint64_t size, bool readonly, bool native_endian = false) throw(std::bad_alloc)
	{
		if(size == 0)
			return;
		core_vma_info i;
		i.name = name;
		i.base = base;
		i.size = size;
		i.backing_ram = memory;
		i.readonly = readonly;
		i.native_endian = native_endian;
		i.iospace_rw = NULL;
		inf.push_back(i);
	}

	void create_region(std::list<core_vma_info>& inf, const std::string& name, uint64_t base,
		SNES::MappedRAM& memory, bool readonly, bool native_endian = false) throw(std::bad_alloc)
	{
		create_region(inf, name, base, memory.data(), memory.size(), readonly, native_endian);
	}

	void map_internal(std::list<core_vma_info>& inf, const std::string& name, uint16_t index, void* memory,
		size_t memsize)
	{
		ptrmap[index] = std::make_pair(reinterpret_cast<uint64_t>(memory), static_cast<uint64_t>(memsize));
		create_region(inf, name, 0x101000000 + index * 0x1000000, reinterpret_cast<uint8_t*>(memory),
			memsize, true, true);
	}

	std::list<core_vma_info> get_VMAlist()
	{
		std::list<core_vma_info> ret;
		if(!internal_rom)
			return ret;
		create_region(ret, "WRAM", 0x007E0000, SNES::cpu.wram, 131072, false);
		create_region(ret, "APURAM", 0x00000000, SNES::smp.apuram, 65536, false);
		create_region(ret, "VRAM", 0x00010000, SNES::ppu.vram, 65536, false);
		create_region(ret, "OAM", 0x00020000, SNES::ppu.oam, 544, false);
		create_region(ret, "CGRAM", 0x00021000, SNES::ppu.cgram, 512, false);
		if(SNES::cartridge.has_srtc()) create_region(ret, "RTC", 0x00022000, SNES::srtc.rtc, 20, false);
		if(SNES::cartridge.has_spc7110rtc()) create_region(ret, "RTC", 0x00022000, SNES::spc7110.rtc, 20,
			false);
		if(SNES::cartridge.has_necdsp()) {
			create_region(ret, "DSPRAM", 0x00023000, reinterpret_cast<uint8_t*>(SNES::necdsp.dataRAM),
				4096, false, true);
			create_region(ret, "DSPPROM", 0xF0000000, reinterpret_cast<uint8_t*>(SNES::necdsp.programROM),
				65536, true, true);
			create_region(ret, "DSPDROM", 0xF0010000, reinterpret_cast<uint8_t*>(SNES::necdsp.dataROM),
				4096, true, true);
		}
		create_region(ret, "SRAM", 0x10000000, SNES::cartridge.ram, false);
		create_region(ret, "ROM", 0x80000000, SNES::cartridge.rom, true);
		create_region(ret, "BUS", 0x1000000, 0x1000000, snes_bus_iospace_rw);
		create_region(ret, "PTRTABLE", 0x100000000, 0x100000, ptrtable_iospace_rw);
		map_internal(ret, "CPU_STATE", 0, &SNES::cpu, sizeof(SNES::cpu));
		map_internal(ret, "PPU_STATE", 1, &SNES::ppu, sizeof(SNES::ppu));
		map_internal(ret, "SMP_STATE", 2, &SNES::smp, sizeof(SNES::smp));
		map_internal(ret, "DSP_STATE", 3, &SNES::dsp, sizeof(SNES::dsp));
		if(internal_rom == &type_bsx || internal_rom == &type_bsxslotted) {
			create_region(ret, "BSXFLASH", 0x90000000, SNES::bsxflash.memory, true);
			create_region(ret, "BSX_RAM", 0x20000000, SNES::bsxcartridge.sram, false);
			create_region(ret, "BSX_PRAM", 0x30000000, SNES::bsxcartridge.psram, false);
		}
		if(internal_rom == &type_sufamiturbo) {
			create_region(ret, "SLOTA_ROM", 0x90000000, SNES::sufamiturbo.slotA.rom, true);
			create_region(ret, "SLOTB_ROM", 0xA0000000, SNES::sufamiturbo.slotB.rom, true);
			create_region(ret, "SLOTA_RAM", 0x20000000, SNES::sufamiturbo.slotA.ram, false);
			create_region(ret, "SLOTB_RAM", 0x30000000, SNES::sufamiturbo.slotB.ram, false);
		}
		if(internal_rom == &type_sgb) {
			create_region(ret, "GBROM", 0x90000000, GameBoy::cartridge.romdata,
				GameBoy::cartridge.romsize, true);
			create_region(ret, "GBRAM", 0x20000000, GameBoy::cartridge.ramdata,
				GameBoy::cartridge.ramsize, false);
		}
		return ret;
	}

	std::set<std::string> srams()
	{
		std::set<std::string> r;
		if(!internal_rom)
			return r;
		for(unsigned i = 0; i < SNES::cartridge.nvram.size(); i++) {
			SNES::Cartridge::NonVolatileRAM& s = SNES::cartridge.nvram[i];
			r.insert(sram_name(s.id, s.slot));
		}
		return r;
	}

	const char* hexes = "0123456789ABCDEF";

	void redraw_cover_fbinfo()
	{
		for(size_t i = 0; i < sizeof(cover_fbmem) / sizeof(cover_fbmem[0]); i++)
			cover_fbmem[i] = 0;
		std::string ident = _bsnes_core.core_identifier();
		cover_render_string(cover_fbmem, 0, 0, ident, 0x7FFFF, 0x00000, 512, 448, 2048, 4);
		std::ostringstream name;
		name << "Internal ROM name: ";
		for(unsigned i = 0; i < 21; i++) {
			unsigned busaddr = 0x00FFC0 + i;
			unsigned char ch = SNES::bus.read(busaddr);
			if(ch < 32 || ch > 126)
				name << "<" << hexes[ch / 16] << hexes[ch % 16] << ">";
			else
				name << ch;
		}
		cover_render_string(cover_fbmem, 0, 16, name.str(), 0x7FFFF, 0x00000, 512, 448, 2048, 4);
		unsigned y = 32;
		for(auto i : cover_information()) {
			cover_render_string(cover_fbmem, 0, y, i, 0x7FFFF, 0x00000, 512, 448, 2048, 4);
			y += 16;
		}
	}

	core_core_params _bsnes_core = {
		//Identify core.
		[]() -> std::string {
			return (stringfmt()  << snes_library_id() << " (" << SNES::Info::Profile << " core)").str();
		},
		//Set region.
		[](core_region& region) -> bool {
			if(&region == &region_auto)
				SNES::config.region = SNES::System::Region::Autodetect;
			else if(&region == &region_ntsc)
				SNES::config.region = SNES::System::Region::NTSC;
			else if(&region == &region_pal)
				SNES::config.region = SNES::System::Region::PAL;
			else
				return false;
			return true;
		},
		//Get video rate
		[]() -> std::pair<uint32_t, uint32_t> {
			if(!internal_rom)
				return std::make_pair(60, 1);
			uint32_t div;
			if(SNES::system.region() == SNES::System::Region::PAL)
				div = last_interlace ? DURATION_PAL_FIELD : DURATION_PAL_FRAME;
			else
				div = last_interlace ? DURATION_NTSC_FIELD : DURATION_NTSC_FRAME;
			return std::make_pair(SNES::system.cpu_frequency(), div);
		},
		//Get audio rate
		[]() -> std::pair<uint32_t, uint32_t> {
			if(!internal_rom)
				return std::make_pair(64081, 2);
			return std::make_pair(SNES::system.apu_frequency(), static_cast<uint32_t>(768));
		},
		//Get SNES CPU/APU rate.
		[]() -> std::pair<uint32_t, uint32_t> {
			return std::make_pair(SNES::system.cpu_frequency(), SNES::system.apu_frequency());
		},
		//Store SRAM.
		[]() -> std::map<std::string, std::vector<char>> {
			std::map<std::string, std::vector<char>> out;
			if(!internal_rom)
				return out;
			for(unsigned i = 0; i < SNES::cartridge.nvram.size(); i++) {
				SNES::Cartridge::NonVolatileRAM& r = SNES::cartridge.nvram[i];
				std::string savename = sram_name(r.id, r.slot);
				std::vector<char> x;
				x.resize(r.size);
				memcpy(&x[0], r.data, r.size);
				out[savename] = x;
			}
			return out;
		},
		//Load SRAM.
		[](std::map<std::string, std::vector<char>>& sram) -> void {
			std::set<std::string> used;
			if(!internal_rom) {
				for(auto i : sram)
					messages << "WARNING: SRAM '" << i.first << ": Not found on cartridge."
						<< std::endl;
				return;
			}
			if(sram.empty())
				return;
			for(unsigned i = 0; i < SNES::cartridge.nvram.size(); i++) {
				SNES::Cartridge::NonVolatileRAM& r = SNES::cartridge.nvram[i];
				std::string savename = sram_name(r.id, r.slot);
				if(sram.count(savename)) {
					std::vector<char>& x = sram[savename];
					if(r.size != x.size())
						messages << "WARNING: SRAM '" << savename << "': Loaded " << x.size()
							<< " bytes, but the SRAM is " << r.size << "." << std::endl;
					memcpy(r.data, &x[0], (r.size < x.size()) ? r.size : x.size());
					used.insert(savename);
				} else
					messages << "WARNING: SRAM '" << savename << ": No data." << std::endl;
			}
			for(auto i : sram)
				if(!used.count(i.first))
					messages << "WARNING: SRAM '" << i.first << ": Not found on cartridge."
						<< std::endl;
		},
		//Serialize core.
		[](std::vector<char>& out) -> void {
			if(!internal_rom)
				throw std::runtime_error("No ROM loaded");
			serializer s = SNES::system.serialize();
			out.resize(s.size());
			memcpy(&out[0], s.data(), s.size());
		},
		//Unserialize core.
		[](const char* in, size_t insize) -> void {
			if(!internal_rom)
				throw std::runtime_error("No ROM loaded");
			serializer s(reinterpret_cast<const uint8_t*>(in), insize);
			if(!SNES::system.unserialize(s))
				throw std::runtime_error("SNES core rejected savestate");
			have_saved_this_frame = true;
			do_reset_flag = -1;
		},
		//Get region.
		[]() -> core_region& {
			return (SNES::system.region() == SNES::System::Region::PAL) ? region_pal : region_ntsc;
		},
		//Power the core.
		[]() -> void {
			if(internal_rom) snes_power();
		},
		//Unload the cartridge.
		[]() -> void {
			if(!internal_rom) return;
			snes_term();
			snes_unload_cartridge();
			internal_rom = NULL;
		},
		//Get scale factors.
		[](uint32_t width, uint32_t height) -> std::pair<uint32_t, uint32_t> {
			return std::make_pair((width < 400) ? 2 : 1, (height < 400) ? 2 : 1);
		},
		//Install handler
		[]() -> void {
			basic_init();
			old = SNES::interface;
			SNES::interface = &my_interface_obj;
			SNES::system.init();
		},
		//Uninstall handler
		[]() -> void { SNES::interface = old; },
		//Emulate frame.
		[]() -> void {
			if(!internal_rom)
				return;
			bool was_delay_reset = false;
			int16_t reset = ecore_callbacks->set_input(0, 0, 1, (do_reset_flag >= 0) ? 1 : 0);
			int16_t hreset = 0;
			if(support_hreset)
				hreset = ecore_callbacks->set_input(0, 0, 4, do_hreset_flag ? 1 : 0);
			if(reset) {
				long hi = ecore_callbacks->set_input(0, 0, 2, do_reset_flag / 10000);
				long lo = ecore_callbacks->set_input(0, 0, 3, do_reset_flag % 10000);
				long delay = 10000 * hi + lo;
				if(delay > 0) {
					was_delay_reset = true;
#ifdef BSNES_HAS_DEBUGGER
					messages << "Executing delayed reset... This can take some time!"
						<< std::endl;
					video_refresh_done = false;
					delayreset_cycles_run = 0;
					delayreset_cycles_target = delay;
					forced_hook = true;
					SNES::cpu.step_event = delayreset_fn;
again:
					SNES::system.run();
					if(SNES::scheduler.exit_reason() == SNES::Scheduler::ExitReason::DebuggerEvent
						&& SNES::debugger.break_event ==
						SNES::Debugger::BreakEvent::BreakpointHit) {
						snesdbg_on_break();
						goto again;
					}
					SNES::cpu.step_event = nall::function<bool()>();
					forced_hook = false;
					if(video_refresh_done) {
						//Force the reset here.
						do_reset_flag = -1;
						messages << "SNES reset (forced at " << delayreset_cycles_run << ")"
							<< std::endl;
						if(hreset)
							SNES::system.power();
						else
							SNES::system.reset();
						return;
					}
					if(hreset)
						SNES::system.power();
					else
						SNES::system.reset();
					messages  << "SNES reset (delayed " << delayreset_cycles_run << ")"
						<< std::endl;
#else
					messages << "Delayresets not supported on this bsnes version "
						"(needs v084 or v085)" << std::endl;
					if(hreset)
						SNES::system.power();
					else
						SNES::system.reset();
#endif
				} else if(delay == 0) {
					if(hreset)
						SNES::system.power();
					else
						SNES::system.reset();
					messages << "SNES reset" << std::endl;
				}
			}
			do_reset_flag = -1;

			if(!have_saved_this_frame && save_every_frame && !was_delay_reset)
				SNES::system.runtosave();
#ifdef BSNES_HAS_DEBUGGER
			if(trace_enabled())
				SNES::cpu.step_event = trace_fn;
#endif
again2:
			SNES::system.run();
			if(SNES::scheduler.exit_reason() == SNES::Scheduler::ExitReason::DebuggerEvent &&
				SNES::debugger.break_event == SNES::Debugger::BreakEvent::BreakpointHit) {
				snesdbg_on_break();
				goto again2;
			}
#ifdef BSNES_HAS_DEBUGGER
			SNES::cpu.step_event = nall::function<bool()>();
#endif
			have_saved_this_frame = false;
		},
		//Run to save.
		[]() -> void {
			if(!internal_rom)
				return;
			stepping_into_save = true;
			SNES::system.runtosave();
			have_saved_this_frame = true;
			stepping_into_save = false;
		},
		//Get poll flag.
		[]() -> bool { return SNES::cpu.controller_flag; },
		//Set poll flag.
		[](bool pflag) -> void {
			SNES::cpu.controller_flag = pflag;
		},
		//Request reset.
		[](long delay, bool hard) -> void { do_reset_flag = delay; do_hreset_flag = hard; },
		//Port types.
		port_types,
		//Cover page.
		[]() -> framebuffer_raw& {
			static framebuffer_raw x(cover_fbinfo);
			redraw_cover_fbinfo();
			return x;
		},
		//Short name.
		[]() -> std::string
		{
#ifdef BSNES_IS_COMPAT
			return (stringfmt() << "bsnes" << BSNES_VERSION << "c").str();
#else
			return (stringfmt() << "bsnes" << BSNES_VERSION << "a").str();
#endif
		}
	};

	core_core bsnes_core(_bsnes_core);

	core_type_params _type_snes = {
		"snes", "SNES", 0, BSNES_RESET_LEVEL ,
		[](core_romimage* img, std::map<std::string, std::string>& settings, uint64_t secs,
			uint64_t subsecs) -> int {
			return load_rom<&type_snes>(img, settings, secs, subsecs,
				load_rom_X1<snes_load_cartridge_normal>);
		},
		_controllerconfig, "sfc;smc;swc;fig;ufo;sf2;gd3;gd7;dx2;mgd;mgh", NULL, snes_regions, snes_images,
		&bsnes_settings, &bsnes_core, bsnes_get_bus_map, get_VMAlist, srams
	};
	core_type_params _type_bsx = {
		"bsx", "BS-X (non-slotted)", 1, BSNES_RESET_LEVEL , 
		[](core_romimage* img, std::map<std::string, std::string>& settings, uint64_t secs,
			uint64_t subsecs) -> int {
			return load_rom<&type_bsx>(img, settings, secs, subsecs,
				load_rom_X2<snes_load_cartridge_bsx>);
		},
		_controllerconfig, "bs", "bsx.sfc", bsx_regions, bsx_images, &bsnes_settings, &bsnes_core,
		bsnes_get_bus_map, get_VMAlist, srams
	};
	core_type_params _type_bsxslotted = {
		"bsxslotted", "BS-X (slotted)", 2, BSNES_RESET_LEVEL ,
		[](core_romimage* img, std::map<std::string, std::string>& settings,
			uint64_t secs, uint64_t subsecs) -> int {
			return load_rom<&type_bsxslotted>(img, settings, secs, subsecs,
				load_rom_X2<snes_load_cartridge_bsx_slotted>);
		},
		_controllerconfig, "bss", "bsxslotted.sfc", bsxs_regions, bsxs_images, &bsnes_settings, &bsnes_core,
		bsnes_get_bus_map, get_VMAlist, srams
	};
	core_type_params _type_sufamiturbo = {
		"sufamiturbo", "Sufami Turbo", 3, BSNES_RESET_LEVEL ,
		[](core_romimage* img, std::map<std::string, std::string>& settings, uint64_t secs,
			uint64_t subsecs) -> int {
			return load_rom<&type_sufamiturbo>(img, settings, secs, subsecs,
				load_rom_X3<snes_load_cartridge_sufami_turbo>);
		},
		_controllerconfig, "st", "sufamiturbo.sfc", sufamiturbo_regions, sufamiturbo_images, &bsnes_settings,
		&bsnes_core, bsnes_get_bus_map, get_VMAlist, srams
	};
	core_type_params _type_sgb = {
		"sgb", "Super Game Boy", 4, BSNES_RESET_LEVEL ,
		[](core_romimage* img, std::map<std::string, std::string>& settings, uint64_t secs,
			uint64_t subsecs) -> int
		{
			return load_rom<&type_sgb>(img, settings, secs, subsecs,
				load_rom_X2<snes_load_cartridge_super_game_boy>);
		},
		_controllerconfig, "gb;dmg;sgb", "sgb.sfc", sgb_regions, sgb_images, &bsnes_settings, &bsnes_core,
		bsnes_get_bus_map, get_VMAlist, srams
	};

	core_type type_snes(_type_snes);
	core_type type_bsx(_type_bsx);
	core_type type_bsxslotted(_type_bsxslotted);
	core_type type_sufamiturbo(_type_sufamiturbo);
	core_type type_sgb(_type_sgb);

	function_ptr_command<arg_filename> dump_core(lsnes_cmd, "dump-core", "No description available",
		"No description available\n",
		[](arg_filename args) throw(std::bad_alloc, std::runtime_error) {
			std::vector<char> out;
			_bsnes_core.serialize(out);
			std::ofstream x(args, std::ios_base::out | std::ios_base::binary);
			x.write(&out[0], out.size());
		});

	function_ptr_luafun lua_memory_readreg(LS, "memory.getregister", [](lua_state& L, const std::string& fname) ->
		int {
		std::string r = L.get_string(1, fname.c_str());
		auto& c = SNES::cpu.regs;
		auto& c2 = SNES::cpu;
		if(r == "pbpc")		L.pushnumber((unsigned)c.pc);
		else if(r == "pb")	L.pushnumber((unsigned)c.pc >> 16);
		else if(r == "pc")	L.pushnumber((unsigned)c.pc & 0xFFFF);
		else if(r == "r0")	L.pushnumber((unsigned)c.r[0]);
		else if(r == "r1")	L.pushnumber((unsigned)c.r[1]);
		else if(r == "r2")	L.pushnumber((unsigned)c.r[2]);
		else if(r == "r3")	L.pushnumber((unsigned)c.r[3]);
		else if(r == "r4")	L.pushnumber((unsigned)c.r[4]);
		else if(r == "r5")	L.pushnumber((unsigned)c.r[5]);
		else if(r == "a")	L.pushnumber((unsigned)c.a);
		else if(r == "x")	L.pushnumber((unsigned)c.x);
		else if(r == "y")	L.pushnumber((unsigned)c.y);
		else if(r == "z")	L.pushnumber((unsigned)c.z);
		else if(r == "s")	L.pushnumber((unsigned)c.s);
		else if(r == "d")	L.pushnumber((unsigned)c.d);
		else if(r == "db")	L.pushnumber((unsigned)c.db);
		else if(r == "p")	L.pushnumber((unsigned)c.p);
		else if(r == "p_n")	L.pushboolean(c.p.n);
		else if(r == "p_v")	L.pushboolean(c.p.v);
		else if(r == "p_m")	L.pushboolean(c.p.m);
		else if(r == "p_x")	L.pushboolean(c.p.x);
		else if(r == "p_d")	L.pushboolean(c.p.d);
		else if(r == "p_i")	L.pushboolean(c.p.i);
		else if(r == "p_z")	L.pushboolean(c.p.z);
		else if(r == "p_c")	L.pushboolean(c.p.c);
		else if(r == "e")	L.pushboolean(c.e);
		else if(r == "irq")	L.pushboolean(c.irq);
		else if(r == "wai")	L.pushboolean(c.wai);
		else if(r == "mdr")	L.pushnumber((unsigned)c.mdr);
		else if(r == "vector")	L.pushnumber((unsigned)c.vector);
		else if(r == "aa")	L.pushnumber((unsigned)c2.aa);
		else if(r == "rd")	L.pushnumber((unsigned)c2.rd);
		else if(r == "sp")	L.pushnumber((unsigned)c2.sp);
		else if(r == "dp")	L.pushnumber((unsigned)c2.dp);
		else			L.pushnil();
		return 1;
	});

#ifdef BSNES_HAS_DEBUGGER
	char snes_debug_cb_keys[SNES::Debugger::Breakpoints];
	char snes_debug_cb_trace;

	void snesdbg_execute_callback(char& cb, signed r)
	{
		LS.pushlightuserdata(&cb);
		LS.gettable(LUA_REGISTRYINDEX);
		LS.pushnumber(r);
		if(LS.type(-2) == LUA_TFUNCTION) {
			int s = LS.pcall(1, 0, 0);
			if(s)
				LS.pop(1);
		} else {
			messages << "Can't execute debug callback" << std::endl;
			LS.pop(2);
		}
		if(lua_requests_repaint) {
			lua_requests_repaint = false;
			lsnes_cmd.invoke("repaint");
		}
	}

	void snesdbg_on_break()
	{
		signed r = SNES::debugger.breakpoint_hit;
		snesdbg_execute_callback(snes_debug_cb_keys[r], r);
	}

	void snesdbg_on_trace()
	{
		snesdbg_execute_callback(snes_debug_cb_trace, -1);
	}

	void snesdbg_set_callback(lua_state& L, char& cb)
	{
		L.pushlightuserdata(&cb);
		L.pushvalue(-2);
		L.settable(LUA_REGISTRYINDEX);
	}

	bool snesdbg_get_bp_enabled(lua_state& L)
	{
		bool r;
		L.getfield(-1, "addr");
		r = (L.type(-1) == LUA_TNUMBER);
		L.pop(1);
		return r;
	}

	uint32_t snesdbg_get_bp_addr(lua_state& L)
	{
		uint32_t r = 0;
		L.getfield(-1, "addr");
		if(L.type(-1) == LUA_TNUMBER)
			r = static_cast<uint32_t>(L.tonumber(-1));
		L.pop(1);
		return r;
	}

	uint32_t snesdbg_get_bp_data(lua_state& L)
	{
		signed r = -1;
		L.getfield(-1, "data");
		if(L.type(-1) == LUA_TNUMBER)
			r = static_cast<signed>(L.tonumber(-1));
		L.pop(1);
		return r;
	}

	SNES::Debugger::Breakpoint::Mode snesdbg_get_bp_mode(lua_state& L)
	{
		SNES::Debugger::Breakpoint::Mode r = SNES::Debugger::Breakpoint::Mode::Exec;
		L.getfield(-1, "mode");
		if(L.type(-1) == LUA_TSTRING && !strcmp(L.tostring(-1), "e"))
			r = SNES::Debugger::Breakpoint::Mode::Exec;
		if(L.type(-1) == LUA_TSTRING && !strcmp(L.tostring(-1), "x"))
			r = SNES::Debugger::Breakpoint::Mode::Exec;
		if(L.type(-1) == LUA_TSTRING && !strcmp(L.tostring(-1), "exec"))
			r = SNES::Debugger::Breakpoint::Mode::Exec;
		if(L.type(-1) == LUA_TSTRING && !strcmp(L.tostring(-1), "r"))
			r = SNES::Debugger::Breakpoint::Mode::Read;
		if(L.type(-1) == LUA_TSTRING && !strcmp(L.tostring(-1), "read"))
			r = SNES::Debugger::Breakpoint::Mode::Read;
		if(L.type(-1) == LUA_TSTRING && !strcmp(L.tostring(-1), "w"))
			r = SNES::Debugger::Breakpoint::Mode::Write;
		if(L.type(-1) == LUA_TSTRING && !strcmp(L.tostring(-1), "write"))
			r = SNES::Debugger::Breakpoint::Mode::Write;
		L.pop(1);
		return r;
	}

	SNES::Debugger::Breakpoint::Source snesdbg_get_bp_source(lua_state& L)
	{
		SNES::Debugger::Breakpoint::Source r = SNES::Debugger::Breakpoint::Source::CPUBus;
		L.getfield(-1, "source");
		if(L.type(-1) == LUA_TSTRING && !strcmp(L.tostring(-1), "cpubus"))
			r = SNES::Debugger::Breakpoint::Source::CPUBus;
		if(L.type(-1) == LUA_TSTRING && !strcmp(L.tostring(-1), "apuram"))
			r = SNES::Debugger::Breakpoint::Source::APURAM;
		if(L.type(-1) == LUA_TSTRING && !strcmp(L.tostring(-1), "vram"))
			r = SNES::Debugger::Breakpoint::Source::VRAM;
		if(L.type(-1) == LUA_TSTRING && !strcmp(L.tostring(-1), "oam"))
			r = SNES::Debugger::Breakpoint::Source::OAM;
		if(L.type(-1) == LUA_TSTRING && !strcmp(L.tostring(-1), "cgram"))
			r = SNES::Debugger::Breakpoint::Source::CGRAM;
		L.pop(1);
		return r;
	}

	void snesdbg_get_bp_callback(lua_state& L)
	{
		L.getfield(-1, "callback");
	}

	function_ptr_luafun lua_memory_setdebug(LS, "memory.setdebug", [](lua_state& L, const std::string& fname) ->
		int {
		unsigned r = L.get_numeric_argument<unsigned>(1, fname.c_str());
		if(r >= SNES::Debugger::Breakpoints) {
			L.pushstring("Bad breakpoint number");
			L.error();
			return 0;
		}
		if(L.type(2) == LUA_TNIL) {
			//Clear breakpoint.
			SNES::debugger.breakpoint[r].enabled = false;
			return 0;
		} else if(L.type(2) == LUA_TTABLE) {
			L.pushvalue(2);
			auto& x = SNES::debugger.breakpoint[r];
			x.enabled = snesdbg_get_bp_enabled(L);
			x.addr = snesdbg_get_bp_addr(L);
			x.data = snesdbg_get_bp_data(L);
			x.mode = snesdbg_get_bp_mode(L);
			x.source = snesdbg_get_bp_source(L);
			snesdbg_get_bp_callback(L);
			snesdbg_set_callback(L, snes_debug_cb_keys[r]);
			L.pop(2);
			return 0;
		} else {
			L.pushstring("Expected argument 2 to memory.setdebug to be nil or table");
			L.error();
			return 0;
		}
	});

	function_ptr_luafun lua_memory_setstep(LS, "memory.setstep", [](lua_state& L, const std::string& fname) ->
		int {
		uint64_t r = L.get_numeric_argument<uint64_t>(1, fname.c_str());
		L.pushvalue(2);
		snesdbg_set_callback(L, snes_debug_cb_trace);
		trace_counter = r;
		update_trace_hook_state();
		L.pop(1);
		return 0;
	});

	void snesdbg_settrace(std::string r)
	{
		if(trace_output_enable)
			messages << "------- End of trace -----" << std::endl;
		trace_output.close();
		trace_output_enable = false;
		if(r != "") {
			trace_output.close();
			trace_output.open(r);
			if(trace_output) {
				trace_output_enable = true;
				messages << "------- Start of trace -----" << std::endl;
			} else
				messages << "Can't open " << r << std::endl;
		}
		update_trace_hook_state();
	}

	function_ptr_luafun lua_memory_settrace(LS, "memory.settrace", [](lua_state& L, const std::string& fname) ->
		int {
		std::string r = L.get_string(1, fname.c_str());
		snesdbg_settrace(r);
	});

	function_ptr_command<const std::string&> start_trace(lsnes_cmd, "set-trace", "No description available",
		"No description available\n",
		[](const std::string& r) throw(std::bad_alloc, std::runtime_error) {
			snesdbg_settrace(r);
		});

#else
	void snesdbg_on_break() {}
	void snesdbg_on_trace() {}
#endif
}