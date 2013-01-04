#ifndef _emucore__hpp__included__
#define _emucore__hpp__included__

#include <map>
#include <list>
#include <cstdlib>
#include <string>
#include <set>
#include <vector>
#include "library/framebuffer.hpp"
#include "library/controller-data.hpp"
#include "core/romtype.hpp"


//Get the CPU rate.
uint32_t get_snes_cpu_rate();
//Get the APU rate.
uint32_t get_snes_apu_rate();
//Get the core identifier.
std::string get_core_identifier();
//Needs analog action?
bool get_core_need_analog();
//Get the default controller type for specified port.
std::string get_core_default_port(unsigned port);
//Do basic core initialization (to get it to stable state).
void do_basic_core_init();
//Get set of SRAMs.
std::set<std::string> get_sram_set();
//Get region.
core_region& core_get_region();
//Get the current video rate.
std::pair<uint32_t, uint32_t> get_video_rate();
//Get the current audio rate.
std::pair<uint32_t, uint32_t> get_audio_rate();
//Set preload settings.
void set_preload_settings();
//Set the region.
bool core_set_region(core_region& region);
//Power the core.
void core_power();
//Unload the cartridge.
void core_unload_cartridge();
//Save SRAM set.
std::map<std::string, std::vector<char>> save_sram() throw(std::bad_alloc);
//Load SRAM set.
void load_sram(std::map<std::string, std::vector<char>>& sram) throw(std::bad_alloc);
//Serialize state.
void core_serialize(std::vector<char>& out);
//Unserialize state.
void core_unserialize(const char* in, size_t insize);
//Install handler.
void core_install_handler();
//Uninstall handler.
void core_uninstall_handler();
//Emulate a frame.
void core_emulate_frame();
//Run to point save.
void core_runtosave();
//Get the scale factors.
std::pair<uint32_t, uint32_t> get_scale_factors(uint32_t width, uint32_t height);
//Get bus map range.
std::pair<uint64_t, uint64_t> core_get_bus_map();
//Get poll flag (set to 1 on each real poll, except if 2.
unsigned core_get_poll_flag();
//Set poll flag (set to 1 on each real poll, except if 2.
void core_set_poll_flag(unsigned pflag);
//Request reset on next frame.
void core_request_reset(long delay);
//The port type group.
extern port_type_group core_portgroup;
//Number of user ports.
extern unsigned core_userports;
//Core supports resets.
extern const bool core_supports_reset;
//Core supports delayed resets.
extern const bool core_supports_dreset;

//Callbacks.
struct emucore_callbacks
{
public:
	virtual ~emucore_callbacks() throw();
	//Get the input for specified control.
	virtual int16_t get_input(unsigned port, unsigned index, unsigned control) = 0;
	//Set the input for specified control (used for system controls, only works in readwrite mode).
	//Returns the actual value of control (may differ if in readonly mode).
	virtual int16_t set_input(unsigned port, unsigned index, unsigned control, int16_t value) = 0;
	//Do timer tick.
	virtual void timer_tick(uint32_t increment, uint32_t per_second) = 0;
	//Get the firmware path.
	virtual std::string get_firmware_path() = 0;
	//Get the base path.
	virtual std::string get_base_path() = 0;
	//Get the current time.
	virtual time_t get_time() = 0;
	//Get random seed.
	virtual time_t get_randomseed() = 0;
	//Output frame.
	virtual void output_frame(framebuffer_raw& screen, uint32_t fps_n, uint32_t fps_d) = 0;
};

extern struct emucore_callbacks* ecore_callbacks;

//A VMA.
struct vma_info
{
	std::string name;
	uint64_t base;
	uint64_t size;
	void* backing_ram;
	bool readonly;
	bool native_endian;
	uint8_t (*iospace_rw)(uint64_t offset, uint8_t data, bool write);
};

std::list<vma_info> get_vma_list();

#endif
